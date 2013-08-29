# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Defines a set of constants shared by test runners and other scripts."""

import collections
import os
import subprocess
import sys


DIR_SOURCE_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__),
                                               os.pardir, os.pardir, os.pardir))
ISOLATE_DEPS_DIR = os.path.join(DIR_SOURCE_ROOT, 'isolate_deps_dir')
EMULATOR_SDK_ROOT = os.path.abspath(os.path.join(DIR_SOURCE_ROOT, os.pardir,
                                                 os.pardir))

# TODO(craigdh): Remove these once references have been removed downstream.
CHROME_PACKAGE = 'com.google.android.apps.chrome'
CHROME_ACTIVITY = 'com.google.android.apps.chrome.Main'

CHROME_STABLE_PACKAGE = 'com.android.chrome'
CHROME_BETA_PACKAGE = 'com.chrome.beta'

LEGACY_BROWSER_PACKAGE = 'com.google.android.browser'
LEGACY_BROWSER_ACTIVITY = 'com.android.browser.BrowserActivity'

CONTENT_SHELL_PACKAGE = 'org.chromium.content_shell_apk'
CONTENT_SHELL_ACTIVITY = 'org.chromium.content_shell_apk.ContentShellActivity'

CHROMIUM_TEST_SHELL_PACKAGE = 'org.chromium.chrome.testshell'
CHROMIUM_TEST_SHELL_ACTIVITY = (
    'org.chromium.chrome.testshell.ChromiumTestShellActivity')
CHROMIUM_TEST_SHELL_DEVTOOLS_SOCKET = 'chromium_testshell_devtools_remote'
CHROMIUM_TEST_SHELL_HOST_DRIVEN_DIR = os.path.join(
    DIR_SOURCE_ROOT, 'chrome', 'android')


PackageInfo = collections.namedtuple('PackageInfo',
    ['package', 'activity', 'cmdline_file', 'devtools_socket',
     'test_package'])

PACKAGE_INFO = {
    'chrome': PackageInfo(
        CHROME_PACKAGE,
        CHROME_ACTIVITY,
        '/data/local/chrome-command-line',
        'chrome_devtools_remote',
        'com.google.android.apps.chrome.tests'),
    'legacy_browser': PackageInfo(
        LEGACY_BROWSER_PACKAGE,
        LEGACY_BROWSER_ACTIVITY,
        None,
        None,
        None),
    'content_shell': PackageInfo(
        CONTENT_SHELL_PACKAGE,
        CONTENT_SHELL_ACTIVITY,
        '/data/local/tmp/content-shell-command-line',
        None,
        None),
    'chromium_test_shell': PackageInfo(
        CHROMIUM_TEST_SHELL_PACKAGE,
        CHROMIUM_TEST_SHELL_ACTIVITY,
        '/data/local/tmp/chromium-testshell-command-line',
        CHROMIUM_TEST_SHELL_DEVTOOLS_SOCKET,
        'org.chromium.chrome.testshell.tests'),
    'gtest': PackageInfo(
        'org.chromium.native_test',
        'org.chromium.native_test.ChromeNativeTestActivity',
        '/data/local/tmp/chrome-native-tests-command-line',
        None,
        None),
    'content_browsertests': PackageInfo(
        'org.chromium.content_browsertests_apk',
        'org.chromium.content_browsertests_apk.ContentBrowserTestsActivity',
        '/data/local/tmp/content-browser-tests-command-line',
        None,
        None),
}


# Ports arrangement for various test servers used in Chrome for Android.
# Lighttpd server will attempt to use 9000 as default port, if unavailable it
# will find a free port from 8001 - 8999.
LIGHTTPD_DEFAULT_PORT = 9000
LIGHTTPD_RANDOM_PORT_FIRST = 8001
LIGHTTPD_RANDOM_PORT_LAST = 8999
TEST_SYNC_SERVER_PORT = 9031
TEST_SEARCH_BY_IMAGE_SERVER_PORT = 9041

# The net test server is started from port 10201.
# TODO(pliard): http://crbug.com/239014. Remove this dirty workaround once
# http://crbug.com/239014 is fixed properly.
TEST_SERVER_PORT_FIRST = 10201
TEST_SERVER_PORT_LAST = 30000
# A file to record next valid port of test server.
TEST_SERVER_PORT_FILE = '/tmp/test_server_port'
TEST_SERVER_PORT_LOCKFILE = '/tmp/test_server_port.lock'

TEST_EXECUTABLE_DIR = '/data/local/tmp'
# Directories for common java libraries for SDK build.
# These constants are defined in build/android/ant/common.xml
SDK_BUILD_JAVALIB_DIR = 'lib.java'
SDK_BUILD_TEST_JAVALIB_DIR = 'test.lib.java'
SDK_BUILD_APKS_DIR = 'apks'

# The directory on the device where perf test output gets saved to.
DEVICE_PERF_OUTPUT_DIR = '/data/data/' + CHROME_PACKAGE + '/files'

SCREENSHOTS_DIR = os.path.join(DIR_SOURCE_ROOT, 'out_screenshots')

ANDROID_SDK_VERSION = 18
ANDROID_SDK_ROOT = os.path.join(DIR_SOURCE_ROOT,
                                'third_party/android_tools/sdk')
ANDROID_NDK_ROOT = os.path.join(DIR_SOURCE_ROOT,
                                'third_party/android_tools/ndk')

UPSTREAM_FLAKINESS_SERVER = 'test-results.appspot.com'


def GetBuildType():
  try:
    return os.environ['BUILDTYPE']
  except KeyError:
    raise Exception('The BUILDTYPE environment variable has not been set')


def SetBuildType(build_type):
  os.environ['BUILDTYPE'] = build_type


def _GetADBPath():
  if os.environ.get('ANDROID_SDK_ROOT'):
    return 'adb'
  # If envsetup.sh hasn't been sourced and there's no adb in the path,
  # set it here.
  try:
    with file(os.devnull, 'w') as devnull:
      subprocess.call(['adb', 'version'], stdout=devnull, stderr=devnull)
    return 'adb'
  except OSError:
    print >> sys.stderr, 'No adb found in $PATH, fallback to checked in binary.'
    return os.path.join(ANDROID_SDK_ROOT, 'platform-tools', 'adb')


ADB_PATH = _GetADBPath()

# Exit codes
ERROR_EXIT_CODE = 1
WARNING_EXIT_CODE = 88
