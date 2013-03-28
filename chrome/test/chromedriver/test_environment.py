# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""TestEnvironment classes.

These classes abstract away the various setups needed to run the WebDriver java
tests in various environments.
"""

import os
import sys

_THIS_DIR = os.path.abspath(os.path.dirname(__file__))

sys.path.insert(0, os.path.join(_THIS_DIR, os.pardir, 'pylib'))

from common import util
from continuous_archive import CHROME_26_REVISION

if util.IsLinux():
  sys.path.insert(0, os.path.join(_THIS_DIR, os.pardir, os.pardir, os.pardir,
                                  'build', 'android'))
  from pylib import android_commands
  from pylib import forwarder
  from pylib import valgrind_tools

ANDROID_TEST_HTTP_PORT = 2311
ANDROID_TEST_HTTPS_PORT = 2411


_DESKTOP_NEGATIVE_FILTER = {}
_DESKTOP_NEGATIVE_FILTER['HEAD'] = [
    # https://code.google.com/p/chromedriver/issues/detail?id=283
    'ExecutingAsyncJavascriptTest#'
    'shouldNotTimeoutIfScriptCallsbackInsideAZeroTimeout',
]
_DESKTOP_NEGATIVE_FILTER[CHROME_26_REVISION] = (
    _DESKTOP_NEGATIVE_FILTER['HEAD'] + [
        'UploadTest#testFileUploading',
        'AlertsTest',
    ]
)


class BaseTestEnvironment(object):
  """Manages the environment java tests require to run."""

  def GlobalSetUp(self):
    """Sets up the global test environment state."""
    pass

  def GlobalTearDown(self):
    """Tears down the global test environment state."""
    pass

  def GetPassedJavaTests(self, filter_filename):
    """Get the list of java test that have passed in this environment.

    Args:
      filter_filename: Filename of the positive filter.

    Returns:
      List of test names.
    """
    with open(os.path.join(_THIS_DIR, filter_filename), 'r') as f:
      return [line.strip('\n') for line in f]


class DesktopTestEnvironment(BaseTestEnvironment):
  """Manages the environment java tests require to run on Desktop."""
  def __init__(self, chrome_revision):
    """Initializes a desktop test environment.

    Args:
      chrome_revision: Optionally a chrome revision to run the tests against.
    """
    self._chrome_revision = chrome_revision

  @staticmethod
  def _GetOSSpecificFailedJavaTests():
    """Get the list of java tests to disable in this specific OS.

    Returns:
      List of test names.
    """
    return []

  #override
  def GetPassedJavaTests(self, filter_filename='passed_java_tests.txt'):
    passed_java_tests = []
    failed_tests = (
        self._GetOSSpecificFailedJavaTests() +
        _DESKTOP_NEGATIVE_FILTER[self._chrome_revision]
    )
    # Filter out failed tests.
    for java_test in super(DesktopTestEnvironment, self).GetPassedJavaTests(
        filter_filename):
      suite_name = java_test.split('#')[0]
      if java_test in failed_tests or suite_name in failed_tests:
        continue
      passed_java_tests.append(java_test)
    return passed_java_tests


class LinuxTestEnvironment(DesktopTestEnvironment):
  """Manages the environment java tests require to run on Linux."""

  #override
  @staticmethod
  def _GetOSSpecificFailedJavaTests():
    return [
        # https://code.google.com/p/chromedriver/issues/detail?id=284
        'TypingTest#testArrowKeysAndPageUpAndDown',
        'TypingTest#testHomeAndEndAndPageUpAndPageDownKeys',
        'TypingTest#testNumberpadKeys',
    ]


class WindowsTestEnvironment(DesktopTestEnvironment):
  """Manages the environment java tests require to run on Windows."""

  #override
  @staticmethod
  def _GetOSSpecificFailedJavaTests():
    return [
        # https://code.google.com/p/chromedriver/issues/detail?id=282
        'PageLoadingTest#'
        'testShouldNotHangIfDocumentOpenCallIsNeverFollowedByDocumentCloseCall',
    ]


class AndroidTestEnvironment(BaseTestEnvironment):
  """Manages the environment java tests require to run on Android."""

  def __init__(self):
    super(AndroidTestEnvironment, self).__init__()
    self._adb = None
    self._forwarder = None

  def GlobalSetUp(self):
    """Sets up the global test environment state."""
    os.putenv('TEST_HTTP_PORT', str(ANDROID_TEST_HTTP_PORT))
    os.putenv('TEST_HTTPS_PORT', str(ANDROID_TEST_HTTPS_PORT))
    self._adb = android_commands.AndroidCommands()
    self._forwarder = forwarder.Forwarder(self._adb, 'Debug')
    self._forwarder.Run(
        [(ANDROID_TEST_HTTP_PORT, ANDROID_TEST_HTTP_PORT),
         (ANDROID_TEST_HTTPS_PORT, ANDROID_TEST_HTTPS_PORT)],
        valgrind_tools.BaseTool(), '127.0.0.1')

  def GlobalTearDown(self):
    """Tears down the global test environment state."""
    if self._adb is not None:
      forwarder.Forwarder.KillDevice(self._adb, valgrind_tools.BaseTool())
    if self._forwarder is not None:
      self._forwarder.Close()

  #override
  def GetPassedJavaTests(self, filter_filename='passed_java_android_tests.txt'):
    return super(AndroidTestEnvironment, self).GetPassedJavaTests(
        filter_filename)
