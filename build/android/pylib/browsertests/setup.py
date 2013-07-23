# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generate test runner factory and tests for content_browsertests."""

import logging
import os
import sys

from pylib import android_commands
from pylib import cmd_helper
from pylib import constants
from pylib import ports
from pylib.base import base_test_result
from pylib.gtest import setup as gtest_setup
from pylib.gtest import test_runner
from pylib.utils import report_results


def Setup(test_arguments, timeout, cleanup_test_files, tool, build_type,
          push_deps, gtest_filter):
  """Create the test runner factory and tests.

  Args:
    test_arguments: Additional arguments to pass to the test binary.
    timeout: Timeout for each test.
    cleanup_test_files: Whether or not to cleanup test files on device.
    tool: Name of the Valgrind tool.
    build_type: 'Release' or 'Debug'.
    push_deps: If True, push all dependencies to the device.
    gtest_filter: filter for tests.

  Returns:
    A tuple of (TestRunnerFactory, tests).
  """

  if not ports.ResetTestServerPortAllocation():
    raise Exception('Failed to reset test server port.')

  suite_path = os.path.join(cmd_helper.OutDirectory.get(), build_type, 'apks',
                            constants.BROWSERTEST_SUITE_NAME + '.apk')

  gtest_setup._GenerateDepsDirUsingIsolate(
      constants.BROWSERTEST_SUITE_NAME, build_type)

  # Constructs a new TestRunner with the current options.
  def TestRunnerFactory(device, shard_index):
    return test_runner.TestRunner(
        device,
        suite_path,
        test_arguments,
        timeout,
        cleanup_test_files,
        tool,
        build_type,
        push_deps,
        constants.BROWSERTEST_TEST_PACKAGE_NAME,
        constants.BROWSERTEST_TEST_ACTIVITY_NAME,
        constants.BROWSERTEST_COMMAND_LINE_FILE)

  # TODO(gkanwar): This breaks the abstraction of having test_dispatcher.py deal
  # entirely with the devices. Can we do this another way?
  attached_devices = android_commands.GetAttachedDevices()

  all_tests = gtest_setup.GetTestsFiltered(
      constants.BROWSERTEST_SUITE_NAME, gtest_filter, TestRunnerFactory,
      attached_devices)

  return (TestRunnerFactory, all_tests)


def _FilterTests(all_enabled_tests):
  """Filters out tests and fixtures starting with PRE_ and MANUAL_."""
  return [t for t in all_enabled_tests if _ShouldRunOnBot(t)]


def _ShouldRunOnBot(test):
  fixture, case = test.split('.', 1)
  if _StartsWith(fixture, case, 'PRE_'):
    return False
  if _StartsWith(fixture, case, 'MANUAL_'):
    return False
  return True


def _StartsWith(a, b, prefix):
  return a.startswith(prefix) or b.startswith(prefix)
