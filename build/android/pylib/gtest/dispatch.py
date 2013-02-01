# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import logging
import os

from pylib import android_commands
from pylib import cmd_helper
from pylib import ports
from pylib.utils import emulator
from pylib.utils import xvfb

import gtest_config
import test_sharder


def _FullyQualifiedTestSuites(exe, option_test_suite, build_type):
  """Get a list of absolute paths to test suite targets.

  Args:
    exe: if True, use the executable-based test runner.
    option_test_suite: the test_suite specified as an option.
    build_type: 'Release' or 'Debug'.

  Returns:
    A list of tuples containing the suite and absolute path.
    Ex. ('content_unittests',
          '/tmp/chrome/src/out/Debug/content_unittests_apk/'
          'content_unittests-debug.apk')
  """
  test_suite_dir = os.path.join(cmd_helper.OutDirectory.get(), build_type)
  if option_test_suite:
    all_test_suites = [option_test_suite]
  else:
    all_test_suites = gtest_config.STABLE_TEST_SUITES

  if exe:
    qualified_test_suites = [os.path.join(test_suite_dir, t)
                             for t in all_test_suites]
  else:
    # out/(Debug|Release)/$SUITE_apk/$SUITE-debug.apk
    qualified_test_suites = [os.path.join(test_suite_dir,
                                          t + '_apk',
                                          t + '-debug.apk')
                             for t in all_test_suites]
  for t, q in zip(all_test_suites, qualified_test_suites):
    if not os.path.exists(q):
      raise Exception('Test suite %s not found in %s.\n'
                      'Supported test suites:\n %s\n'
                      'Ensure it has been built.\n' %
                      (t, q, gtest_config.STABLE_TEST_SUITES))
  return zip(all_test_suites, qualified_test_suites)


def _RunATestSuite(options, suite_name):
  """Run a single test suite.

  Helper for Dispatch() to allow stop/restart of the emulator across
  test bundles.  If using the emulator, we start it on entry and stop
  it on exit.

  Args:
    options: options for running the tests.
    suite_name: name of the test suite being run.

  Returns:
    0 if successful, number of failing tests otherwise.
  """
  step_name = os.path.basename(options.test_suite).replace('-debug.apk', '')
  attached_devices = []
  buildbot_emulators = []

  if options.use_emulator:
    buildbot_emulators = emulator.LaunchEmulators(options.emulator_count,
                                                  wait_for_boot=True)
    attached_devices = [e.device for e in buildbot_emulators]
  elif options.test_device:
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

  if options.gtest_filter:
    logging.warning('Sharding is not possible with these configurations.')
    attached_devices = [attached_devices[0]]

  sharder = test_sharder.TestSharder(
      attached_devices,
      options.test_suite,
      options.gtest_filter,
      options.test_arguments,
      options.timeout,
      options.cleanup_test_files,
      options.tool,
      options.build_type,
      options.webkit)

  test_results = sharder.RunShardedTests()
  test_results.LogFull(
      test_type='Unit test',
      test_package=suite_name,
      build_type=options.build_type,
      flakiness_server=options.flakiness_dashboard_server)
  test_results.PrintAnnotation()

  for buildbot_emulator in buildbot_emulators:
    buildbot_emulator.Shutdown()

  return len(test_results.GetAllBroken())


def _ListTestSuites():
  """Display a list of available test suites."""
  print 'Available test suites are:'
  for test_suite in gtest_config.STABLE_TEST_SUITES:
    print test_suite


def Dispatch(options):
  """Dispatches the tests, sharding if possible.

  If options.use_emulator is True, all tests will be run in new emulator
  instance.

  Args:
    options: options for running the tests.

  Returns:
    0 if successful, number of failing tests otherwise.
  """
  if options.test_suite == 'help':
    _ListTestSuites()
    return 0

  if options.use_xvfb:
    framebuffer = xvfb.Xvfb()
    framebuffer.Start()

  all_test_suites = _FullyQualifiedTestSuites(options.exe, options.test_suite,
                                             options.build_type)
  failures = 0
  for suite_name, suite_path in all_test_suites:
    # Give each test suite its own copy of options.
    test_options = copy.deepcopy(options)
    test_options.test_suite = suite_path
    failures += _RunATestSuite(test_options, suite_name)

  if options.use_xvfb:
    framebuffer.Stop()
  return failures
