# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Dispatches GTests."""

import copy
import fnmatch
import glob
import logging
import os
import shutil
import sys

from pylib import android_commands
from pylib import cmd_helper
from pylib import constants
from pylib import ports
from pylib.base import base_test_result
from pylib.base import shard
from pylib.utils import emulator
from pylib.utils import report_results
from pylib.utils import xvfb

import gtest_config
import test_runner

sys.path.insert(0,
                os.path.join(constants.DIR_SOURCE_ROOT, 'build', 'util', 'lib'))
from common import unittest_util


_ISOLATE_FILE_PATHS = {
    'base_unittests': 'base/base_unittests.isolate',
    'breakpad_unittests': 'breakpad/breakpad_unittests.isolate',
    'cc_perftests': 'cc/cc_perftests.isolate',
    'components_unittests': 'components/components_unittests.isolate',
    'content_browsertests': 'content/content_browsertests.isolate',
    'content_unittests': 'content/content_unittests.isolate',
    'media_unittests': 'media/media_unittests.isolate',
    'modules_unittests': 'third_party/webrtc/modules/modules_unittests.isolate',
    'net_unittests': 'net/net_unittests.isolate',
    'ui_unittests': 'ui/ui_unittests.isolate',
    'unit_tests': 'chrome/unit_tests.isolate',
}

# Used for filtering large data deps at a finer grain than what's allowed in
# isolate files since pushing deps to devices is expensive.
# Wildcards are allowed.
_DEPS_EXCLUSION_LIST = [
    'chrome/test/data/extensions/api_test',
    'chrome/test/data/extensions/secure_shell',
    'chrome/test/data/firefox*',
    'chrome/test/data/gpu',
    'chrome/test/data/image_decoding',
    'chrome/test/data/import',
    'chrome/test/data/page_cycler',
    'chrome/test/data/perf',
    'chrome/test/data/pyauto_private',
    'chrome/test/data/safari_import',
    'chrome/test/data/scroll',
    'chrome/test/data/third_party',
    'third_party/hunspell_dictionaries/*.dic',
    # crbug.com/258690
    'webkit/data/bmp_decoder',
    'webkit/data/ico_decoder',
]

_ISOLATE_SCRIPT = os.path.join(
    constants.DIR_SOURCE_ROOT, 'tools', 'swarm_client', 'isolate.py')


def _GenerateDepsDirUsingIsolate(test_suite, build_type):
  """Generate the dependency dir for the test suite using isolate.

  Args:
    test_suite: Name of the test suite (e.g. base_unittests).
    build_type: Release/Debug
  """
  product_dir = os.path.join(cmd_helper.OutDirectory.get(), build_type)
  assert os.path.isabs(product_dir)

  if os.path.isdir(constants.ISOLATE_DEPS_DIR):
    shutil.rmtree(constants.ISOLATE_DEPS_DIR)

  isolate_rel_path = _ISOLATE_FILE_PATHS.get(test_suite)
  if not isolate_rel_path:
    logging.info('Did not find an isolate file for the test suite.')
    return

  isolate_abs_path = os.path.join(constants.DIR_SOURCE_ROOT, isolate_rel_path)
  isolated_abs_path = os.path.join(
      product_dir, '%s.isolated' % test_suite)
  assert os.path.exists(isolate_abs_path)
  isolate_cmd = [
      'python', _ISOLATE_SCRIPT,
      'remap',
      '--isolate', isolate_abs_path,
      '--isolated', isolated_abs_path,
      '-V', 'PRODUCT_DIR=%s' % product_dir,
      '-V', 'OS=android',
      '--outdir', constants.ISOLATE_DEPS_DIR,
  ]
  assert not cmd_helper.RunCmd(isolate_cmd)

  # We're relying on the fact that timestamps are preserved
  # by the remap command (hardlinked). Otherwise, all the data
  # will be pushed to the device once we move to using time diff
  # instead of md5sum. Perform a sanity check here.
  for root, _, filenames in os.walk(constants.ISOLATE_DEPS_DIR):
    if filenames:
      linked_file = os.path.join(root, filenames[0])
      orig_file = os.path.join(
          constants.DIR_SOURCE_ROOT,
          os.path.relpath(linked_file, constants.ISOLATE_DEPS_DIR))
      if os.stat(linked_file).st_ino == os.stat(orig_file).st_ino:
        break
      else:
        raise Exception('isolate remap command did not use hardlinks.')

  # Delete excluded files as defined by _DEPS_EXCLUSION_LIST.
  old_cwd = os.getcwd()
  try:
    os.chdir(constants.ISOLATE_DEPS_DIR)
    excluded_paths = [x for y in _DEPS_EXCLUSION_LIST for x in glob.glob(y)]
    if excluded_paths:
      logging.info('Excluding the following from dependency list: %s',
                   excluded_paths)
    for p in excluded_paths:
      if os.path.isdir(p):
        shutil.rmtree(p)
      else:
        os.remove(p)
  finally:
    os.chdir(old_cwd)

  # On Android, all pak files need to be in the top-level 'paks' directory.
  paks_dir = os.path.join(constants.ISOLATE_DEPS_DIR, 'paks')
  os.mkdir(paks_dir)
  for root, _, filenames in os.walk(os.path.join(constants.ISOLATE_DEPS_DIR,
                                                 'out')):
    for filename in fnmatch.filter(filenames, '*.pak'):
      shutil.move(os.path.join(root, filename), paks_dir)

  # Move everything in PRODUCT_DIR to top level.
  deps_product_dir = os.path.join(constants.ISOLATE_DEPS_DIR, 'out', build_type)
  if os.path.isdir(deps_product_dir):
    for p in os.listdir(deps_product_dir):
      shutil.move(os.path.join(deps_product_dir, p), constants.ISOLATE_DEPS_DIR)
    os.rmdir(deps_product_dir)
    os.rmdir(os.path.join(constants.ISOLATE_DEPS_DIR, 'out'))


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

  Raises:
    Exception: If test suite not found.
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
                      (t, q, [s.name for s in gtest_config.STABLE_TEST_SUITES]))
  return qualified_test_suites


def _GetDisabledTestsFilterFromFile(test_suite):
  """Returns a gtest filter based on the *_disabled file.

  Args:
    test_suite: Name of the test suite (e.g. base_unittests).

  Returns:
    A gtest filter which excludes disabled tests.
    Example: '*-StackTrace.*:StringPrintfTest.StringPrintfMisc'
  """
  filter_file_path = os.path.join(
      os.path.abspath(os.path.dirname(__file__)),
      'filter', '%s_disabled' % test_suite)

  if not filter_file_path or not os.path.exists(filter_file_path):
    logging.info('No filter file found at %s', filter_file_path)
    return '*'

  filters = [x for x in [x.strip() for x in file(filter_file_path).readlines()]
             if x and x[0] != '#']
  disabled_filter = '*-%s' % ':'.join(filters)
  logging.info('Applying filter "%s" obtained from %s',
               disabled_filter, filter_file_path)
  return disabled_filter


def _GetTestsFromDevice(runner_factory, devices):
  """Get a list of tests from a device.

  Args:
    runner_factory: callable that takes a device and index and returns a
      TestRunner object.
    devices: List of devices.

  Returns:
    All the tests in the test suite.
  """
  for device in devices:
    try:
      logging.info('Obtaining tests from %s', device)
      runner = runner_factory(device, 0)
      runner.test_package.Install()
      return runner.test_package.GetAllTests()
    except (android_commands.errors.WaitForResponseTimedOutError,
            android_commands.errors.DeviceUnresponsiveError), e:
      logging.warning('Failed obtaining tests from %s with exception: %s',
                      device, e)
  raise Exception('No device available to get the list of tests.')


def _FilterTestsUsingPrefixes(all_tests, pre=False, manual=False):
  """Removes tests with disabled prefixes.

  Args:
    all_tests: List of tests to filter.
    pre: If True, include tests with _PRE prefix.
    manual: If True, include tests with _MANUAL prefix.

  Returns:
    List of tests remaining.
  """
  filtered_tests = []
  filter_prefixes = ['DISABLED_', 'FLAKY_', 'FAILS_']

  if not pre:
    filter_prefixes.append('PRE_')

  if not manual:
    filter_prefixes.append('MANUAL_')

  for t in all_tests:
    test_case, test = t.split('.', 1)
    if not any([test_case.startswith(prefix) or test.startswith(prefix) for
                prefix in filter_prefixes]):
      filtered_tests.append(t)
  return filtered_tests


def GetTestsFiltered(test_suite, gtest_filter, runner_factory, devices):
  """Get all tests in the suite and filter them.

  Obtains a list of tests from the test package on the device, and
  applies the following filters in order:
    1. Remove tests with disabled prefixes.
    2. Remove tests specified in the *_disabled files in the 'filter' dir
    3. Applies |gtest_filter|.

  Args:
    test_suite: Name of the test suite (e.g. base_unittests).
    gtest_filter: A filter including negative and/or positive patterns.
    runner_factory: callable that takes a device and index and returns a
      TestRunner object.
    devices: List of devices.

  Returns:
    List of tests remaining.
  """
  tests = _GetTestsFromDevice(runner_factory, devices)
  tests = _FilterTestsUsingPrefixes(
      tests, bool(gtest_filter), bool(gtest_filter))
  tests = unittest_util.FilterTestNames(
      tests, _GetDisabledTestsFilterFromFile(test_suite))

  if gtest_filter:
    tests = unittest_util.FilterTestNames(tests, gtest_filter)

  return tests


def _RunATestSuite(options, suite_name):
  """Run a single test suite.

  Helper for Dispatch() to allow stop/restart of the emulator across
  test bundles.  If using the emulator, we start it on entry and stop
  it on exit.

  Args:
    options: options for running the tests.
    suite_name: name of the test suite being run.

  Returns:
    A tuple of (base_test_result.TestRunResult object, exit code).

  Raises:
    Exception: For various reasons including device failure or failing to reset
        the test server port.
  """
  attached_devices = []
  buildbot_emulators = []

  if options.use_emulator:
    buildbot_emulators = emulator.LaunchEmulators(options.emulator_count,
                                                  options.abi,
                                                  wait_for_boot=True)
    attached_devices = [e.device for e in buildbot_emulators]
  elif options.test_device:
    attached_devices = [options.test_device]
  else:
    attached_devices = android_commands.GetAttachedDevices()

  if not attached_devices:
    raise Exception('A device must be attached and online.')

  # Reset the test port allocation. It's important to do it before starting
  # to dispatch any tests.
  if not ports.ResetTestServerPortAllocation():
    raise Exception('Failed to reset test server port.')

  _GenerateDepsDirUsingIsolate(suite_name, options.build_type)

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
        options.push_deps,
        constants.GTEST_TEST_PACKAGE_NAME,
        constants.GTEST_TEST_ACTIVITY_NAME,
        constants.GTEST_COMMAND_LINE_FILE)

  # Get tests and split them up based on the number of devices.
  tests = GetTestsFiltered(suite_name, options.test_filter,
                           RunnerFactory, attached_devices)
  num_devices = len(attached_devices)
  tests = [':'.join(tests[i::num_devices]) for i in xrange(num_devices)]
  tests = [t for t in tests if t]

  # Run tests.
  test_results, exit_code = shard.ShardAndRunTests(
      RunnerFactory, attached_devices, tests, options.build_type,
      test_timeout=None, num_retries=options.num_retries)

  report_results.LogFull(
      results=test_results,
      test_type='Unit test',
      test_package=suite_name,
      build_type=options.build_type,
      flakiness_server=options.flakiness_dashboard_server)

  if os.path.isdir(constants.ISOLATE_DEPS_DIR):
    shutil.rmtree(constants.ISOLATE_DEPS_DIR)

  for buildbot_emulator in buildbot_emulators:
    buildbot_emulator.Shutdown()

  return (test_results, exit_code)


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
    base_test_result.TestRunResults object with the results of running the tests
  """
  results = base_test_result.TestRunResults()

  if options.test_suite == 'help':
    _ListTestSuites()
    return (results, 0)

  if options.use_xvfb:
    framebuffer = xvfb.Xvfb()
    framebuffer.Start()

  all_test_suites = _FullyQualifiedTestSuites(options.exe, options.test_suite,
                                              options.build_type)
  exit_code = 0
  for suite_name, suite_path in all_test_suites:
    # Give each test suite its own copy of options.
    test_options = copy.deepcopy(options)
    test_options.test_suite = suite_path
    test_results, test_exit_code = _RunATestSuite(test_options, suite_name)
    results.AddTestRunResults(test_results)
    if test_exit_code and exit_code != constants.ERROR_EXIT_CODE:
      exit_code = test_exit_code

  if options.use_xvfb:
    framebuffer.Stop()

  return (results, exit_code)
