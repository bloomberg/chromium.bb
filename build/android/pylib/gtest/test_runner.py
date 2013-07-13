# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os

from pylib import android_commands
from pylib import constants
from pylib.android_commands import errors
from pylib.base import base_test_result
from pylib.base import base_test_runner
from pylib.utils import run_tests_helper

import test_package_apk
import test_package_executable


# We're moving to using isolate files instead of harcoding
# dependencies here. Look at the TODO in dispatch.py.
def _GetDataFilesForTestSuite(test_suite_basename):
  """Returns a list of data files/dirs needed by the test suite.

  Args:
    test_suite_basename: The test suite basename (e.g. base_unittests).

  Returns:
    A list of test file and directory paths.
  """
  # Ideally, we'd just push all test data. However, it has >100MB, and a lot
  # of the files are not relevant (some are used for browser_tests, others for
  # features not supported, etc..).
  if test_suite_basename == 'modules_unittests':
    return [
        'resources',
        'data',
    ]
  return []


def _TestSuiteRequiresMockTestServer(test_suite_basename):
  """Returns True if the test suite requires mock test server."""
  tests_require_net_test_server = ['unit_tests', 'net_unittests',
                                   'content_unittests',
                                   'content_browsertests']
  return (test_suite_basename in
          tests_require_net_test_server)


class TestRunner(base_test_runner.BaseTestRunner):
  """Single test suite attached to a single device.

  Args:
    device: Device to run the tests.
    test_suite: A specific test suite to run, empty to run all.
    test_arguments: Additional arguments to pass to the test binary.
    timeout: Timeout for each test.
    cleanup_test_files: Whether or not to cleanup test files on device.
    tool_name: Name of the Valgrind tool.
    build_type: 'Release' or 'Debug'.
    in_webkit_checkout: Whether the suite is being run from a WebKit checkout.
    push_deps: If True, push all dependencies to the device.
    test_apk_package_name: Apk package name for tests running in APKs.
    test_activity_name: Test activity to invoke for APK tests.
    command_line_file: Filename to use to pass arguments to tests.
    deps_dir: The path to the dependency dir on the host to push to the device.
  """

  def __init__(self, device, test_suite, test_arguments, timeout,
               cleanup_test_files, tool_name, build_type,
               in_webkit_checkout, push_deps, test_apk_package_name=None,
               test_activity_name=None, command_line_file=None, deps_dir=None):
    super(TestRunner, self).__init__(device, tool_name, build_type, push_deps)
    self._running_on_emulator = self.device.startswith('emulator')
    self._test_arguments = test_arguments
    self.in_webkit_checkout = in_webkit_checkout
    self._cleanup_test_files = cleanup_test_files
    self._deps_dir = deps_dir

    logging.warning('Test suite: ' + test_suite)
    if os.path.splitext(test_suite)[1] == '.apk':
      self.test_package = test_package_apk.TestPackageApk(
          self.adb,
          device,
          test_suite,
          timeout,
          self._cleanup_test_files,
          self.tool,
          test_apk_package_name,
          test_activity_name,
          command_line_file)
    else:
      # Put a copy into the android out/target directory, to allow stack trace
      # generation.
      symbols_dir = os.path.join(constants.DIR_SOURCE_ROOT, 'out', build_type,
                                 'lib.target')
      self.test_package = test_package_executable.TestPackageExecutable(
          self.adb,
          device,
          test_suite,
          timeout,
          self._cleanup_test_files,
          self.tool,
          symbols_dir)

  #override
  def InstallTestPackage(self):
    self.test_package.StripAndCopyExecutable()

  #override
  def PushDataDeps(self):
    self.adb.WaitForSdCardReady(20)
    self.tool.CopyFiles()
    if self.test_package.test_suite_basename == 'webkit_unit_tests':
      self.PushWebKitUnitTestsData()
      return

    if not self._deps_dir:
      logging.info('Did not find an isolate file for the test suite.')
      for p in _GetDataFilesForTestSuite(self.test_package.test_suite_basename):
        self.adb.PushIfNeeded(
            os.path.join(constants.DIR_SOURCE_ROOT, p),
            os.path.join(self.adb.GetExternalStorage(), p))
      return

    device_dir = self.adb.GetExternalStorage()
    # TODO(frankf): linux_dumper_unittest_helper needs to be in the same dir
    # as breakpad_unittests exe. Find a better way to do this.
    if self.test_package.test_suite_basename == 'breakpad_unittests':
      device_dir = constants.TEST_EXECUTABLE_DIR
    for p in os.listdir(self._deps_dir):
      self.adb.PushIfNeeded(
          os.path.join(self._deps_dir, p),
          os.path.join(device_dir, p))

  def PushWebKitUnitTestsData(self):
    """Pushes the webkit_unit_tests data files to the device.

    The path of this directory is different when the suite is being run as
    part of a WebKit check-out.
    """
    webkit_src = os.path.join(constants.DIR_SOURCE_ROOT, 'third_party',
                              'WebKit')
    if self.in_webkit_checkout:
      webkit_src = os.path.join(constants.DIR_SOURCE_ROOT, '..', '..', '..')

    self.adb.PushIfNeeded(
        os.path.join(webkit_src, 'Source/WebKit/chromium/tests/data'),
        os.path.join(
            self.adb.GetExternalStorage(),
            'third_party/WebKit/Source/WebKit/chromium/tests/data'))
    self.adb.PushIfNeeded(
        os.path.join(constants.DIR_SOURCE_ROOT,
                     'third_party/hyphen/hyph_en_US.dic'),
        os.path.join(self.adb.GetExternalStorage(),
                     'third_party/hyphen/hyph_en_US.dic'))

  # TODO(craigdh): There is no reason for this to be part of TestRunner.
  def GetDisabledTests(self):
    """Returns a list of disabled tests.

    Returns:
      A list of disabled tests obtained from 'filter' subdirectory.
    """
    gtest_filter_base_path = os.path.join(
        os.path.abspath(os.path.dirname(__file__)),
        'filter',
        self.test_package.test_suite_basename)
    disabled_tests = run_tests_helper.GetExpectations(
       gtest_filter_base_path + '_disabled')
    if self._running_on_emulator:
      # Append emulator's filter file.
      disabled_tests.extend(run_tests_helper.GetExpectations(
          gtest_filter_base_path + '_emulator_additional_disabled'))
    return disabled_tests

  #override
  def RunTest(self, test):
    test_results = base_test_result.TestRunResults()
    if not test:
      return test_results, None

    try:
      self.test_package.ClearApplicationState()
      self.test_package.CreateTestRunnerScript(test, self._test_arguments)
      test_results = self.test_package.RunTestsAndListResults()
    finally:
      self.CleanupSpawningServerState()
    # Calculate unknown test results.
    all_tests = set(test.split(':'))
    all_tests_ran = set([t.GetName() for t in test_results.GetAll()])
    unknown_tests = all_tests - all_tests_ran
    test_results.AddResults(
        [base_test_result.BaseTestResult(t, base_test_result.ResultType.UNKNOWN)
         for t in unknown_tests])
    retry = ':'.join([t.GetName() for t in test_results.GetNotPass()])
    return test_results, retry

  #override
  def SetUp(self):
    """Sets up necessary test enviroment for the test suite."""
    super(TestRunner, self).SetUp()
    if _TestSuiteRequiresMockTestServer(self.test_package.test_suite_basename):
      self.LaunchChromeTestServerSpawner()
    self.tool.SetupEnvironment()

  #override
  def TearDown(self):
    """Cleans up the test enviroment for the test suite."""
    self.tool.CleanUpEnvironment()
    if self._cleanup_test_files:
      self.adb.RemovePushedFiles()
    super(TestRunner, self).TearDown()
