# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os

from pylib import android_commands
from pylib import cmd_helper
from pylib import constants
from pylib import ports
from pylib.base import shard
from pylib.gtest import dispatch as gtest_dispatch
from pylib.gtest import test_runner

CONTENT_BROWSERTEST_SUITENAME = 'content_browsertests'


def Dispatch(options):
  attached_devices = []
  if options.test_device:
    attached_devices = [options.test_device]
  else:
    attached_devices = android_commands.GetAttachedDevices()

  if not attached_devices:
    logging.critical('A device must be attached and online.')
    return 1

  # Reset the test port allocation. It's important to do it before starting
  # to dispatch any tests.
  if not ports.ResetTestServerPortAllocation():
    raise Exception('Failed to reset test server port.')

  test_suite_dir = os.path.join(cmd_helper.OutDirectory.get(),
                                options.build_type)
  options.test_suite = os.path.join(test_suite_dir,
                                    'apks',
                                    CONTENT_BROWSERTEST_SUITENAME + '.apk')

  options.test_arguments = '--single_process %s' % options.test_arguments

  # Constructs a new TestRunner with the current options.
  def RunnerFactory(device):
    return test_runner.TestRunner(
        device,
        options.test_suite,
        options.test_arguments,
        options.timeout,
        options.cleanup_test_files,
        options.tool,
        options.build_type,
        options.webkit,
        constants.BROWSERTEST_TEST_PACKAGE_NAME,
        constants.BROWSERTEST_TEST_ACTIVITY_NAME,
        constants.BROWSERTEST_COMMAND_LINE_FILE)

    # Get tests and split them up based on the number of devices.
  if options.gtest_filter:
    all_tests = [t for t in options.gtest_filter.split(':') if t]
  else:
    all_tests = gtest_dispatch.GetAllEnabledTests(RunnerFactory,
                                                  attached_devices)

  # Run tests.
  test_results = shard.ShardAndRunTests(RunnerFactory, attached_devices,
                                        all_tests, options.build_type)
  test_results.LogFull(
      test_type='Unit test',
      test_package=CONTENT_BROWSERTEST_SUITENAME,
      build_type=options.build_type,
      flakiness_server=options.flakiness_dashboard_server)
  test_results.PrintAnnotation()
