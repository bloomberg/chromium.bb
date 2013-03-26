# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import fnmatch
import logging
import os

from pylib import android_commands
from pylib import cmd_helper
from pylib import constants
from pylib import ports
from pylib.base import shard
from pylib.utils import emulator
from pylib.utils import report_results
from pylib.utils import xvfb

import gtest_config
import test_runner


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
  def GetQualifiedSuite(suite):
    if suite.is_suite_exe:
      relpath = suite.name
    else:
      # out/(Debug|Release)/$SUITE_apk/$SUITE-debug.apk
      relpath = os.path.join(suite.name + '_apk', suite.name + '-debug.apk')
    return suite.name, os.path.join(test_suite_dir, relpath)

  test_suite_dir = os.path.join(cmd_helper.OutDirectory.get(), build_type)
  if option_test_suite:
    all_test_suites = [gtest_config.Suite(exe, option_test_suite)]
  else:
    all_test_suites = gtest_config.STABLE_TEST_SUITES

  # List of tuples (suite_name, suite_path)
  qualified_test_suites = map(GetQualifiedSuite, all_test_suites)

  for t, q in qualified_test_suites:
    if not os.path.exists(q):
      raise Exception('Test suite %s not found in %s.\n'
                      'Supported test suites:\n %s\n'
                      'Ensure it has been built.\n' %
                      (t, q, gtest_config.STABLE_TEST_SUITES))
  return qualified_test_suites


def GetTestsFromDevice(runner):
  """Get a list of tests from a device, excluding disabled tests.

  Args:
    runner: a TestRunner.
  """
  # The executable/apk needs to be copied before we can call GetAllTests.
  runner.test_package.StripAndCopyExecutable()
  all_tests = runner.test_package.GetAllTests()
  # Only includes tests that do not have any match in the disabled list.
  disabled_list = runner.GetDisabledTests()
  return filter(lambda t: not any([fnmatch.fnmatch(t, disabled_pattern)
                                   for disabled_pattern in disabled_list]),
                all_tests)


def GetAllEnabledTests(runner_factory, devices):
  """Get all enabled tests.

  Obtains a list of enabled tests from the test package on the device,
  then filters it again using the disabled list on the host.

  Args:
    runner_factory: callable that takes a devices and returns a TestRunner.
    devices: list of devices.

  Returns:
    List of all enabled tests.

  Raises Exception if all devices failed.
  """
  for device in devices:
    try:
      logging.info('Obtaining tests from %s', device)
      runner = runner_factory(device, 0)
      return GetTestsFromDevice(runner)
    except Exception as e:
      logging.warning('Failed obtaining tests from %s with exception: %s',
                      device, e)
  raise Exception('No device available to get the list of tests.')


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

  # Constructs a new TestRunner with the current options.
  def RunnerFactory(device, shard_index):
    return test_runner.TestRunner(
        device,
        options.test_suite,
        options.test_arguments,
        options.timeout,
        options.cleanup_test_files,
        options.tool,
        options.build_type,
        options.webkit,
        constants.GTEST_TEST_PACKAGE_NAME,
        constants.GTEST_TEST_ACTIVITY_NAME,
        constants.GTEST_COMMAND_LINE_FILE)

  # Get tests and split them up based on the number of devices.
  if options.gtest_filter:
    all_tests = [t for t in options.gtest_filter.split(':') if t]
  else:
    all_tests = GetAllEnabledTests(RunnerFactory, attached_devices)
  num_devices = len(attached_devices)
  tests = [':'.join(all_tests[i::num_devices]) for i in xrange(num_devices)]
  tests = [t for t in tests if t]

  # Run tests.
  test_results = shard.ShardAndRunTests(RunnerFactory, attached_devices, tests,
                                        options.build_type)

  report_results.LogFull(
      results=test_results,
      test_type='Unit test',
      test_package=suite_name,
      build_type=options.build_type,
      flakiness_server=options.flakiness_dashboard_server)
  report_results.PrintAnnotation(test_results)

  for buildbot_emulator in buildbot_emulators:
    buildbot_emulator.Shutdown()

  return len(test_results.GetNotPass())


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
