# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Defines a set of constants shared by test runners and other scripts."""

# TODO(jbudorick): Split these constants into coherent modules.

# pylint: disable=W0212

import collections
import logging
import os
import subprocess

import devil.android.sdk.keyevent
from devil.android.sdk import version_codes

keyevent = devil.android.sdk.keyevent


DIR_SOURCE_ROOT = os.environ.get('CHECKOUT_SOURCE_ROOT',
    os.path.abspath(os.path.join(os.path.dirname(__file__),
                                 os.pardir, os.pardir, os.pardir, os.pardir)))

PackageInfo = collections.namedtuple('PackageInfo',
    ['package', 'activity', 'cmdline_file', 'devtools_socket',
     'test_package'])

PACKAGE_INFO = {
    'chrome_document': PackageInfo(
        'com.google.android.apps.chrome.document',
        'com.google.android.apps.chrome.document.ChromeLauncherActivity',
        '/data/local/chrome-command-line',
        'chrome_devtools_remote',
        None),
    'chrome': PackageInfo(
        'com.google.android.apps.chrome',
        'com.google.android.apps.chrome.Main',
        '/data/local/chrome-command-line',
        'chrome_devtools_remote',
        'com.google.android.apps.chrome.tests'),
    'chrome_beta': PackageInfo(
        'com.chrome.beta',
        'com.google.android.apps.chrome.Main',
        '/data/local/chrome-command-line',
        'chrome_devtools_remote',
        None),
    'chrome_stable': PackageInfo(
        'com.android.chrome',
        'com.google.android.apps.chrome.Main',
        '/data/local/chrome-command-line',
        'chrome_devtools_remote',
        None),
    'chrome_dev': PackageInfo(
        'com.chrome.dev',
        'com.google.android.apps.chrome.Main',
        '/data/local/chrome-command-line',
        'chrome_devtools_remote',
        None),
    'chrome_canary': PackageInfo(
        'com.chrome.canary',
        'com.google.android.apps.chrome.Main',
        '/data/local/chrome-command-line',
        'chrome_devtools_remote',
        None),
    'chrome_work': PackageInfo(
        'com.chrome.work',
        'com.google.android.apps.chrome.Main',
        '/data/local/chrome-command-line',
        'chrome_devtools_remote',
        None),
    'chromium': PackageInfo(
        'org.chromium.chrome',
        'com.google.android.apps.chrome.Main',
        '/data/local/chrome-command-line',
        'chrome_devtools_remote',
        'org.chromium.chrome.tests'),
    'legacy_browser': PackageInfo(
        'com.google.android.browser',
        'com.android.browser.BrowserActivity',
        None,
        None,
        None),
    'chromecast_shell': PackageInfo(
        'com.google.android.apps.mediashell',
        'com.google.android.apps.mediashell.MediaShellActivity',
        '/data/local/tmp/castshell-command-line',
        None,
        None),
    'content_shell': PackageInfo(
        'org.chromium.content_shell_apk',
        'org.chromium.content_shell_apk.ContentShellActivity',
        '/data/local/tmp/content-shell-command-line',
        None,
        'org.chromium.content_shell_apk.tests'),
    'android_webview_shell': PackageInfo(
        'org.chromium.android_webview.shell',
        'org.chromium.android_webview.shell.AwShellActivity',
        '/data/local/tmp/android-webview-command-line',
        None,
        'org.chromium.android_webview.test'),
    'gtest': PackageInfo(
        'org.chromium.native_test',
        'org.chromium.native_test.NativeUnitTestActivity',
        '/data/local/tmp/chrome-native-tests-command-line',
        None,
        None),
    'components_browsertests': PackageInfo(
        'org.chromium.components_browsertests_apk',
        ('org.chromium.components_browsertests_apk' +
         '.ComponentsBrowserTestsActivity'),
        '/data/local/tmp/chrome-native-tests-command-line',
        None,
        None),
    'content_browsertests': PackageInfo(
        'org.chromium.content_browsertests_apk',
        'org.chromium.content_browsertests_apk.ContentBrowserTestsActivity',
        '/data/local/tmp/chrome-native-tests-command-line',
        None,
        None),
    'chromedriver_webview_shell': PackageInfo(
        'org.chromium.chromedriver_webview_shell',
        'org.chromium.chromedriver_webview_shell.Main',
        None,
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
TEST_POLICY_SERVER_PORT = 9051


TEST_EXECUTABLE_DIR = '/data/local/tmp'
# Directories for common java libraries for SDK build.
# These constants are defined in build/android/ant/common.xml
SDK_BUILD_JAVALIB_DIR = 'lib.java'
SDK_BUILD_TEST_JAVALIB_DIR = 'test.lib.java'
SDK_BUILD_APKS_DIR = 'apks'

ADB_KEYS_FILE = '/data/misc/adb/adb_keys'

PERF_OUTPUT_DIR = os.path.join(DIR_SOURCE_ROOT, 'out', 'step_results')
# The directory on the device where perf test output gets saved to.
DEVICE_PERF_OUTPUT_DIR = (
    '/data/data/' + PACKAGE_INFO['chrome'].package + '/files')

SCREENSHOTS_DIR = os.path.join(DIR_SOURCE_ROOT, 'out_screenshots')

ANDROID_SDK_VERSION = version_codes.MARSHMALLOW
ANDROID_SDK_BUILD_TOOLS_VERSION = '23.0.0'
ANDROID_SDK_ROOT = os.path.join(DIR_SOURCE_ROOT,
                                'third_party', 'android_tools', 'sdk')
ANDROID_SDK_TOOLS = os.path.join(ANDROID_SDK_ROOT,
                                 'build-tools', ANDROID_SDK_BUILD_TOOLS_VERSION)
ANDROID_NDK_ROOT = os.path.join(DIR_SOURCE_ROOT,
                                'third_party', 'android_tools', 'ndk')

PROGUARD_SCRIPT_PATH = os.path.join(
    ANDROID_SDK_ROOT, 'tools', 'proguard', 'bin', 'proguard.sh')

PROGUARD_ROOT = os.path.join(DIR_SOURCE_ROOT, 'third_party', 'proguard')

BAD_DEVICES_JSON = os.path.join(DIR_SOURCE_ROOT,
                                os.environ.get('CHROMIUM_OUT_DIR', 'out'),
                                'bad_devices.json')

UPSTREAM_FLAKINESS_SERVER = 'test-results.appspot.com'

# TODO(jbudorick): Remove once unused.
DEVICE_LOCAL_PROPERTIES_PATH = '/data/local.prop'

# TODO(jbudorick): Rework this into testing/buildbot/
PYTHON_UNIT_TEST_SUITES = {
  'pylib_py_unittests': {
    'path': os.path.join(DIR_SOURCE_ROOT, 'build', 'android'),
    'test_modules': [
      'devil.android.device_utils_test',
      'devil.android.md5sum_test',
      'devil.utils.cmd_helper_test',
      'pylib.results.json_results_test',
      'pylib.utils.proguard_test',
    ]
  },
  'gyp_py_unittests': {
    'path': os.path.join(DIR_SOURCE_ROOT, 'build', 'android', 'gyp'),
    'test_modules': [
      'java_cpp_enum_tests',
    ]
  },
}

LOCAL_MACHINE_TESTS = ['junit', 'python']
VALID_ENVIRONMENTS = ['local', 'remote_device']
VALID_TEST_TYPES = ['gtest', 'instrumentation', 'junit', 'linker', 'monkey',
                    'perf', 'python', 'uiautomator', 'uirobot']
VALID_DEVICE_TYPES = ['Android', 'iOS']


def GetBuildType():
  try:
    return os.environ['BUILDTYPE']
  except KeyError:
    raise EnvironmentError(
        'The BUILDTYPE environment variable has not been set')


def SetBuildType(build_type):
  os.environ['BUILDTYPE'] = build_type


def SetBuildDirectory(build_directory):
  os.environ['CHROMIUM_OUT_DIR'] = build_directory


def SetOutputDirectory(output_directory):
  os.environ['CHROMIUM_OUTPUT_DIR'] = output_directory


def GetOutDirectory(build_type=None):
  """Returns the out directory where the output binaries are built.

  Args:
    build_type: Build type, generally 'Debug' or 'Release'. Defaults to the
      globally set build type environment variable BUILDTYPE.
  """
  if 'CHROMIUM_OUTPUT_DIR' in os.environ:
    return os.path.abspath(os.path.join(
        DIR_SOURCE_ROOT, os.environ.get('CHROMIUM_OUTPUT_DIR')))

  return os.path.abspath(os.path.join(
      DIR_SOURCE_ROOT, os.environ.get('CHROMIUM_OUT_DIR', 'out'),
      GetBuildType() if build_type is None else build_type))


def _Memoize(func):
  def Wrapper():
    try:
      return func._result
    except AttributeError:
      func._result = func()
      return func._result
  return Wrapper


def SetAdbPath(adb_path):
  os.environ['ADB_PATH'] = adb_path


def GetAdbPath():
  # Check if a custom adb path as been set. If not, try to find adb
  # on the system.
  if os.environ.get('ADB_PATH'):
    return os.environ.get('ADB_PATH')
  else:
    return _FindAdbPath()


@_Memoize
def _FindAdbPath():
  if os.environ.get('ANDROID_SDK_ROOT'):
    return 'adb'
  # If envsetup.sh hasn't been sourced and there's no adb in the path,
  # set it here.
  try:
    with file(os.devnull, 'w') as devnull:
      subprocess.call(['adb', 'version'], stdout=devnull, stderr=devnull)
    return 'adb'
  except OSError:
    logging.debug('No adb found in $PATH, fallback to checked in binary.')
    return os.path.join(ANDROID_SDK_ROOT, 'platform-tools', 'adb')

# Exit codes
ERROR_EXIT_CODE = 1
INFRA_EXIT_CODE = 87
WARNING_EXIT_CODE = 88
