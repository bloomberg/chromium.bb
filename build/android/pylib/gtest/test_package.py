# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import logging
import os
import re

from pylib import constants
from pylib import pexpect
from pylib.android_commands import errors
from pylib.base import base_test_result
from pylib.perf_tests_helper import PrintPerfResult


class TestPackage(object):
  """A helper base class for both APK and stand-alone executables.

  Args:
    adb: ADB interface the tests are using.
    device: Device to run the tests.
    test_suite: A specific test suite to run, empty to run all.
    timeout: Timeout for each test.
    cleanup_test_files: Whether or not to cleanup test files on device.
    tool: Name of the Valgrind tool.
  """

  def __init__(self, adb, device, test_suite, timeout,
               cleanup_test_files, tool):
    self.adb = adb
    self.device = device
    self.test_suite_full = test_suite
    self.test_suite = os.path.splitext(test_suite)[0]
    self.test_suite_basename = self._GetTestSuiteBaseName()
    self.test_suite_dirname = os.path.dirname(
        self.test_suite.split(self.test_suite_basename)[0])
    self.cleanup_test_files = cleanup_test_files
    self.tool = tool
    if timeout == 0:
      timeout = 60
    # On a VM (e.g. chromium buildbots), this timeout is way too small.
    if os.environ.get('BUILDBOT_SLAVENAME'):
      timeout = timeout * 2
    self.timeout = timeout * self.tool.GetTimeoutScale()

  def ClearApplicationState(self):
    """Clears the application state."""
    raise NotImplementedError('Method must be overriden.')

  def GetDisabledPrefixes(self):
    return ['DISABLED_', 'FLAKY_', 'FAILS_']

  def _ParseGTestListTests(self, all_tests):
    """Parses and filters the raw test lists.

    Args:
    all_tests: The raw test listing with the following format:

      IPCChannelTest.
        SendMessageInChannelConnected
      IPCSyncChannelTest.
        Simple
        DISABLED_SendWithTimeoutMixedOKAndTimeout

    Returns:
      A list of non-disabled tests. For the above raw listing:

      [IPCChannelTest.SendMessageInChannelConnected, IPCSyncChannelTest.Simple]
    """
    ret = []
    current = ''
    disabled_prefixes = self.GetDisabledPrefixes()
    for test in all_tests:
      if not test:
        continue
      if test[0] != ' ' and not test.endswith('.'):
        # Ignore any lines with unexpected format.
        continue
      if test[0] != ' ' and test.endswith('.'):
        current = test
        continue
      if 'YOU HAVE' in test:
        break
      test_name = test[2:]
      if not any([test_name.startswith(x) for x in disabled_prefixes]):
        ret += [current + test_name]
    return ret

  def PushDataAndPakFiles(self):
    external_storage = self.adb.GetExternalStorage()
    if (self.test_suite_basename == 'ui_unittests' or
        self.test_suite_basename == 'unit_tests'):
      self.adb.PushIfNeeded(
          self.test_suite_dirname + '/chrome.pak',
          external_storage + '/paks/chrome.pak')
      self.adb.PushIfNeeded(
          self.test_suite_dirname + '/locales/en-US.pak',
          external_storage + '/paks/en-US.pak')
    if self.test_suite_basename == 'unit_tests':
      self.adb.PushIfNeeded(
          self.test_suite_dirname + '/resources.pak',
          external_storage + '/paks/resources.pak')
      self.adb.PushIfNeeded(
          self.test_suite_dirname + '/chrome_100_percent.pak',
          external_storage + '/paks/chrome_100_percent.pak')
      self.adb.PushIfNeeded(self.test_suite_dirname + '/test_data',
                            external_storage + '/test_data')
    if self.test_suite_basename in ('content_unittests',
                                    'components_unittests'):
      self.adb.PushIfNeeded(
          self.test_suite_dirname + '/content_resources.pak',
          external_storage + '/paks/content_resources.pak')
    if self.test_suite_basename == 'breakpad_unittests':
      self.adb.PushIfNeeded(
          self.test_suite_dirname + '/linux_dumper_unittest_helper',
          constants.TEST_EXECUTABLE_DIR + '/linux_dumper_unittest_helper')
    if self.test_suite_basename == 'content_browsertests':
      self.adb.PushIfNeeded(
          self.test_suite_dirname +
          '/../content_shell/assets/content_shell.pak',
          external_storage + '/paks/content_shell.pak')

  def _WatchTestOutput(self, p):
    """Watches the test output.

    Args:
      p: the process generating output as created by pexpect.spawn.

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
          if found == 0:  # re_ok
            if full_test_name == p.match.group(1).replace('\r', ''):
              results.AddResult(base_test_result.BaseTestResult(
                  full_test_name, base_test_result.ResultType.PASS,
                  log=p.before))
          elif found == 2:  # re_crash
            results.AddResult(base_test_result.BaseTestResult(
                full_test_name, base_test_result.ResultType.CRASH,
                log=p.before))
            break
          else:  # re_fail
            results.AddResult(base_test_result.BaseTestResult(
                full_test_name, base_test_result.ResultType.FAIL, log=p.before))
    except pexpect.EOF:
      logging.error('Test terminated - EOF')
      # We're here because either the device went offline, or the test harness
      # crashed without outputting the CRASHED marker (crbug.com/175538).
      if not self.adb.IsOnline():
        raise errors.DeviceUnresponsiveError('Device %s went offline.' %
                                             self.device)
      if full_test_name:
        results.AddResult(base_test_result.BaseTestResult(
            full_test_name, base_test_result.ResultType.CRASH, log=p.before))
    except pexpect.TIMEOUT:
      logging.error('Test terminated after %d second timeout.',
                    self.timeout)
      if full_test_name:
        results.AddResult(base_test_result.BaseTestResult(
            full_test_name, base_test_result.ResultType.TIMEOUT, log=p.before))
    finally:
      p.close()

    ret_code = self._GetGTestReturnCode()
    if ret_code:
      logging.critical(
          'gtest exit code: %d\npexpect.before: %s\npexpect.after: %s',
          ret_code, p.before, p.after)

    return results
