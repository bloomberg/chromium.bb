# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import fnmatch
import logging
import os

from pylib import cmd_helper
from pylib.base import base_test_sharder
from pylib.gtest import test_runner

class TestSharder(base_test_sharder.BaseTestSharder):
  """Responsible for sharding the tests on the connected devices."""

  def __init__(self, attached_devices, test_suite, gtest_filter,
               test_arguments, timeout, cleanup_test_files, tool,
               build_type, in_webkit_checkout):
    super(TestSharder, self).__init__(attached_devices, build_type)
    self.test_suite = test_suite
    self.gtest_filter = gtest_filter or ''
    self.test_arguments = test_arguments
    self.timeout = timeout
    self.cleanup_test_files = cleanup_test_files
    self.tool = tool
    self.in_webkit_checkout = in_webkit_checkout
    self.all_tests = []
    if not self.gtest_filter:
      # No filter has been specified, let's add all tests then.
      self.all_tests, self.attached_devices = self._GetAllEnabledTests()
    self.tests = self.all_tests

  def _GetAllEnabledTests(self):
    """Get all enabled tests and available devices.

    Obtains a list of enabled tests from the test package on the device,
    then filters it again using the diabled list on the host.

    Returns:
      Tuple of (all enabled tests, available devices).

    Raises Exception if all devices failed.
    """
    # TODO(frankf): This method is doing too much in a non-systematic way.
    # If the intention is to drop flaky devices, why not go through all devices
    # instead of breaking on the first succesfull run?
    available_devices = list(self.attached_devices)
    while available_devices:
      try:
        return (self._GetTestsFromDevice(available_devices[-1]),
                available_devices)
      except Exception as e:
        logging.warning('Failed obtaining tests from %s %s',
                        available_devices[-1], e)
        available_devices.pop()

    raise Exception('No device available to get the list of tests.')

  def _GetTestsFromDevice(self, device):
    logging.info('Obtaining tests from %s', device)
    runner = test_runner.TestRunner(
        device,
        self.test_suite,
        self.gtest_filter,
        self.test_arguments,
        self.timeout,
        self.cleanup_test_files,
        self.tool,
        0,
        self.build_type,
        self.in_webkit_checkout)
    # The executable/apk needs to be copied before we can call GetAllTests.
    runner.test_package.StripAndCopyExecutable()
    all_tests = runner.test_package.GetAllTests()
    disabled_list = runner.GetDisabledTests()
    # Only includes tests that do not have any match in the disabled list.
    all_tests = filter(lambda t:
                       not any([fnmatch.fnmatch(t, disabled_pattern)
                                for disabled_pattern in disabled_list]),
                       all_tests)
    return all_tests

  def CreateShardedTestRunner(self, device, index):
    """Creates a suite-specific test runner.

    Args:
      device: Device serial where this shard will run.
      index: Index of this device in the pool.

    Returns:
      A TestRunner object.
    """
    device_num = len(self.attached_devices)
    shard_test_list = self.tests[index::device_num]
    test_filter = ':'.join(shard_test_list) + self.gtest_filter
    return test_runner.TestRunner(
        device,
        self.test_suite,
        test_filter,
        self.test_arguments,
        self.timeout,
        self.cleanup_test_files, self.tool, index,
        self.build_type,
        self.in_webkit_checkout)
