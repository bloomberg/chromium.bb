# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Configuration file for android gtest suites."""

# Add new suites here before upgrading them to the stable list below.
EXPERIMENTAL_TEST_SUITES = [
    'content_gl_tests',
    'heap_profiler_unittests',
]

# Do not modify this list without approval of an android owner.
# This list determines which suites are run by default, both for local
# testing and on android trybots running on commit-queue.
STABLE_TEST_SUITES = [
    'android_webview_unittests',
    'base_unittests',
    'breakpad_unittests',
    'cc_unittests',
    'components_unittests',
    'content_browsertests',
    'content_unittests',
    'events_unittests',
    'gl_tests',
    'gpu_unittests',
    'ipc_tests',
    'media_unittests',
    'net_unittests',
    'sandbox_linux_unittests',
    'sql_unittests',
    'sync_unit_tests',
    'ui_unittests',
    'unit_tests',
    'webkit_unit_tests',
]

# Tests fail in component=shared_library build, which is required for ASan.
# http://crbug.com/344868
ASAN_EXCLUDED_TEST_SUITES = [
    'breakpad_unittests',
    'sandbox_linux_unittests'
]

WEBRTC_CHROMIUM_TEST_SUITES = [
    'content_browsertests',
]

WEBRTC_NATIVE_TEST_SUITES = [
    'audio_decoder_unittests',
    'common_audio_unittests',
    'common_video_unittests',
    'modules_tests',
    'modules_unittests',
    'system_wrappers_unittests',
    'test_support_unittests',
    'tools_unittests',
    'video_engine_core_unittests',
    'voice_engine_unittests',
]
