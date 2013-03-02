# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Defines a set of constants shared by test runners and other scripts."""

import os


CHROME_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__),
                                          os.pardir, os.pardir, os.pardir))

CHROME_PACKAGE = 'com.google.android.apps.chrome'
CHROME_ACTIVITY = 'com.google.android.apps.chrome.Main'
CHROME_DEVTOOLS_SOCKET = 'chrome_devtools_remote'

CHROME_TESTS_PACKAGE = 'com.google.android.apps.chrome.tests'

LEGACY_BROWSER_PACKAGE = 'com.google.android.browser'
LEGACY_BROWSER_ACTIVITY = 'com.android.browser.BrowserActivity'

CONTENT_SHELL_PACKAGE = 'org.chromium.content_shell_apk'
CONTENT_SHELL_ACTIVITY = 'org.chromium.content_shell_apk.ContentShellActivity'

CHROME_SHELL_PACKAGE = 'org.chromium.chrome.browser.test'

CHROMIUM_TEST_SHELL_PACKAGE = 'org.chromium.chrome.testshell'
CHROMIUM_TEST_SHELL_ACTIVITY = (
    'org.chromium.chrome.testshell.ChromiumTestShellActivity')
CHROMIUM_TEST_SHELL_DEVTOOLS_SOCKET = 'chromium_testshell_devtools_remote'
CHROMIUM_TEST_SHELL_HOST_DRIVEN_DIR = os.path.join(
    CHROME_DIR, 'chrome', 'android')

GTEST_TEST_PACKAGE_NAME = 'org.chromium.native_test'
GTEST_TEST_ACTIVITY_NAME = 'org.chromium.native_test.ChromeNativeTestActivity'
GTEST_COMMAND_LINE_FILE = 'chrome-native-tests-command-line'

BROWSERTEST_SUITE_NAME = 'content_browsertests'
BROWSERTEST_TEST_PACKAGE_NAME = 'org.chromium.content_browsertests_apk'
BROWSERTEST_TEST_ACTIVITY_NAME = (
    'org.chromium.content_browsertests_apk.ContentBrowserTestsActivity')
BROWSERTEST_COMMAND_LINE_FILE = 'content-browser-tests-command-line'

# Ports arrangement for various test servers used in Chrome for Android.
# Lighttpd server will attempt to use 9000 as default port, if unavailable it
# will find a free port from 8001 - 8999.
LIGHTTPD_DEFAULT_PORT = 9000
LIGHTTPD_RANDOM_PORT_FIRST = 8001
LIGHTTPD_RANDOM_PORT_LAST = 8999
TEST_SYNC_SERVER_PORT = 9031

# The net test server is started from 10000. Reserve 20000 ports for the all
# test-server based tests should be enough for allocating different port for
# individual test-server based test.
TEST_SERVER_PORT_FIRST = 10000
TEST_SERVER_PORT_LAST = 30000
# A file to record next valid port of test server.
TEST_SERVER_PORT_FILE = '/tmp/test_server_port'
TEST_SERVER_PORT_LOCKFILE = '/tmp/test_server_port.lock'

TEST_EXECUTABLE_DIR = '/data/local/tmp'
# Directories for common java libraries for SDK build.
# These constants are defined in build/android/ant/common.xml
SDK_BUILD_TEST_JAVALIB_DIR = 'test.lib.java'
SDK_BUILD_APKS_DIR = 'apks'

# The directory on the device where perf test output gets saved to.
DEVICE_PERF_OUTPUT_DIR = '/data/data/' + CHROME_PACKAGE + '/files'

SCREENSHOTS_DIR = os.path.join(CHROME_DIR, 'out_screenshots')

ANDROID_SDK_VERSION = 17
ANDROID_SDK_ROOT = os.path.join(CHROME_DIR, 'third_party/android_tools/sdk')
ANDROID_NDK_ROOT = os.path.join(CHROME_DIR, 'third_party/android_tools/ndk')

UPSTREAM_FLAKINESS_SERVER = 'test-results.appspot.com'
