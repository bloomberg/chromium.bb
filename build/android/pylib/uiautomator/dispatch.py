# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Dispatches the uiautomator tests."""

import logging
import os

from pylib import android_commands
from pylib.base import base_test_result
from pylib.base import shard

import test_package
import test_runner


def Dispatch(options):
  """Dispatches uiautomator tests onto connected device(s).

  If possible, this method will attempt to shard the tests to
  all connected devices. Otherwise, dispatch and run tests on one device.

  Args:
    options: Command line options.

  Returns:
    A TestRunResults object holding the results of the Java tests.

  Raises:
    Exception: when there are no attached devices.
  """
  test_pkg = test_package.TestPackage(
      options.uiautomator_jar, options.uiautomator_info_jar)
  tests = test_pkg._GetAllMatchingTests(
      options.annotations, options.test_filter)
  if not tests:
    logging.warning('No uiautomator tests to run with current args.')
    return base_test_result.TestRunResults()

  attached_devices = android_commands.GetAttachedDevices()
  if not attached_devices:
    raise Exception('There are no devices online.')

  if options.device:
    assert options.device in attached_devices
    attached_devices = [options.device]

  def TestRunnerFactory(device, shard_index):
    return test_runner.TestRunner(
        options, device, shard_index, test_pkg, [])

  return shard.ShardAndRunTests(TestRunnerFactory, attached_devices, tests,
                                options.build_type)
