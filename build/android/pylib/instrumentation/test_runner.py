# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs the Java tests. See more information on run_instrumentation_tests.py."""

import logging
import os
import re
import shutil
import sys
import time

from pylib import android_commands
from pylib import cmd_helper
from pylib import constants
from pylib import forwarder
from pylib import json_perf_parser
from pylib import perf_tests_helper
from pylib import valgrind_tools
from pylib.base import base_test_result
from pylib.base import base_test_runner

import test_result


_PERF_TEST_ANNOTATION = 'PerfTest'


class TestRunner(base_test_runner.BaseTestRunner):
  """Responsible for running a series of tests connected to a single device."""

  _DEVICE_DATA_DIR = 'chrome/test/data'
  _EMMA_JAR = os.path.join(os.environ.get('ANDROID_BUILD_TOP', ''),
                           'external/emma/lib/emma.jar')
  _COVERAGE_MERGED_FILENAME = 'unittest_coverage.es'
  _COVERAGE_WEB_ROOT_DIR = os.environ.get('EMMA_WEB_ROOTDIR')
  _COVERAGE_FILENAME = 'coverage.ec'
  _COVERAGE_RESULT_PATH = ('/data/data/com.google.android.apps.chrome/files/' +
                           _COVERAGE_FILENAME)
  _COVERAGE_META_INFO_PATH = os.path.join(os.environ.get('ANDROID_BUILD_TOP',
                                                         ''),
                                          'out/target/common/obj/APPS',
                                          'Chrome_intermediates/coverage.em')
  _HOSTMACHINE_PERF_OUTPUT_FILE = '/tmp/chrome-profile'
  _DEVICE_PERF_OUTPUT_SEARCH_PREFIX = (constants.DEVICE_PERF_OUTPUT_DIR +
                                       '/chrome-profile*')
  _DEVICE_HAS_TEST_FILES = {}

  def __init__(self, options, device, shard_index, coverage, test_pkg,
               ports_to_forward, is_uiautomator_test=False):
    """Create a new TestRunner.

    Args:
      options: An options object with the following required attributes:
      -  build_type: 'Release' or 'Debug'.
      -  install_apk: Re-installs the apk if opted.
      -  save_perf_json: Whether or not to save the JSON file from UI perf
            tests.
      -  screenshot_failures: Take a screenshot for a test failure
      -  tool: Name of the Valgrind tool.
      -  wait_for_debugger: blocks until the debugger is connected.
      -  disable_assertions: Whether to disable java assertions on the device.
      device: Attached android device.
      shard_index: Shard index.
      coverage: Collects coverage information if opted.
      test_pkg: A TestPackage object.
      ports_to_forward: A list of port numbers for which to set up forwarders.
                        Can be optionally requested by a test case.
      is_uiautomator_test: Whether this is a uiautomator test.
    Raises:
      Exception: if coverage metadata is not available.
    """
    super(TestRunner, self).__init__(device, options.tool, options.build_type)
    self._lighttp_port = constants.LIGHTTPD_RANDOM_PORT_FIRST + shard_index

    self.build_type = options.build_type
    self.test_data = options.test_data
    self.save_perf_json = options.save_perf_json
    self.screenshot_failures = options.screenshot_failures
    self.wait_for_debugger = options.wait_for_debugger
    self.disable_assertions = options.disable_assertions
    self.coverage = coverage
    self.test_pkg = test_pkg
    self.ports_to_forward = ports_to_forward
    self.is_uiautomator_test = is_uiautomator_test
    if self.is_uiautomator_test:
      self.package_name = options.package_name
    else:
      self.install_apk = options.install_apk

    self.forwarder = None

    if self.coverage:
      if os.path.exists(TestRunner._COVERAGE_MERGED_FILENAME):
        os.remove(TestRunner._COVERAGE_MERGED_FILENAME)
      if not os.path.exists(TestRunner._COVERAGE_META_INFO_PATH):
        raise Exception('FATAL ERROR in ' + sys.argv[0] +
                        ' : Coverage meta info [' +
                        TestRunner._COVERAGE_META_INFO_PATH +
                        '] does not exist.')
      if (not TestRunner._COVERAGE_WEB_ROOT_DIR or
          not os.path.exists(TestRunner._COVERAGE_WEB_ROOT_DIR)):
        raise Exception('FATAL ERROR in ' + sys.argv[0] +
                        ' : Path specified in $EMMA_WEB_ROOTDIR [' +
                        TestRunner._COVERAGE_WEB_ROOT_DIR +
                        '] does not exist.')

  def CopyTestFilesOnce(self):
    """Pushes the test data files to the device. Installs the apk if opted."""
    if TestRunner._DEVICE_HAS_TEST_FILES.get(self.device, False):
      logging.warning('Already copied test files to device %s, skipping.',
                      self.device)
      return
    for dest_host_pair in self.test_data:
      dst_src = dest_host_pair.split(':',1)
      dst_layer = dst_src[0]
      host_src = dst_src[1]
      host_test_files_path = constants.CHROME_DIR + '/' + host_src
      if os.path.exists(host_test_files_path):
        self.adb.PushIfNeeded(host_test_files_path,
                              self.adb.GetExternalStorage() + '/' +
                              TestRunner._DEVICE_DATA_DIR + '/' + dst_layer)
    if self.is_uiautomator_test:
      self.test_pkg.Install(self.adb)
    elif self.install_apk:
      self.test_pkg.Install(self.adb)

    self.tool.CopyFiles()
    TestRunner._DEVICE_HAS_TEST_FILES[self.device] = True

  def SaveCoverageData(self, test):
    """Saves the Emma coverage data before it's overwritten by the next test.

    Args:
      test: the test whose coverage data is collected.
    """
    if not self.coverage:
      return
    if not self.adb.Adb().Pull(TestRunner._COVERAGE_RESULT_PATH,
                               constants.CHROME_DIR):
      logging.error('ERROR: Unable to find file ' +
                    TestRunner._COVERAGE_RESULT_PATH +
                    ' on the device for test ' + test)
    pulled_coverage_file = os.path.join(constants.CHROME_DIR,
                                        TestRunner._COVERAGE_FILENAME)
    if os.path.exists(TestRunner._COVERAGE_MERGED_FILENAME):
      cmd = ['java', '-classpath', TestRunner._EMMA_JAR, 'emma', 'merge',
             '-in', pulled_coverage_file,
             '-in', TestRunner._COVERAGE_MERGED_FILENAME,
             '-out', TestRunner._COVERAGE_MERGED_FILENAME]
      cmd_helper.RunCmd(cmd)
    else:
      shutil.copy(pulled_coverage_file,
                  TestRunner._COVERAGE_MERGED_FILENAME)
    os.remove(pulled_coverage_file)

  def GenerateCoverageReportIfNeeded(self):
    """Uses the Emma to generate a coverage report and a html page."""
    if not self.coverage:
      return
    cmd = ['java', '-classpath', TestRunner._EMMA_JAR,
           'emma', 'report', '-r', 'html',
           '-in', TestRunner._COVERAGE_MERGED_FILENAME,
           '-in', TestRunner._COVERAGE_META_INFO_PATH]
    cmd_helper.RunCmd(cmd)
    new_dir = os.path.join(TestRunner._COVERAGE_WEB_ROOT_DIR,
                           time.strftime('Coverage_for_%Y_%m_%d_%a_%H:%M'))
    shutil.copytree('coverage', new_dir)

    latest_dir = os.path.join(TestRunner._COVERAGE_WEB_ROOT_DIR,
                              'Latest_Coverage_Run')
    if os.path.exists(latest_dir):
      shutil.rmtree(latest_dir)
    os.mkdir(latest_dir)
    webserver_new_index = os.path.join(new_dir, 'index.html')
    webserver_new_files = os.path.join(new_dir, '_files')
    webserver_latest_index = os.path.join(latest_dir, 'index.html')
    webserver_latest_files = os.path.join(latest_dir, '_files')
    # Setup new softlinks to last result.
    os.symlink(webserver_new_index, webserver_latest_index)
    os.symlink(webserver_new_files, webserver_latest_files)
    cmd_helper.RunCmd(['chmod', '755', '-R', latest_dir, new_dir])

  def _GetInstrumentationArgs(self):
    ret = {}
    if self.coverage:
      ret['coverage'] = 'true'
    if self.wait_for_debugger:
      ret['debug'] = 'true'
    return ret

  def _TakeScreenshot(self, test):
    """Takes a screenshot from the device."""
    screenshot_name = os.path.join(constants.SCREENSHOTS_DIR, test + '.png')
    logging.info('Taking screenshot named %s', screenshot_name)
    self.adb.TakeScreenshot(screenshot_name)

  def SetUp(self):
    """Sets up the test harness and device before all tests are run."""
    super(TestRunner, self).SetUp()
    if not self.adb.IsRootEnabled():
      logging.warning('Unable to enable java asserts for %s, non rooted device',
                      self.device)
    else:
      if self.adb.SetJavaAssertsEnabled(enable=not self.disable_assertions):
        self.adb.Reboot(full_reboot=False)

    self.CopyTestFilesOnce()
    # We give different default value to launch HTTP server based on shard index
    # because it may have race condition when multiple processes are trying to
    # launch lighttpd with same port at same time.
    http_server_ports = self.LaunchTestHttpServer(
        os.path.join(constants.CHROME_DIR), self._lighttp_port)
    if self.ports_to_forward:
      port_pairs = [(port, port) for port in self.ports_to_forward]
      # We need to remember which ports the HTTP server is using, since the
      # forwarder will stomp on them otherwise.
      port_pairs.append(http_server_ports)
      self.forwarder = forwarder.Forwarder(self.adb, self.build_type)
      self.forwarder.Run(port_pairs, self.tool, '127.0.0.1')
    self.flags.AddFlags(['--enable-test-intents'])

  def TearDown(self):
    """Cleans up the test harness and saves outstanding data from test run."""
    if self.forwarder:
      self.forwarder.Close()
    self.GenerateCoverageReportIfNeeded()
    super(TestRunner, self).TearDown()

  def TestSetup(self, test):
    """Sets up the test harness for running a particular test.

    Args:
      test: The name of the test that will be run.
    """
    self.SetupPerfMonitoringIfNeeded(test)
    self._SetupIndividualTestTimeoutScale(test)
    self.tool.SetupEnvironment()

    # Make sure the forwarder is still running.
    self.RestartHttpServerForwarderIfNecessary()

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
    self.adb.Adb().SendCommand('shell rm ' +
                               TestRunner._DEVICE_PERF_OUTPUT_SEARCH_PREFIX)
    self.adb.StartMonitoringLogcat()

  def TestTeardown(self, test, raw_result):
    """Cleans up the test harness after running a particular test.

    Depending on the options of this TestRunner this might handle coverage
    tracking or performance tracking.  This method will only be called if the
    test passed.

    Args:
      test: The name of the test that was just run.
      raw_result: result for this test.
    """

    self.tool.CleanUpEnvironment()

    # The logic below relies on the test passing.
    if not raw_result or raw_result.GetStatusCode():
      return

    self.TearDownPerfMonitoring(test)
    self.SaveCoverageData(test)

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
    regex = self.adb.WaitForLogMatch(re.compile('\*\*PERFANNOTATION\(' +
                                                raw_test_name +
                                                '\)\:(.*)'), None)

    # If the test is set to run on a specific device type only (IE: only
    # tablet or phone) and it is being run on the wrong device, the test
    # just quits and does not do anything.  The java test harness will still
    # print the appropriate annotation for us, but will add --NORUN-- for
    # us so we know to ignore the results.
    # The --NORUN-- tag is managed by MainActivityTestBase.java
    if regex.group(1) != '--NORUN--':

      # Obtain the relevant perf data.  The data is dumped to a
      # JSON formatted file.
      json_string = self.adb.GetProtectedFileContents(
          '/data/data/com.google.android.apps.chrome/files/PerfTestData.txt')

      if json_string:
        json_string = '\n'.join(json_string)
      else:
        raise Exception('Perf file does not exist or is empty')

      if self.save_perf_json:
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
          perf_tests_helper.PrintPerfResult(perf_set[1], perf_set[2],
                                            [result['average']],
                                            result['units'])

  def _SetupIndividualTestTimeoutScale(self, test):
    timeout_scale = self._GetIndividualTestTimeoutScale(test)
    valgrind_tools.SetChromeTimeoutScale(self.adb, timeout_scale)

  def _GetIndividualTestTimeoutScale(self, test):
    """Returns the timeout scale for the given |test|."""
    annotations = self.test_pkg.GetTestAnnotations(test)
    timeout_scale = 1
    if 'TimeoutScale' in annotations:
      for annotation in annotations:
        scale_match = re.match('TimeoutScale:([0-9]+)', annotation)
        if scale_match:
          timeout_scale = int(scale_match.group(1))
    if self.wait_for_debugger:
      timeout_scale *= 100
    return timeout_scale

  def _GetIndividualTestTimeoutSecs(self, test):
    """Returns the timeout in seconds for the given |test|."""
    annotations = self.test_pkg.GetTestAnnotations(test)
    if 'Manual' in annotations:
      return 600 * 60
    if 'External' in annotations:
      return 10 * 60
    if 'LargeTest' in annotations or _PERF_TEST_ANNOTATION in annotations:
      return 5 * 60
    if 'MediumTest' in annotations:
      return 3 * 60
    return 1 * 60

  #override.
  def RunTest(self, test):
    raw_result = None
    start_date_ms = None
    results = base_test_result.TestRunResults()
    timeout=(self._GetIndividualTestTimeoutSecs(test) *
             self._GetIndividualTestTimeoutScale(test) *
             self.tool.GetTimeoutScale())
    try:
      self.TestSetup(test)
      start_date_ms = int(time.time()) * 1000

      if self.is_uiautomator_test:
        self.adb.ClearApplicationState(self.package_name)
        # TODO(frankf): Stop-gap solution. Should use annotations.
        if 'FirstRun' in test:
          self.flags.RemoveFlags(['--disable-fre'])
        else:
          self.flags.AddFlags(['--disable-fre'])
        raw_result = self.adb.RunUIAutomatorTest(
            test, self.test_pkg.GetPackageName(), timeout)
      else:
        raw_result = self.adb.RunInstrumentationTest(
            test, self.test_pkg.GetPackageName(),
            self._GetInstrumentationArgs(), timeout)

      duration_ms = int(time.time()) * 1000 - start_date_ms
      status_code = raw_result.GetStatusCode()
      if status_code:
        log = raw_result.GetFailureReason()
        if not log:
          log = 'No information.'
        if self.screenshot_failures or log.find('INJECT_EVENTS perm') >= 0:
          self._TakeScreenshot(test)
        result = test_result.InstrumentationTestResult(
            test, base_test_result.ResultType.FAIL, start_date_ms, duration_ms,
            log=log)
      else:
        result = test_result.InstrumentationTestResult(
            test, base_test_result.ResultType.PASS, start_date_ms, duration_ms)
      results.AddResult(result)
    # Catch exceptions thrown by StartInstrumentation().
    # See ../../third_party/android/testrunner/adb_interface.py
    except (android_commands.errors.WaitForResponseTimedOutError,
            android_commands.errors.DeviceUnresponsiveError,
            android_commands.errors.InstrumentationError), e:
      if start_date_ms:
        duration_ms = int(time.time()) * 1000 - start_date_ms
      else:
        start_date_ms = int(time.time()) * 1000
        duration_ms = 0
      message = str(e)
      if not message:
        message = 'No information.'
      results.AddResult(test_result.InstrumentationTestResult(
          test, base_test_result.ResultType.CRASH, start_date_ms, duration_ms,
          log=message))
      raw_result = None
    self.TestTeardown(test, raw_result)
    return (results, None if results.DidRunPass() else test)
