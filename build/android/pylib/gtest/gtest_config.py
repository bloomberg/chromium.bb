# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Configuration file for android gtest suites."""

# Add new suites here before upgrading them to the stable list below.
EXPERIMENTAL_TEST_SUITES = [
    'sandbox_linux_unittests',
]

# Do not modify this list without approval of an android owner.
# This list determines which suites are run by default, both for local
# testing and on android trybots running on commit-queue.
STABLE_TEST_SUITES = [
    'base_unittests',
    'cc_unittests',
    'content_unittests',
    'gpu_unittests',
    'ipc_tests',
    'media_unittests',
    'net_unittests',
    'sql_unittests',
    'sync_unit_tests',
    'ui_unittests',
    'unit_tests',
    'webkit_compositor_bindings_unittests',
    'android_webview_unittests',
]
