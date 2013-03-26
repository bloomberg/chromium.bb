# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Configuration file for android gtest suites."""

import collections


Suite = collections.namedtuple('Suite', ['is_suite_exe', 'name'])
Exe = lambda name : Suite(True, name)
Apk = lambda name : Suite(False, name)


# Add new suites here before upgrading them to the stable list below.
EXPERIMENTAL_TEST_SUITES = [
]

# Do not modify this list without approval of an android owner.
# This list determines which suites are run by default, both for local
# testing and on android trybots running on commit-queue.
STABLE_TEST_SUITES = [
    Apk('TestWebKitAPI'),
    Apk('android_webview_unittests'),
    Apk('base_unittests'),
    Apk('cc_unittests'),
    Apk('components_unittests'),
    Apk('content_unittests'),
    Apk('gpu_unittests'),
    Apk('ipc_tests'),
    Apk('media_unittests'),
    Apk('net_unittests'),
    Apk('sql_unittests'),
    Apk('sync_unit_tests'),
    Apk('ui_unittests'),
    Apk('unit_tests'),
    Apk('webkit_compositor_bindings_unittests'),
    Apk('webkit_unit_tests'),
    Exe('breakpad_unittests'),
    Exe('sandbox_linux_unittests'),
]
