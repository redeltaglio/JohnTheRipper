/*
 * This software is Copyright (c) 2018 Denis Burykin
 * [denis_burykin yahoo com], [denis-burykin2014 yandex ru]
 * and it is hereby released to the general public under the following terms:
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted.
 *
 * This file is part of John the Ripper password cracker,
 * Copyright (c) 1996-2001,2008,2010-2013,2015 by Solar Designer
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted.
 *
 * There's ABSOLUTELY NO WARRANTY, express or implied.
 */
#include <stdio.h>
#include <string.h>

#include "arch.h"
#include "common.h"
#include "formats.h"
#include "memory.h"
#include "config.h"
#include "options.h"

#include "ztex/device_bitstream.h"
#include "ztex/device_format.h"
#include "ztex/task.h"
#include "ztex/pkt_comm/cmp_config.h"


#define FORMAT_LABEL		"sha512crypt-ztex"
#define ALGORITHM_NAME		"sha512crypt ZTEX"
/*
#define FORMAT_NAME			"crypt(3) $6$"
#define FORMAT_TAG			"$6$"
#define FORMAT_TAG_LEN		(sizeof(FORMAT_TAG)-1)

#define BENCHMARK_COMMENT		""
#define BENCHMARK_LENGTH		0
*/
#define	SALT_LENGTH			16
#define CIPHERTEXT_LENGTH	86

// Data structures differ from ones from CPU implementation:
// - Only partial hash in the database (BINARY_SIZE=4)
// - The hardware allows candidate length no more than 64
//
#define PLAINTEXT_LENGTH	64
#define BINARY_SIZE			4
#define BINARY_ALIGN		4
#define SALT_SIZE			sizeof(sha512crypt_salt_t)
#define SALT_ALIGN			4
/*
#define ROUNDS_PREFIX          "rounds="
#define ROUNDS_DEFAULT         5000
#define ROUNDS_MIN             1
#define ROUNDS_MAX             999999999
*/

//
// Using valid(), get_binary(), some define's
// possible TODO: unify get_salt()
//
#include "sha512crypt_common.h"


static struct device_bitstream bitstream = {
	// bitstream ID (check is performed by querying operating bitstream)
	0x512c,
	"$JOHN/ztex/ztex115y_sha512crypt.bit",
	// parameters for high-speed packet communication (pkt_comm)
	{ 2, 6144, 8190 },
	// computing performance estimation (in candidates per interval)
	// (keys * mask_num_cand)/crypt_all_interval per jtr_device.
	1,			// set by init()
	128*1024,	// Absolute max. keys/crypt_all_interval for all devices.
	512,		// Max. number of entries in onboard comparator.
	160,		// Min. number of keys for effective device utilization
	1, { 135 },	// Programmable clocks
	"sha512crypt"	// label for configuration file
};


typedef struct {
	unsigned int len;
	unsigned int rounds;
	unsigned char salt[16];
} sha512crypt_salt_t;


static struct fmt_tests tests[] = {

	{"$6$LKO/Ute40T3FNF95$6S/6T2YuOIHY0N3XpLKABJ3soYcXD9mB7uVbtEZDj"
		"/LNscVhZoZ9DEH.sBciDrMsHOWOoASbNLTypH/5X26gN0", "U*U*U*U*"},
	{"$6$LKO/Ute40T3FNF95$wK80cNqkiAUzFuVGxW6eFe8J.fSVI65MD5yEm8EjY"
		"MaJuDrhwe5XXpHDJpwF/kY.afsUs1LlgQAaOapVNbggZ1", "U*U***U"},
	{"$6$LKO/Ute40T3FNF95$YS81pp1uhOHTgKLhSMtQCr2cDiUiN03Ud3gyD4ame"
		"viK1Zqz.w3oXsMgO6LrqmIEcG3hiqaUqHi/WEE2zrZqa/", "U*U***U*"},

	{"$6$saltstring$svn8UoSVapNtMuq1ukKS4tPQd8iKwSMHWjl/O817G3uBnIFNjn"
		"QJuesI68u4OTLiBFdcbYEdFCoEOfaS35inz1", "Hello world!"},
	{"$6$saltstring$thYL/sWXcrctga2PlQjFsvA3l.lG3RPHgk7ADjxkgKZoo0UsYV"
		"X9gsxryuhCl6RTDsE.0F8aDMaHBkUzUGqd11", "Hello........."},

	{"$6$rounds=2000$saltSALTsaltSALT$TQ.57CbfxQTWKO8b1rkPVic99auVj.Je"
		"fhUjAB9YtTXRGiZH.NmgSS04t1WaSLhkTrGxt.Aj61KS0oq46Jpal1",
		"salt_len=16, key_len=64, contains 8-bit chars ("
		"\xc0\xc1\xc2\xc3\xc4) .........z"},
	{"$6$rounds=5019$salt_length13$3IEhRbEWfDy5kOvq5ikjCEaAt0RnNywOTw8"
		"bk6z7Vc9wvGn29kj7xxCmn.ZGMy1/H6mL7PK.Si1w5XNs.y79t/",
		"key_len_9"},
	{"$6$rounds=1008$salt_length_16..$SEjF0rHbMWUP/cjkWe53PnExTtkYusJS"
		"s.Dk2tc.fHnMt6vICRCP3gUSpvHJ5tFy8kjzgRjQQQ6z6aLlHJ04U0",
		"key_len_is16...."},
	{"$6$rounds=1071$YbN9RAZ1h3IQXaTD$6naBlXjVO/qqPMdDemvKUGhgvX91zRjd"
		"LwlFkWFcVVujnpri0EfFrkYaS9lraqWCxsLTODlmNRPJjPWWJngFn1",
		"keyLen7"},

	{"$6$rounds=1000$salt_length_is16$qwDPvfL2spXBiDQOmqNOLJn6JNTPbQg"
		"UYa2RtxIKDDAR9m3Cso.AtkXb2iksEP7PLEBuPVD8wIMwGpFPEMieH/",
		"sha512crypt-ztex test #1 (salt=16,key_len=48) .."},
	{"$6$rounds=1000$salt_lengthis15$EeHyq27P/oA1UysdPCQ2p/HkqLadTUfS"
		"jYFSNOPfspe8IEPfGPbew91kDWLfEda3dUzc7vfIsc8BpgXlZcOQs.",
		"sha512crypt-ztex test #2 (salt=15,key_len=48) .."},
	{"$6$rounds=1001$1$lZlt6XW1lySUVFdntw7D/iZNZ7yOKpFVkIBnZyj2Mt85lr"
		"V34Hjk6tr1J8FAw2N5gk5ceJfv3wG6fzQTWpT/h0",
		"sha512crypt-ztex test #3 (salt_len=1, A0>200) .z"},
	{"$6$rounds=1013$1234567890aBcdef$StfD5z1XXAuXkuUWkV3EMaG/y1Gp46u"
		"keHLcCoXl7ANC1.h4AeP.RJW5Mui4Ngh50cVMe1AoL2pr0gCo71HXo1",
		"sha512crypt-ztex test #4 (salt_len=16, key_len=56) ....."},
	{"$6$rounds=1021$1234567890aBcde$QxJixRbv7Pg4Wdgz7HNwjnGR/QYwMoRe"
		"d3lHeq8lJKH6UShkNcSmbXwoqYKQkWvdhp9xcikUXD5AKxNwHgZHE.",
		"sha512crypt-ztex test #5 (salt_len=15, key_len=56) ....."},
	{NULL}
};


static void *get_salt(char *ciphertext)
{
	static sha512crypt_salt_t salt;
	int len;

	memset(&salt, 0, sizeof(salt));
	salt.rounds = ROUNDS_DEFAULT;
	ciphertext += FORMAT_TAG_LEN;
	if (!strncmp(ciphertext, ROUNDS_PREFIX,
			sizeof(ROUNDS_PREFIX) - 1)) {
		const char *num = ciphertext + sizeof(ROUNDS_PREFIX) - 1;
		char *endp;
		unsigned long int srounds = strtoul(num, &endp, 10);
		if (*endp == '$') {
			ciphertext = endp + 1;
			srounds = srounds < ROUNDS_MIN ?
				ROUNDS_MIN : srounds;
			salt.rounds = srounds > ROUNDS_MAX ?
				ROUNDS_MAX : srounds;
		}
	}

	for (len = 0; ciphertext[len] != '$' && len < 16; len++);

	memcpy(salt.salt, ciphertext, len);
	salt.len = len;
	return &salt;
}


static unsigned int iteration_count(void *salt)
{
	sha512crypt_salt_t *sha512crypt_salt;

	sha512crypt_salt = salt;
	return sha512crypt_salt->rounds;
}


int target_rounds;

static void init(struct fmt_main *fmt_main)
{
//printf("verbosity (init):%d\n", options.verbosity);
	// It uses performance estimation (bitstream.candidates_per_crypt)
	// to calculate keys_per_crypt. Performance depends on count of rounds.
	// Count is not available in init() and can change at runtime.
	// In crypt_all(), count is available but keys_per_crypt can't change.
	//
	// It gets TargetRounds from john.conf and adjust
	// bitstream.candidates_per_crypt.

	// Starting from TARGET_ROUNDS_1KPC, it sets keys_per_crypt
	// equal to bitstream.min_keys
	//
	const int TARGET_ROUNDS_1KPC = 256*1024;

	target_rounds = cfg_get_int("ZTEX:", bitstream.label,
			"TargetRounds");
	if (target_rounds <= 0)
		target_rounds = 5000;

	if (target_rounds < 1000)
		fprintf(stderr, "Warning: invalid TargetRounds=%d in john.conf."
			" Valid values are 1000-999999999\n", target_rounds);

	if (target_rounds < 1000)
		target_rounds = 1000;

	if (target_rounds >= TARGET_ROUNDS_1KPC)
		bitstream.candidates_per_crypt = bitstream.min_keys;
	else
		bitstream.candidates_per_crypt = bitstream.min_keys
			* (2 * TARGET_ROUNDS_1KPC / target_rounds);

	//fprintf(stderr, "bitstream.candidates_per_crypt=%d\n",
	//		bitstream.candidates_per_crypt);

	device_format_init(fmt_main, &bitstream, options.acc_devices);//,
		//options.verbosity);
}


/*
 * There's an existing CPU implementation. It stores salt in the database.
 * 1) Salt data structure db_salt->salt is specific to algorithm
 * 2) db_salt->salt depends on host system (e.g. different on 32 and 64-bit)
 * 3) This salt internally has tunable costs and is used to
 * populate db_salt->cost[FMT_TUNABLE_COSTS].
 *
 * Salt is sent to devices in some uniform way in CMP_CONFIG packet:
 * - first it goes binary salt in network byte order
 * - then it sends 4-byte tunable cost(s) if any
 * - then it sends 2 bytes - number of hashes
 * - then partial hashes ("binaries") sorted in ascending order.
 * (sha512crypt-ztex accepts up to 512 32-bit hashes, order doesn't matter).
 */


extern volatile int bench_running;

int warning_target_rounds = 0;

static int crypt_all(int *pcount, struct db_salt *salt)
{
	int curr_rounds = salt->cost[0];

	int result;
	sha512crypt_salt_t *sha512crypt_salt = salt->salt;
	unsigned char salt_buf[18]; // salt to send to device

	if (!warning_target_rounds && !bench_running
			&& (curr_rounds >= target_rounds * 4
			|| curr_rounds <= target_rounds / 4)
	) {
		fprintf(stderr, "Warning: TargetRounds=%d, processing"
			" hash with rounds=%d, expecting suboptimal performance or"
			" timeout, consider to adjust TargetRounds in john.conf\n",
			target_rounds, curr_rounds);
		warning_target_rounds = 1;
	}

	// 1 byte unused, 1 byte salt_len, 16 bytes salt in network byte order
	salt_buf[0] = 0;
	salt_buf[1] = sha512crypt_salt->len;
	memcpy(salt_buf + 2, sha512crypt_salt->salt, 16);

	cmp_config_new(salt, salt_buf, 18);

	result = device_format_crypt_all(pcount, salt);
	return result;
}


extern struct task_list *task_list;

static int cmp_exact(char *source, int index)
{
	struct task_result *result = task_result_by_index(task_list, index);

	//fprintf(stderr,"cmp_exact start %d, key %s\n",index,result->key);

	return !memcmp(result->binary, get_binary(source), 64);
}


struct fmt_main fmt_ztex_sha512crypt = {
	{
		FORMAT_LABEL,
		FORMAT_NAME,
		ALGORITHM_NAME,
		BENCHMARK_COMMENT,
		BENCHMARK_LENGTH,
		0,
		PLAINTEXT_LENGTH,
		BINARY_SIZE,
		BINARY_ALIGN,
		SALT_SIZE,
		SALT_ALIGN,
		1, //MIN_KEYS_PER_CRYPT,
		1, //MAX_KEYS_PER_CRYPT,
		FMT_CASE | FMT_8_BIT | FMT_TRUNC | FMT_MASK,
		{
			"iteration count",
		},
		{
			FORMAT_TAG
		},
		tests
	}, {
		init,
		device_format_done,
		device_format_reset,
		fmt_default_prepare,
		valid,
		fmt_default_split,
		get_binary,
		get_salt,
		{
			iteration_count,
		},
		fmt_default_source,
		{
			fmt_default_binary_hash_0,
			fmt_default_binary_hash_1,
			fmt_default_binary_hash_2,
			fmt_default_binary_hash_3,
			fmt_default_binary_hash_4,
			fmt_default_binary_hash_5,
			fmt_default_binary_hash_6
		},
		fmt_default_salt_hash,
		NULL,
		device_format_set_salt,
		device_format_set_key,
		device_format_get_key,
		fmt_default_clear_keys,
		crypt_all,
		{
			device_format_get_hash_0,
			device_format_get_hash_1,
			device_format_get_hash_2,
			device_format_get_hash_3,
			device_format_get_hash_4,
			device_format_get_hash_5,
			device_format_get_hash_6
		},
		device_format_cmp_all,
		device_format_cmp_one,
		cmp_exact
	}
};
