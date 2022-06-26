/*
 * test-alsa-ctl.c
 * Copyright (c) 2016-2022 Arkadiusz Bokowy
 *
 * This file is a part of bluez-alsa.
 *
 * This project is licensed under the terms of the MIT license.
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <libgen.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>

#include <check.h>
#include <alsa/asoundlib.h>

#include "shared/defs.h"

#include "inc/preload.inc"
#include "inc/server.inc"

static int snd_ctl_open_bluealsa(
		snd_ctl_t **ctlp,
		const char *service,
		const char *extra_config,
		int mode) {

	char buffer[256];
	snd_config_t *conf = NULL;
	snd_input_t *input = NULL;
	int err;

	sprintf(buffer,
			"ctl.bluealsa {\n"
			"  type bluealsa\n"
			"  service \"org.bluealsa.%s\"\n"
			"  battery true\n"
			"  %s\n"
			"}\n", service, extra_config);

	if ((err = snd_config_top(&conf)) < 0)
		goto fail;
	if ((err = snd_input_buffer_open(&input, buffer, strlen(buffer))) != 0)
		goto fail;
	if ((err = snd_config_load(conf, input)) != 0)
		goto fail;
	err = snd_ctl_open_lconf(ctlp, "bluealsa", mode, conf);

fail:
	if (conf != NULL)
		snd_config_delete(conf);
	if (input != NULL)
		snd_input_close(input);
	return err;
}

static int test_ctl_open(pid_t *pid, snd_ctl_t **ctl, int mode) {
	const char *service = "test";
	if ((*pid = spawn_bluealsa_server(service, 1000, true, 0, true, true, true, false)) == -1)
		return -1;
	return snd_ctl_open_bluealsa(ctl, service, "", mode);
}

static int test_pcm_close(pid_t pid, snd_ctl_t *ctl) {
	int rv = 0;
	if (ctl != NULL)
		rv = snd_ctl_close(ctl);
	if (pid != -1) {
		kill(pid, SIGTERM);
		waitpid(pid, NULL, 0);
	}
	return rv;
}

START_TEST(test_controls) {
	fprintf(stderr, "\nSTART TEST: %s (%s:%d)\n", __func__, __FILE__, __LINE__);

	snd_ctl_t *ctl = NULL;
	pid_t pid = -1;

	ck_assert_int_eq(test_ctl_open(&pid, &ctl, 0), 0);

	snd_ctl_elem_list_t *elems;
	snd_ctl_elem_list_alloca(&elems);

	ck_assert_int_eq(snd_ctl_elem_list(ctl, elems), 0);
	ck_assert_int_eq(snd_ctl_elem_list_get_count(elems), 13);
	ck_assert_int_eq(snd_ctl_elem_list_alloc_space(elems, 13), 0);
	ck_assert_int_eq(snd_ctl_elem_list(ctl, elems), 0);

	ck_assert_int_eq(snd_ctl_elem_list_get_used(elems), 13);

	ck_assert_str_eq(snd_ctl_elem_list_get_name(elems, 0), "12:34:56:78:9A:BC - A2DP Capture Switch");
	ck_assert_str_eq(snd_ctl_elem_list_get_name(elems, 1), "12:34:56:78:9A:BC - A2DP Capture Volume");
	ck_assert_str_eq(snd_ctl_elem_list_get_name(elems, 2), "12:34:56:78:9A:BC - A2DP Playback Switch");
	ck_assert_str_eq(snd_ctl_elem_list_get_name(elems, 3), "12:34:56:78:9A:BC - A2DP Playback Volume");

	ck_assert_str_eq(snd_ctl_elem_list_get_name(elems, 4), "12:34:56:78:9A:BC - SCO Capture Switch");
	ck_assert_str_eq(snd_ctl_elem_list_get_name(elems, 5), "12:34:56:78:9A:BC - SCO Capture Volume");
	ck_assert_str_eq(snd_ctl_elem_list_get_name(elems, 6), "12:34:56:78:9A:BC - SCO Playback Switch");
	ck_assert_str_eq(snd_ctl_elem_list_get_name(elems, 7), "12:34:56:78:9A:BC - SCO Playback Volume");
	ck_assert_str_eq(snd_ctl_elem_list_get_name(elems, 8), "12:34:56:78:9A:BC | Battery Playback Volume");

	ck_assert_str_eq(snd_ctl_elem_list_get_name(elems, 9), "23:45:67:89:AB:CD - A2DP Capture Switch");
	ck_assert_str_eq(snd_ctl_elem_list_get_name(elems, 10), "23:45:67:89:AB:CD - A2DP Capture Volume");
	ck_assert_str_eq(snd_ctl_elem_list_get_name(elems, 11), "23:45:67:89:AB:CD - A2DP Playback Switch");
	ck_assert_str_eq(snd_ctl_elem_list_get_name(elems, 12), "23:45:67:89:AB:CD - A2DP Playback Volume");

	ck_assert_int_eq(test_pcm_close(pid, ctl), 0);

} END_TEST

START_TEST(test_mute_and_volume) {
	fprintf(stderr, "\nSTART TEST: %s (%s:%d)\n", __func__, __FILE__, __LINE__);

	snd_ctl_t *ctl = NULL;
	pid_t pid = -1;

	ck_assert_int_eq(test_ctl_open(&pid, &ctl, 0), 0);

	snd_ctl_elem_value_t *elem_switch;
	snd_ctl_elem_value_alloca(&elem_switch);
	/* 23:45:67:89:AB:CD - A2DP Playback Switch */
	snd_ctl_elem_value_set_numid(elem_switch, 12);

	ck_assert_int_eq(snd_ctl_elem_read(ctl, elem_switch), 0);
	ck_assert_int_eq(snd_ctl_elem_value_get_boolean(elem_switch, 0), 1);
	ck_assert_int_eq(snd_ctl_elem_value_get_boolean(elem_switch, 1), 1);

	snd_ctl_elem_value_set_boolean(elem_switch, 0, 0);
	snd_ctl_elem_value_set_boolean(elem_switch, 1, 0);
	ck_assert_int_gt(snd_ctl_elem_write(ctl, elem_switch), 0);

	snd_ctl_elem_value_t *elem_volume;
	snd_ctl_elem_value_alloca(&elem_volume);
	/* 23:45:67:89:AB:CD - A2DP Playback Volume */
	snd_ctl_elem_value_set_numid(elem_volume, 13);

	ck_assert_int_eq(snd_ctl_elem_read(ctl, elem_volume), 0);
	ck_assert_int_eq(snd_ctl_elem_value_get_integer(elem_volume, 0), 127);
	ck_assert_int_eq(snd_ctl_elem_value_get_integer(elem_volume, 1), 127);

	snd_ctl_elem_value_set_integer(elem_volume, 0, 42);
	snd_ctl_elem_value_set_integer(elem_volume, 1, 42);
	ck_assert_int_gt(snd_ctl_elem_write(ctl, elem_volume), 0);

	ck_assert_int_eq(snd_ctl_elem_read(ctl, elem_volume), 0);
	ck_assert_int_eq(snd_ctl_elem_value_get_integer(elem_volume, 0), 42);
	ck_assert_int_eq(snd_ctl_elem_value_get_integer(elem_volume, 1), 42);

	ck_assert_int_eq(test_pcm_close(pid, ctl), 0);

} END_TEST

START_TEST(test_volume_db_range) {
	fprintf(stderr, "\nSTART TEST: %s (%s:%d)\n", __func__, __FILE__, __LINE__);

	snd_ctl_t *ctl = NULL;
	pid_t pid = -1;

	ck_assert_int_eq(test_ctl_open(&pid, &ctl, 0), 0);

	snd_ctl_elem_id_t *elem;
	snd_ctl_elem_id_alloca(&elem);
	/* 12:34:56:78:9A:BC - A2DP Playback Volume */
	snd_ctl_elem_id_set_numid(elem, 4);

	long min, max;
	ck_assert_int_eq(snd_ctl_get_dB_range(ctl, elem, &min, &max), 0);
	ck_assert_int_eq(min, -9600);
	ck_assert_int_eq(max, 0);

	ck_assert_int_eq(test_pcm_close(pid, ctl), 0);

} END_TEST

START_TEST(test_single_device) {
	fprintf(stderr, "\nSTART TEST: %s (%s:%d)\n", __func__, __FILE__, __LINE__);

	snd_ctl_t *ctl = NULL;
	pid_t pid = -1;

	const char *service = "test";
	ck_assert_int_ne(pid = spawn_bluealsa_server(service, 1000,
				true, 0, true, true, false, false), -1);

	ck_assert_int_eq(snd_ctl_open_bluealsa(&ctl, service,
				"device \"00:00:00:00:00:00\"", 0), 0);

	snd_ctl_card_info_t *info;
	snd_ctl_card_info_alloca(&info);

	ck_assert_int_eq(snd_ctl_card_info(ctl, info), 0);
	ck_assert_str_eq(snd_ctl_card_info_get_name(info), "23:45:67:89:AB:CD");

	snd_ctl_elem_list_t *elems;
	snd_ctl_elem_list_alloca(&elems);

	ck_assert_int_eq(snd_ctl_elem_list(ctl, elems), 0);
	ck_assert_int_eq(snd_ctl_elem_list_get_count(elems), 4);
	ck_assert_int_eq(snd_ctl_elem_list_alloc_space(elems, 4), 0);
	ck_assert_int_eq(snd_ctl_elem_list(ctl, elems), 0);

	ck_assert_str_eq(snd_ctl_elem_list_get_name(elems, 0), "A2DP Capture Switch");
	ck_assert_str_eq(snd_ctl_elem_list_get_name(elems, 1), "A2DP Capture Volume");
	ck_assert_str_eq(snd_ctl_elem_list_get_name(elems, 2), "A2DP Playback Switch");
	ck_assert_str_eq(snd_ctl_elem_list_get_name(elems, 3), "A2DP Playback Volume");

	ck_assert_int_eq(test_pcm_close(pid, ctl), 0);

} END_TEST

START_TEST(test_single_device_not_connected) {
	fprintf(stderr, "\nSTART TEST: %s (%s:%d)\n", __func__, __FILE__, __LINE__);

	snd_ctl_t *ctl = NULL;
	pid_t pid = -1;

	const char *service = "test";
	ck_assert_int_ne(pid = spawn_bluealsa_server(service, 1000,
				true, 0, false, false, false, false), -1);

	ck_assert_int_eq(snd_ctl_open_bluealsa(&ctl, service,
				"device \"00:00:00:00:00:00\"", 0), -ENODEV);

	ck_assert_int_eq(test_pcm_close(pid, ctl), 0);

} END_TEST

START_TEST(test_single_device_no_such_device) {
	fprintf(stderr, "\nSTART TEST: %s (%s:%d)\n", __func__, __FILE__, __LINE__);

	snd_ctl_t *ctl = NULL;
	pid_t pid = -1;

	const char *service = "test";
	ck_assert_int_ne(pid = spawn_bluealsa_server(service, 1000,
				true, 0, true, false, false, false), -1);

	ck_assert_int_eq(snd_ctl_open_bluealsa(&ctl, service,
				"device \"DE:AD:12:34:56:78\"", 0), -ENODEV);

	ck_assert_int_eq(test_pcm_close(pid, ctl), 0);

} END_TEST

START_TEST(test_single_device_non_dynamic) {
	fprintf(stderr, "\nSTART TEST: %s (%s:%d)\n", __func__, __FILE__, __LINE__);

	snd_ctl_t *ctl = NULL;
	pid_t pid = -1;

	const char *service = "test";
	ck_assert_int_ne(pid = spawn_bluealsa_server(service, 0,
				true, 500, false, true, false, true), -1);

	ck_assert_int_eq(snd_ctl_open_bluealsa(&ctl, service,
				"device \"23:45:67:89:AB:CD\"\n"
				"battery \"no\"\n"
				"dynamic \"no\"\n", 0), 0);
	ck_assert_int_eq(snd_ctl_subscribe_events(ctl, 1), 0);

	snd_ctl_elem_list_t *elems;
	snd_ctl_elem_list_alloca(&elems);

	snd_ctl_event_t *event;
	snd_ctl_event_malloc(&event);

	ck_assert_int_eq(snd_ctl_elem_list(ctl, elems), 0);
	ck_assert_int_eq(snd_ctl_elem_list_get_count(elems), 6);

	snd_ctl_elem_value_t *elem_volume;
	snd_ctl_elem_value_alloca(&elem_volume);
	/* A2DP Capture Volume */
	snd_ctl_elem_value_set_numid(elem_volume, 2);

	snd_ctl_elem_value_set_integer(elem_volume, 0, 42);
	ck_assert_int_gt(snd_ctl_elem_write(ctl, elem_volume), 0);

	/* check whether element value was updated */
	ck_assert_int_eq(snd_ctl_elem_read(ctl, elem_volume), 0);
	ck_assert_int_eq(snd_ctl_elem_value_get_integer(elem_volume, 0), 42);

	/* Process events until we will be notified about A2DP profile disconnection.
	 * We shall get 2 events from previous value update and 2 events for profile
	 * disconnection (one event per switch/volume element). */
	for (size_t events = 0; events < 4;) {
		ck_assert_int_eq(snd_ctl_wait(ctl, 750), 1);
		while (snd_ctl_read(ctl, event) == 1)
			events++;
	}

	/* the number of elements shall not change */
	ck_assert_int_eq(snd_ctl_elem_list(ctl, elems), 0);
	ck_assert_int_eq(snd_ctl_elem_list_get_count(elems), 6);

	/* element shall be "deactivated" */
	ck_assert_int_eq(snd_ctl_elem_read(ctl, elem_volume), 0);
	ck_assert_int_eq(snd_ctl_elem_value_get_integer(elem_volume, 0), 0);

	snd_ctl_elem_value_set_integer(elem_volume, 0, 42);
	ck_assert_int_gt(snd_ctl_elem_write(ctl, elem_volume), 0);

	ck_assert_int_eq(snd_ctl_elem_read(ctl, elem_volume), 0);
	ck_assert_int_eq(snd_ctl_elem_value_get_integer(elem_volume, 0), 0);

	snd_ctl_event_free(event);
	ck_assert_int_eq(test_pcm_close(pid, ctl), 0);

} END_TEST

START_TEST(test_notifications) {
	fprintf(stderr, "\nSTART TEST: %s (%s:%d)\n", __func__, __FILE__, __LINE__);

	snd_ctl_t *ctl = NULL;
	pid_t pid = -1;

	const char *service = "test";
	ck_assert_int_ne(pid = spawn_bluealsa_server(service, 0xFFFF,
				false, 250, true, false, true, false), -1);

	ck_assert_int_eq(snd_ctl_open_bluealsa(&ctl, service, "", 0), 0);
	ck_assert_int_eq(snd_ctl_subscribe_events(ctl, 1), 0);

	snd_ctl_event_t *event;
	snd_ctl_event_malloc(&event);

	size_t events = 0;
	while (snd_ctl_wait(ctl, 500) == 1)
		while (snd_ctl_read(ctl, event) == 1) {
			ck_assert_int_eq(snd_ctl_event_get_type(event), SND_CTL_EVENT_ELEM);
			events++;
		}

	/* Processed events:
	 * - 0 removes; 2 new elems (12:34:... A2DP)
	 * - 2 removes; 4 new elems (12:34:... A2DP, 23:45:... A2DP)
	 * - 4 removes; 7 new elems (2x A2DP, SCO playback, battery)
	 * - 7 removes; 9 new elems (2x A2DP, SCO playback/capture, battery)
	 * - 4 updates (SCO codec update) */
	ck_assert_int_eq(events, (0 + 2) + (2 + 4) + (4 + 7) + (7 + 9) + 4);

	snd_ctl_event_free(event);
	ck_assert_int_eq(test_pcm_close(pid, ctl), 0);

} END_TEST

int main(int argc, char *argv[]) {

	preload(argc, argv, ".libs/aloader.so");

	/* test-alsa-ctl and bluealsa-mock shall
	 * be placed in the same directory */
	char *argv_0 = strdup(argv[0]);
	bluealsa_mock_path = dirname(argv_0);

	Suite *s = suite_create(__FILE__);
	TCase *tc = tcase_create(__FILE__);
	SRunner *sr = srunner_create(s);

	suite_add_tcase(s, tc);

	tcase_add_test(tc, test_controls);
	tcase_add_test(tc, test_mute_and_volume);
	tcase_add_test(tc, test_volume_db_range);
	tcase_add_test(tc, test_single_device);
	tcase_add_test(tc, test_single_device_not_connected);
	tcase_add_test(tc, test_single_device_no_such_device);
	tcase_add_test(tc, test_single_device_non_dynamic);
	tcase_add_test(tc, test_notifications);

	srunner_run_all(sr, CK_ENV);
	int nf = srunner_ntests_failed(sr);

	srunner_free(sr);
	free(argv_0);

	return nf == 0 ? 0 : 1;
}
