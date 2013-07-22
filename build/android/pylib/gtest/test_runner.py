# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import re

from pylib import android_commands
from pylib import constants
from pylib import pexpect
from pylib.base import base_test_result
from pylib.base import base_test_runner
from pylib.utils import run_tests_helper

import test_package_apk
import test_package_exe


def _TestSuiteRequiresMockTestServer(suite_basename):
  """Returns True if the test suite requires mock test server."""
  tests_require_net_test_server = ['unit_tests', 'net_unittests',
                                   'content_unittests',
                                   'content_browsertests']
  return (suite_basename in
          tests_require_net_test_server)


class TestRunner(base_test_runner.BaseTestRunner):
  def __init__(self, device, suite_name, test_arguments, timeout,
               cleanup_test_files, tool_name, build_type,
               in_webkit_checkout, push_deps, test_apk_package_name=None,
               test_activity_name=None, command_line_file=None):
    """Single test suite attached to a single device.

    Args:
      device: Device to run the tests.
      suite_name: A specific test suite to run, empty to run all.
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
    """
    super(TestRunner, self).__init__(device, tool_name, build_type, push_deps,
                                     cleanup_test_files)
    self._test_arguments = test_arguments
    self.in_webkit_checkout = in_webkit_checkout
    if timeout == 0:
      timeout = 60
    # On a VM (e.g. chromium buildbots), this timeout is way too small.
    if os.environ.get('BUILDBOT_SLAVENAME'):
      timeout = timeout * 2
    self.timeout = timeout * self.tool.GetTimeoutScale()

    logging.warning('Test suite: ' + str(suite_name))
    if os.path.splitext(suite_name)[1] == '.apk':
      self.test_package = test_package_apk.TestPackageApk(
          self.adb,
          device,
          suite_name,
          self.tool,
          test_apk_package_name,
          test_activity_name,
          command_line_file)
    else:
      # Put a copy into the android out/target directory, to allow stack trace
      # generation.
      symbols_dir = os.path.join(constants.DIR_SOURCE_ROOT, 'out', build_type,
                                 'lib.target')
      self.test_package = test_package_exe.TestPackageExecutable(
          self.adb,
          device,
          suite_name,
          self.tool,
          symbols_dir)

  #override
  def InstallTestPackage(self):
    self.test_package.Install()

  #override
  def PushDataDeps(self):
    self.adb.WaitForSdCardReady(20)
    self.tool.CopyFiles()
    if self.test_package.suite_basename == 'webkit_unit_tests':
      self.PushWebKitUnitTestsData()
      return

    if os.path.exists(constants.ISOLATE_DEPS_DIR):
      device_dir = self.adb.GetExternalStorage()
      # TODO(frankf): linux_dumper_unittest_helper needs to be in the same dir
      # as breakpad_unittests exe. Find a better way to do this.
      if self.test_package.suite_basename == 'breakpad_unittests':
        device_dir = constants.TEST_EXECUTABLE_DIR
      for p in os.listdir(constants.ISOLATE_DEPS_DIR):
        self.adb.PushIfNeeded(
            os.path.join(constants.ISOLATE_DEPS_DIR, p),
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
        os.path.join(webkit_src, 'Source/web/tests/data'),
        os.path.join(
            self.adb.GetExternalStorage(),
            'third_party/WebKit/Source/web/tests/data'))
    self.adb.PushIfNeeded(
        os.path.join(constants.DIR_SOURCE_ROOT,
                     'third_party/hyphen/hyph_en_US.dic'),
        os.path.join(self.adb.GetExternalStorage(),
                     'third_party/hyphen/hyph_en_US.dic'))

  def _ParseTestOutput(self, p):
    """Process the test output.

    Args:
      p: An instance of pexpect spawn class.

    Returns:
      A TestRunResults object.
    """
    results = base_test_result.TestRunResults()

    # Test case statuses.
    re_run = re.compile('\[ RUN      \] ?(.*)\r\n')
    re_fail = re.compile('\[  FAILED  \] ?(.*)\r\n')
    re_ok = re.compile('\[       OK \] ?(.*?) .*\r\n')

    # Test run statuses.
    re_passed = re.compile('\[  PASSED  \] ?(.*)\r\n')
    re_runner_fail = re.compile('\[ RUNNER_FAILED \] ?(.*)\r\n')
    # Signal handlers are installed before starting tests
    # to output the CRASHED marker when a crash happens.
    re_crash = re.compile('\[ CRASHED      \](.*)\r\n')

    log = ''
    try:
      while True:
        full_test_name = None
        found = p.expect([re_run, re_passed, re_runner_fail],
                         timeout=self.timeout)
        if found == 1:  # re_passed
          break
        elif found == 2:  # re_runner_fail
          break
        else:  # re_run
          full_test_name = p.match.group(1).replace('\r', '')
          found = p.expect([re_ok, re_fail, re_crash], timeout=self.timeout)
          log = p.before.replace('\r', '')
          if found == 0:  # re_ok
            if full_test_name == p.match.group(1).replace('\r', ''):
              results.AddResult(base_test_result.BaseTestResult(
                  full_test_name, base_test_result.ResultType.PASS,
                  log=log))
          elif found == 2:  # re_crash
            results.AddResult(base_test_result.BaseTestResult(
                full_test_name, base_test_result.ResultType.CRASH,
                log=log))
            break
          else:  # re_fail
            results.AddResult(base_test_result.BaseTestResult(
                full_test_name, base_test_result.ResultType.FAIL, log=log))
    except pexpect.EOF:
      logging.error('Test terminated - EOF')
      # We're here because either the device went offline, or the test harness
      # crashed without outputting the CRASHED marker (crbug.com/175538).
      if not self.adb.IsOnline():
        raise android_commands.errors.DeviceUnresponsiveError(
            'Device %s went offline.' % self.device)
      if full_test_name:
        results.AddResult(base_test_result.BaseTestResult(
            full_test_name, base_test_result.ResultType.CRASH,
            log=p.before.replace('\r', '')))
    except pexpect.TIMEOUT:
      logging.error('Test terminated after %d second timeout.',
                    self.timeout)
      if full_test_name:
        results.AddResult(base_test_result.BaseTestResult(
            full_test_name, base_test_result.ResultType.TIMEOUT,
            log=p.before.replace('\r', '')))
    finally:
      p.close()

    ret_code = self.test_package.GetGTestReturnCode()
    if ret_code:
      logging.critical(
          'gtest exit code: %d\npexpect.before: %s\npexpect.after: %s',
          ret_code, p.before, p.after)

    return results

  #override
  def RunTest(self, test):
    test_results = base_test_result.TestRunResults()
    if not test:
      return test_results, None

    try:
      self.test_package.ClearApplicationState()
      self.test_package.CreateCommandLineFileOnDevice(
          test, self._test_arguments)
      test_results = self._ParseTestOutput(self.test_package.SpawnTestProcess())
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
    if _TestSuiteRequiresMockTestServer(self.test_package.suite_basename):
      self.LaunchChromeTestServerSpawner()
    self.tool.SetupEnvironment()

  #override
  def TearDown(self):
    """Cleans up the test enviroment for the test suite."""
    self.tool.CleanUpEnvironment()
    super(TestRunner, self).TearDown()
