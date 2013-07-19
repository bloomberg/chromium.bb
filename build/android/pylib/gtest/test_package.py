# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Base class representing GTest test packages."""

import os

from pylib import constants


class TestPackage(object):
  """A helper base class for both APK and stand-alone executables.

  Args:
    adb: ADB interface the tests are using.
    device: Device to run the tests.
    suite_path_full: Absolute path to a specific test suite to run,
        empty to run all.
        Ex: '/foo/bar/base_unittests-debug.apk', for which
          self.suite_path_full = '/foo/bar/base_unittests-debug.apk'
          self.suite_path = '/foo/bar/base_unittests-debug'
          self.suite_basename = 'base_unittests'
          self.suite_dirname = '/foo/bar'
    tool: Name of the Valgrind tool.
  """

  def __init__(self, adb, device, suite_path_full, tool):
    self.adb = adb
    self.device = device
    self.suite_path_full = suite_path_full
    self.suite_path = os.path.splitext(suite_path_full)[0]
    self.suite_basename = self._GetTestSuiteBaseName()
    self.suite_dirname = os.path.dirname(
        self.suite_path.split(self.suite_basename)[0])
    self.tool = tool

  def ClearApplicationState(self):
    """Clears the application state."""
    raise NotImplementedError('Method must be overriden.')

  def CreateCommandLineFileOnDevice(self, test_filter, test_arguments):
    """Creates a test runner script and pushes to the device.

    Args:
      test_filter: A test_filter flag.
      test_arguments: Additional arguments to pass to the test binary.
    """
    raise NotImplementedError('Method must be overriden.')

  def GetAllTests(self):
    """Returns a list of all tests available in the test suite."""
    raise NotImplementedError('Method must be overriden.')

  def GetGTestReturnCode(self):
    return None

  def SpawnTestProcess(self):
    """Spawn the test process.

    Returns:
      An instance of pexpect spawn class.
    """
    raise NotImplementedError('Method must be overriden.')

  def Install(self):
    """Install the test package to the device."""
    raise NotImplementedError('Method must be overriden.')

  def _ParseGTestListTests(self, raw_list):
    """Parses a raw test list as provided by --gtest_list_tests.

    Args:
      raw_list: The raw test listing with the following format:

      IPCChannelTest.
        SendMessageInChannelConnected
      IPCSyncChannelTest.
        Simple
        DISABLED_SendWithTimeoutMixedOKAndTimeout

    Returns:
      A list of all tests. For the above raw listing:

      [IPCChannelTest.SendMessageInChannelConnected, IPCSyncChannelTest.Simple,
       IPCSyncChannelTest.DISABLED_SendWithTimeoutMixedOKAndTimeout]
    """
    ret = []
    current = ''
    for test in raw_list:
      if not test:
        continue
      if test[0] != ' ' and not test.endswith('.'):
        # Ignore any lines with unexpected format.
        continue
      if test[0] != ' ' and test.endswith('.'):
        current = test
        continue
      if 'YOU HAVE' in test:
        break
      test_name = test[2:]
      ret += [current + test_name]
    return ret
