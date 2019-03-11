# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import shutil
import tempfile
import unittest
import mock

from telemetry.testing import browser_test_runner

from gpu_tests import path_util
from gpu_tests import gpu_integration_test

path_util.AddDirToPathIfNeeded(path_util.GetChromiumSrcDir(), 'tools', 'perf')
from chrome_telemetry_build import chromium_config


class GpuIntegrationTestUnittest(unittest.TestCase):
  def setUp(self):
    self._test_state = {}
    self._test_result = {}

  def testSimpleIntegrationTest(self):
    self._RunIntegrationTest(
      'simple_integration_unittest',
      ['unittest_data.integration_tests.SimpleTest.unexpected_error',
       'unittest_data.integration_tests.SimpleTest.unexpected_failure'],
      ['unittest_data.integration_tests.SimpleTest.expected_flaky',
       'unittest_data.integration_tests.SimpleTest.expected_failure'],
      ['unittest_data.integration_tests.SimpleTest.expected_skip'],
      [])
    # It might be nice to be more precise about the order of operations
    # with these browser restarts, but this is at least a start.
    self.assertEquals(self._test_state['num_browser_starts'], 6)

  def testIntegrationTesttWithBrowserFailure(self):
    self._RunIntegrationTest(
      'browser_start_failure_integration_unittest', [],
      ['unittest_data.integration_tests.BrowserStartFailureTest.restart'],
      [], [])
    self.assertEquals(self._test_state['num_browser_crashes'], 2)
    self.assertEquals(self._test_state['num_browser_starts'], 3)

  def testIntegrationTestWithBrowserCrashUponStart(self):
    self._RunIntegrationTest(
      'browser_crash_after_start_integration_unittest', [],
      [('unittest_data.integration_tests.BrowserCrashAfterStartTest.restart')],
      [], [])
    self.assertEquals(self._test_state['num_browser_crashes'], 2)
    self.assertEquals(self._test_state['num_browser_starts'], 3)

  def testRetryLimit(self):
    self._RunIntegrationTest(
      'test_retry_limit',
      ['unittest_data.integration_tests.TestRetryLimit.unexpected_failure'],
      [],
      [],
      ['--retry-limit=2'])
    # The number of attempted runs is 1 + the retry limit.
    self.assertEquals(self._test_state['num_test_runs'], 3)

  def _RunTestsWithExpectationsFiles(self):
    self._RunIntegrationTest(
      'run_tests_with_expectations_files',
      [('unittest_data.integration_tests'
        '.RunTestsWithExpectationsFiles.unexpected_test_failure')],
      [('unittest_data.integration_tests'
        '.RunTestsWithExpectationsFiles.expected_failure'),
       ('unittest_data.integration_tests'
        '.RunTestsWithExpectationsFiles.expected_flaky')],
      [('unittest_data.integration_tests'
        '.RunTestsWithExpectationsFiles.expected_skip')],
      ['--retry-limit=3', '--retry-only-retry-on-failure-tests'])

  def testUseTestExpectationsFileToHandleExpectedSkip(self):
    self._RunTestsWithExpectationsFiles()
    results = (self._test_result['tests']['unittest_data']['integration_tests']
               ['RunTestsWithExpectationsFiles']['expected_skip'])
    self.assertEqual(results['expected'], 'SKIP')
    self.assertEqual(results['actual'], 'SKIP')
    self.assertNotIn('is_regression', results)

  def testUseTestExpectationsFileToHandleUnexpectedTestFailure(self):
    self._RunTestsWithExpectationsFiles()
    results = (self._test_result['tests']['unittest_data']['integration_tests']
               ['RunTestsWithExpectationsFiles']['unexpected_test_failure'])
    self.assertEqual(results['expected'], 'PASS')
    self.assertEqual(results['actual'], 'FAIL')
    self.assertIn('is_regression', results)

  def testUseTestExpectationsFileToHandleExpectedFailure(self):
    self._RunTestsWithExpectationsFiles()
    results = (self._test_result['tests']['unittest_data']['integration_tests']
               ['RunTestsWithExpectationsFiles']['expected_failure'])
    self.assertEqual(results['expected'], 'FAIL')
    self.assertEqual(results['actual'], 'FAIL')
    self.assertNotIn('is_regression', results)

  def testUseTestExpectationsFileToHandleExpectedFlakyTest(self):
    self._RunTestsWithExpectationsFiles()
    results = (self._test_result['tests']['unittest_data']['integration_tests']
               ['RunTestsWithExpectationsFiles']['expected_flaky'])
    self.assertEqual(results['expected'], 'PASS')
    self.assertEqual(results['actual'], 'FAIL FAIL FAIL PASS')
    self.assertNotIn('is_regression', results)

  def testRepeat(self):
    self._RunIntegrationTest(
      'test_repeat',
      [],
      ['unittest_data.integration_tests.TestRepeat.success'],
      [],
      ['--repeat=3'])
    self.assertEquals(self._test_state['num_test_runs'], 3)

  def testAlsoRunDisabledTests(self):
    self._RunIntegrationTest(
      'test_also_run_disabled_tests',
      ['unittest_data.integration_tests.TestAlsoRunDisabledTests.skip',
       'unittest_data.integration_tests.TestAlsoRunDisabledTests.flaky'],
      # Tests that are expected to fail and do fail are treated as test passes
      [('unittest_data.integration_tests.'
        'TestAlsoRunDisabledTests.expected_failure')],
      [],
      ['--also-run-disabled-tests'])
    self.assertEquals(self._test_state['num_flaky_test_runs'], 4)
    self.assertEquals(self._test_state['num_test_runs'], 6)

  def testStartBrowser_Retries(self):
    class TestException(Exception):
      pass
    def SetBrowserAndRaiseTestException():
      gpu_integration_test.GpuIntegrationTest.browser = (
          mock.MagicMock())
      raise TestException
    gpu_integration_test.GpuIntegrationTest.browser = None
    gpu_integration_test.GpuIntegrationTest.platform = None
    with mock.patch.object(
        gpu_integration_test.serially_executed_browser_test_case.\
            SeriallyExecutedBrowserTestCase,
            'StartBrowser',
            side_effect=SetBrowserAndRaiseTestException) as mock_start_browser:
      with mock.patch.object(
          gpu_integration_test.GpuIntegrationTest,
          'StopBrowser') as mock_stop_browser:
        with self.assertRaises(TestException):
          gpu_integration_test.GpuIntegrationTest.StartBrowser()
        self.assertEqual(mock_start_browser.call_count,
                         gpu_integration_test._START_BROWSER_RETRIES)
        self.assertEqual(mock_stop_browser.call_count,
                         gpu_integration_test._START_BROWSER_RETRIES)

  def _RunIntegrationTest(self, test_name, failures, successes, skips,
                          additional_args):
    config = chromium_config.ChromiumConfig(
        top_level_dir=path_util.GetGpuTestDir(),
        benchmark_dirs=[
            os.path.join(path_util.GetGpuTestDir(), 'unittest_data')])
    temp_dir = tempfile.mkdtemp()
    test_results_path = os.path.join(temp_dir, 'test_results.json')
    test_state_path = os.path.join(temp_dir, 'test_state.json')
    try:
      browser_test_runner.Run(
          config,
          [test_name,
           '--write-full-results-to=%s' % test_results_path,
           '--test-state-json-path=%s' % test_state_path] + additional_args)
      with open(test_results_path) as f:
        self._test_result = json.load(f)
      with open(test_state_path) as f:
        self._test_state = json.load(f)
      actual_successes, actual_failures, actual_skips = (
          self._ExtractTestResults(self._test_result))
      self.assertEquals(set(actual_failures), set(failures))
      self.assertEquals(set(actual_successes), set(successes))
      self.assertEquals(set(actual_skips), set(skips))
    finally:
      shutil.rmtree(temp_dir)

  def _ExtractTestResults(self, test_result):
    delimiter = test_result['path_delimiter']
    failures = []
    successes = []
    skips = []
    def _IsLeafNode(node):
      test_dict = node[1]
      return ('expected' in test_dict and
              isinstance(test_dict['expected'], basestring))
    node_queues = []
    for t in test_result['tests']:
      node_queues.append((t, test_result['tests'][t]))
    while node_queues:
      node = node_queues.pop()
      full_test_name, test_dict = node
      if _IsLeafNode(node):
        if all(res not in test_dict['expected'].split() for res in
               test_dict['actual'].split()):
          failures.append(full_test_name)
        elif test_dict['expected'] == test_dict['actual'] == 'SKIP':
          skips.append(full_test_name)
        else:
          successes.append(full_test_name)
      else:
        for k in test_dict:
          node_queues.append(
            ('%s%s%s' % (full_test_name, delimiter, k),
             test_dict[k]))
    return successes, failures, skips
