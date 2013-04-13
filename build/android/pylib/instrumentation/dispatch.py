# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Dispatches the instrumentation tests."""

import logging
import os

from pylib import android_commands
from pylib.base import base_test_result
from pylib.base import shard

import test_package
import test_runner


def Dispatch(options):
  """Dispatches instrumentation tests onto connected device(s).

  If possible, this method will attempt to shard the tests to
  all connected devices. Otherwise, dispatch and run tests on one device.

  Args:
    options: Command line options.

  Returns:
    A TestRunResults object holding the results of the Java tests.

  Raises:
    Exception: when there are no attached devices.
  """
  test_pkg = test_package.TestPackage(options.test_apk_path,
                                      options.test_apk_jar_path)
  tests = test_pkg._GetAllMatchingTests(
      options.annotations, options.test_filter)
  if not tests:
    logging.warning('No instrumentation tests to run with current args.')
    return base_test_result.TestRunResults()

  attached_devices = android_commands.GetAttachedDevices()
  if not attached_devices:
    raise Exception('There are no devices online.')

  if options.device:
    assert options.device in attached_devices
    attached_devices = [options.device]

  if len(attached_devices) > 1 and options.wait_for_debugger:
    logging.warning('Debugger can not be sharded, using first available device')
    attached_devices = attached_devices[:1]

  def TestRunnerFactory(device, shard_index):
    return test_runner.TestRunner(
        options, device, shard_index, test_pkg, [])

  return shard.ShardAndRunTests(TestRunnerFactory, attached_devices, tests,
                                options.build_type,
                                num_retries=options.num_retries)
