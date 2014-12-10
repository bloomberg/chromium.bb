# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Class for running instrumentation tests on a single device."""

import logging
import os
import re
import sys
import time

from pylib import constants
from pylib import flag_changer
from pylib import valgrind_tools
from pylib.base import base_test_result
from pylib.base import base_test_runner
from pylib.device import device_errors
from pylib.instrumentation import json_perf_parser
from pylib.instrumentation import test_result

sys.path.append(os.path.join(constants.DIR_SOURCE_ROOT, 'build', 'util', 'lib',
                             'common'))
import perf_tests_results_helper # pylint: disable=F0401


_PERF_TEST_ANNOTATION = 'PerfTest'


class TestRunner(base_test_runner.BaseTestRunner):
  """Responsible for running a series of tests connected to a single device."""

  _DEVICE_COVERAGE_DIR = 'chrome/test/coverage'
  _HOSTMACHINE_PERF_OUTPUT_FILE = '/tmp/chrome-profile'
  _DEVICE_PERF_OUTPUT_SEARCH_PREFIX = (constants.DEVICE_PERF_OUTPUT_DIR +
                                       '/chrome-profile*')

  def __init__(self, test_options, device, shard_index, test_pkg,
               additional_flags=None):
    """Create a new TestRunner.

    Args:
      test_options: An InstrumentationOptions object.
      device: Attached android device.
      shard_index: Shard index.
      test_pkg: A TestPackage object.
      additional_flags: A list of additional flags to add to the command line.
    """
    super(TestRunner, self).__init__(device, test_options.tool,
                                     test_options.cleanup_test_files)
    self._lighttp_port = constants.LIGHTTPD_RANDOM_PORT_FIRST + shard_index

    self.coverage_device_file = None
    self.coverage_dir = test_options.coverage_dir
    self.coverage_host_file = None
    self.options = test_options
    self.test_pkg = test_pkg
    # Use the correct command line file for the package under test.
    cmdline_file = [a.cmdline_file for a in constants.PACKAGE_INFO.itervalues()
                    if a.test_package == self.test_pkg.GetPackageName()]
    assert len(cmdline_file) < 2, 'Multiple packages have the same test package'
    if len(cmdline_file) and cmdline_file[0]:
      self.flags = flag_changer.FlagChanger(self.device, cmdline_file[0])
      if additional_flags:
        self.flags.AddFlags(additional_flags)
    else:
      self.flags = None

  #override
  def InstallTestPackage(self):
    self.test_pkg.Install(self.device)

  def _GetInstrumentationArgs(self):
    ret = {}
    if self.options.wait_for_debugger:
      ret['debug'] = 'true'
    if self.coverage_dir:
      ret['coverage'] = 'true'
      ret['coverageFile'] = self.coverage_device_file

    return ret

  def _TakeScreenshot(self, test):
    """Takes a screenshot from the device."""
    screenshot_name = os.path.join(constants.SCREENSHOTS_DIR, '%s.png' % test)
    logging.info('Taking screenshot named %s', screenshot_name)
    self.device.TakeScreenshot(screenshot_name)

  def SetUp(self):
    """Sets up the test harness and device before all tests are run."""
    super(TestRunner, self).SetUp()
    if not self.device.HasRoot():
      logging.warning('Unable to enable java asserts for %s, non rooted device',
                      str(self.device))
    else:
      if self.device.SetJavaAsserts(True):
        # TODO(jbudorick) How to best do shell restart after the
        #                 android_commands refactor?
        self.device.RunShellCommand('stop')
        self.device.RunShellCommand('start')

    # We give different default value to launch HTTP server based on shard index
    # because it may have race condition when multiple processes are trying to
    # launch lighttpd with same port at same time.
    self.LaunchTestHttpServer(
        os.path.join(constants.DIR_SOURCE_ROOT), self._lighttp_port)
    if self.flags:
      self.flags.AddFlags(['--disable-fre', '--enable-test-intents'])
      if self.options.device_flags:
        with open(self.options.device_flags) as device_flags_file:
          stripped_flags = (l.strip() for l in device_flags_file)
          self.flags.AddFlags([flag for flag in stripped_flags if flag])

  def TearDown(self):
    """Cleans up the test harness and saves outstanding data from test run."""
    if self.flags:
      self.flags.Restore()
    super(TestRunner, self).TearDown()

  def TestSetup(self, test):
    """Sets up the test harness for running a particular test.

    Args:
      test: The name of the test that will be run.
    """
    self.SetupPerfMonitoringIfNeeded(test)
    self._SetupIndividualTestTimeoutScale(test)
    self.tool.SetupEnvironment()

    if self.flags and self._IsFreTest(test):
      self.flags.RemoveFlags(['--disable-fre'])

    # Make sure the forwarder is still running.
    self._RestartHttpServerForwarderIfNecessary()

    if self.coverage_dir:
      coverage_basename = '%s.ec' % test
      self.coverage_device_file = '%s/%s/%s' % (
          self.device.GetExternalStoragePath(),
          TestRunner._DEVICE_COVERAGE_DIR, coverage_basename)
      self.coverage_host_file = os.path.join(
          self.coverage_dir, coverage_basename)

  def _IsFreTest(self, test):
    """Determines whether a test is a first run experience test.

    Args:
      test: The name of the test to be checked.

    Returns:
      Whether the feature being tested is FirstRunExperience.
    """
    annotations = self.test_pkg.GetTestAnnotations(test)
    return 'FirstRunExperience' == annotations.get('Feature', None)

  def _IsPerfTest(self, test):
    """Determines whether a test is a performance test.

    Args:
      test: The name of the test to be checked.

    Returns:
      Whether the test is annotated as a performance test.
    """
    return _PERF_TEST_ANNOTATION in self.test_pkg.GetTestAnnotations(test)

  def SetupPerfMonitoringIfNeeded(self, test):
    """Sets up performance monitoring if the specified test requires it.

    Args:
      test: The name of the test to be run.
    """
    if not self._IsPerfTest(test):
      return
    self.device.old_interface.Adb().SendCommand(
        'shell rm ' + TestRunner._DEVICE_PERF_OUTPUT_SEARCH_PREFIX)
    self.device.old_interface.StartMonitoringLogcat()

  def TestTeardown(self, test, result):
    """Cleans up the test harness after running a particular test.

    Depending on the options of this TestRunner this might handle performance
    tracking.  This method will only be called if the test passed.

    Args:
      test: The name of the test that was just run.
      result: result for this test.
    """

    self.tool.CleanUpEnvironment()

    # The logic below relies on the test passing.
    if not result or not result.DidRunPass():
      return

    self.TearDownPerfMonitoring(test)

    if self.flags and self._IsFreTest(test):
      self.flags.AddFlags(['--disable-fre'])

    if self.coverage_dir:
      self.device.PullFile(
          self.coverage_device_file, self.coverage_host_file)
      self.device.RunShellCommand(
          'rm -f %s' % self.coverage_device_file)

  def TearDownPerfMonitoring(self, test):
    """Cleans up performance monitoring if the specified test required it.

    Args:
      test: The name of the test that was just run.
    Raises:
      Exception: if there's anything wrong with the perf data.
    """
    if not self._IsPerfTest(test):
      return
    raw_test_name = test.split('#')[1]

    # Wait and grab annotation data so we can figure out which traces to parse
    regex = self.device.old_interface.WaitForLogMatch(
        re.compile(r'\*\*PERFANNOTATION\(' + raw_test_name + r'\)\:(.*)'),
        None)

    # If the test is set to run on a specific device type only (IE: only
    # tablet or phone) and it is being run on the wrong device, the test
    # just quits and does not do anything.  The java test harness will still
    # print the appropriate annotation for us, but will add --NORUN-- for
    # us so we know to ignore the results.
    # The --NORUN-- tag is managed by MainActivityTestBase.java
    if regex.group(1) != '--NORUN--':

      # Obtain the relevant perf data.  The data is dumped to a
      # JSON formatted file.
      json_string = self.device.ReadFile(
          '/data/data/com.google.android.apps.chrome/files/PerfTestData.txt',
          as_root=True)

      if json_string:
        json_string = '\n'.join(json_string)
      else:
        raise Exception('Perf file does not exist or is empty')

      if self.options.save_perf_json:
        json_local_file = '/tmp/chromium-android-perf-json-' + raw_test_name
        with open(json_local_file, 'w') as f:
          f.write(json_string)
        logging.info('Saving Perf UI JSON from test ' +
                     test + ' to ' + json_local_file)

      raw_perf_data = regex.group(1).split(';')

      for raw_perf_set in raw_perf_data:
        if raw_perf_set:
          perf_set = raw_perf_set.split(',')
          if len(perf_set) != 3:
            raise Exception('Unexpected number of tokens in perf annotation '
                            'string: ' + raw_perf_set)

          # Process the performance data
          result = json_perf_parser.GetAverageRunInfoFromJSONString(json_string,
                                                                    perf_set[0])
          perf_tests_results_helper.PrintPerfResult(perf_set[1], perf_set[2],
                                                    [result['average']],
                                                    result['units'])

  def _SetupIndividualTestTimeoutScale(self, test):
    timeout_scale = self._GetIndividualTestTimeoutScale(test)
    valgrind_tools.SetChromeTimeoutScale(self.device, timeout_scale)

  def _GetIndividualTestTimeoutScale(self, test):
    """Returns the timeout scale for the given |test|."""
    annotations = self.test_pkg.GetTestAnnotations(test)
    timeout_scale = 1
    if 'TimeoutScale' in annotations:
      try:
        timeout_scale = int(annotations['TimeoutScale'])
      except ValueError:
        logging.warning('Non-integer value of TimeoutScale ignored. (%s)'
                        % annotations['TimeoutScale'])
    if self.options.wait_for_debugger:
      timeout_scale *= 100
    return timeout_scale

  def _GetIndividualTestTimeoutSecs(self, test):
    """Returns the timeout in seconds for the given |test|."""
    annotations = self.test_pkg.GetTestAnnotations(test)
    if 'Manual' in annotations:
      return 10 * 60 * 60
    if 'IntegrationTest' in annotations:
      return 30 * 60
    if 'External' in annotations:
      return 10 * 60
    if 'EnormousTest' in annotations:
      return 10 * 60
    if 'LargeTest' in annotations or _PERF_TEST_ANNOTATION in annotations:
      return 5 * 60
    if 'MediumTest' in annotations:
      return 3 * 60
    if 'SmallTest' in annotations:
      return 1 * 60

    logging.warn(("Test size not found in annotations for test '{0}', using " +
                  "1 minute for timeout.").format(test))
    return 1 * 60

  def _RunTest(self, test, timeout):
    """Runs a single instrumentation test.

    Args:
      test: Test class/method.
      timeout: Timeout time in seconds.

    Returns:
      The raw output of am instrument as a list of lines.
    """
    extras = self._GetInstrumentationArgs()
    extras['class'] = test
    return self.device.StartInstrumentation(
        '%s/%s' % (self.test_pkg.GetPackageName(), self.options.test_runner),
        raw=True, extras=extras, timeout=timeout, retries=0)

  @staticmethod
  def _ParseAmInstrumentRawOutput(raw_output):
    """Parses the output of an |am instrument -r| call.

    Args:
      raw_output: the output of an |am instrument -r| call as a list of lines
    Returns:
      A 3-tuple containing:
        - the instrumentation code as an integer
        - the instrumentation result as a list of lines
        - the instrumentation statuses received as a list of 2-tuples
          containing:
          - the status code as an integer
          - the bundle dump as a dict mapping string keys to a list of
            strings, one for each line.
    """
    INSTR_STATUS = 'INSTRUMENTATION_STATUS: '
    INSTR_STATUS_CODE = 'INSTRUMENTATION_STATUS_CODE: '
    INSTR_RESULT = 'INSTRUMENTATION_RESULT: '
    INSTR_CODE = 'INSTRUMENTATION_CODE: '

    last = None
    instr_code = None
    instr_result = []
    instr_statuses = []
    bundle = {}
    for line in raw_output:
      if line.startswith(INSTR_STATUS):
        instr_var = line[len(INSTR_STATUS):]
        if '=' in instr_var:
          k, v = instr_var.split('=', 1)
          bundle[k] = [v]
          last = INSTR_STATUS
          last_key = k
        else:
          logging.debug('Unknown "%s" line: %s' % (INSTR_STATUS, line))

      elif line.startswith(INSTR_STATUS_CODE):
        instr_status = line[len(INSTR_STATUS_CODE):]
        instr_statuses.append((int(instr_status), bundle))
        bundle = {}
        last = INSTR_STATUS_CODE

      elif line.startswith(INSTR_RESULT):
        instr_result.append(line[len(INSTR_RESULT):])
        last = INSTR_RESULT

      elif line.startswith(INSTR_CODE):
        instr_code = int(line[len(INSTR_CODE):])
        last = INSTR_CODE

      elif last == INSTR_STATUS:
        bundle[last_key].append(line)

      elif last == INSTR_RESULT:
        instr_result.append(line)

    return (instr_code, instr_result, instr_statuses)

  def _GenerateTestResult(self, test, instr_statuses, start_ms, duration_ms):
    """Generate the result of |test| from |instr_statuses|.

    Args:
      instr_statuses: A list of 2-tuples containing:
        - the status code as an integer
        - the bundle dump as a dict mapping string keys to string values
        Note that this is the same as the third item in the 3-tuple returned by
        |_ParseAmInstrumentRawOutput|.
      start_ms: The start time of the test in milliseconds.
      duration_ms: The duration of the test in milliseconds.
    Returns:
      An InstrumentationTestResult object.
    """
    INSTR_STATUS_CODE_START = 1
    INSTR_STATUS_CODE_OK = 0
    INSTR_STATUS_CODE_ERROR = -1
    INSTR_STATUS_CODE_FAIL = -2

    log = ''
    result_type = base_test_result.ResultType.UNKNOWN

    for status_code, bundle in instr_statuses:
      if status_code == INSTR_STATUS_CODE_START:
        pass
      elif status_code == INSTR_STATUS_CODE_OK:
        bundle_test = '%s#%s' % (
            ''.join(bundle.get('class', [''])),
            ''.join(bundle.get('test', [''])))
        skipped = ''.join(bundle.get('test_skipped', ['']))

        if (test == bundle_test and
            result_type == base_test_result.ResultType.UNKNOWN):
          result_type = base_test_result.ResultType.PASS
        elif skipped.lower() in ('true', '1', 'yes'):
          result_type = base_test_result.ResultType.SKIP
          logging.info('Skipped ' + test)
      else:
        if status_code not in (INSTR_STATUS_CODE_ERROR,
                               INSTR_STATUS_CODE_FAIL):
          logging.info('Unrecognized status code %d. Handling as an error.',
                       status_code)
        result_type = base_test_result.ResultType.FAIL
        if 'stack' in bundle:
          log = '\n'.join(bundle['stack'])
        # Dismiss any error dialogs. Limit the number in case we have an error
        # loop or we are failing to dismiss.
        for _ in xrange(10):
          package = self.device.old_interface.DismissCrashDialogIfNeeded()
          if not package:
            break
          # Assume test package convention of ".test" suffix
          if package in self.test_pkg.GetPackageName():
            result_type = base_test_result.ResultType.CRASH
            break

    return test_result.InstrumentationTestResult(
        test, result_type, start_ms, duration_ms, log=log)

  #override
  def RunTest(self, test):
    results = base_test_result.TestRunResults()
    timeout = (self._GetIndividualTestTimeoutSecs(test) *
               self._GetIndividualTestTimeoutScale(test) *
               self.tool.GetTimeoutScale())
    if (self.device.GetProp('ro.build.version.sdk')
        < constants.ANDROID_SDK_VERSION_CODES.JELLY_BEAN):
      timeout *= 10

    start_ms = 0
    duration_ms = 0
    try:
      self.TestSetup(test)

      time_ms = lambda: int(time.time() * 1000)
      start_ms = time_ms()
      raw_output = self._RunTest(test, timeout)
      duration_ms = time_ms() - start_ms

      # Parse the test output
      _, _, statuses = self._ParseAmInstrumentRawOutput(raw_output)
      result = self._GenerateTestResult(test, statuses, start_ms, duration_ms)
      results.AddResult(result)
    except device_errors.CommandTimeoutError as e:
      results.AddResult(test_result.InstrumentationTestResult(
          test, base_test_result.ResultType.TIMEOUT, start_ms, duration_ms,
          log=str(e) or 'No information'))
    except device_errors.DeviceUnreachableError as e:
      results.AddResult(test_result.InstrumentationTestResult(
          test, base_test_result.ResultType.CRASH, start_ms, duration_ms,
          log=str(e) or 'No information'))
    self.TestTeardown(test, results)
    return (results, None if results.DidRunPass() else test)
