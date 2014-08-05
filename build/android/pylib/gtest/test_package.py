# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Base class representing GTest test packages."""
# pylint: disable=R0201


class TestPackage(object):

  """A helper base class for both APK and stand-alone executables.

  Args:
    suite_name: Name of the test suite (e.g. base_unittests).
  """
  def __init__(self, suite_name):
    self.suite_name = suite_name

  def ClearApplicationState(self, device):
    """Clears the application state.

    Args:
      device: Instance of DeviceUtils.
    """
    raise NotImplementedError('Method must be overriden.')

  def CreateCommandLineFileOnDevice(self, device, test_filter, test_arguments):
    """Creates a test runner script and pushes to the device.

    Args:
      device: Instance of DeviceUtils.
      test_filter: A test_filter flag.
      test_arguments: Additional arguments to pass to the test binary.
    """
    raise NotImplementedError('Method must be overriden.')

  def GetAllTests(self, device):
    """Returns a list of all tests available in the test suite.

    Args:
      device: Instance of DeviceUtils.
    """
    raise NotImplementedError('Method must be overriden.')

  def GetGTestReturnCode(self, _device):
    return None

  def SpawnTestProcess(self, device):
    """Spawn the test process.

    Args:
      device: Instance of DeviceUtils.

    Returns:
      An instance of pexpect spawn class.
    """
    raise NotImplementedError('Method must be overriden.')

  def Install(self, device):
    """Install the test package to the device.

    Args:
      device: Instance of DeviceUtils.
    """
    raise NotImplementedError('Method must be overriden.')

  @staticmethod
  def _ParseGTestListTests(raw_list):
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
      test_name = test.split(None, 1)[0]
      ret += [current + test_name]
    return ret
