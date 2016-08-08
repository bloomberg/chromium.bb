# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import mock
import os
import tempfile
import unittest

from telemetry.testing import fakes
from telemetry.testing import browser_test_runner

import gpu_project_config

from gpu_tests import gpu_integration_test
from gpu_tests import gpu_test_expectations

_GLOBAL_TEST_COUNT = 0

class SimpleIntegrationUnittest(gpu_integration_test.GpuIntegrationTest):
  # Must be class-scoped since instances aren't reused across runs.
  _num_flaky_runs_to_fail = 2

  _num_browser_starts = 0

  @classmethod
  def Name(cls):
    return 'simple_integration_unittest'

  def setUp(self):
    global _GLOBAL_TEST_COUNT
    _GLOBAL_TEST_COUNT += 1
    # If this is the first test, fail on setup to ensure that the
    # gpu_integration_test handles failures in setup and remaining tests
    # can be executed
    if _GLOBAL_TEST_COUNT == 1:
      self.tab.Navigate('chrome://crash')
    super(SimpleIntegrationUnittest, self).setUp()

  @classmethod
  def setUpClass(cls):
    finder_options = fakes.CreateBrowserFinderOptions()
    finder_options.browser_options.platform = fakes.FakeLinuxPlatform()
    finder_options.output_formats = ['none']
    finder_options.suppress_gtest_report = True
    finder_options.output_dir = None
    finder_options .upload_bucket = 'public'
    finder_options .upload_results = False
    cls._finder_options = finder_options
    cls.platform = None
    cls.browser = None
    cls.SetBrowserOptions(cls._finder_options)
    cls.StartBrowser()

  @classmethod
  def GenerateGpuTests(cls, options):
    yield ('setup', 'failure.html', ())
    yield ('expected_failure', 'failure.html', ())
    yield ('expected_flaky', 'flaky.html', ())
    yield ('expected_skip', 'failure.html', ())
    yield ('unexpected_failure', 'failure.html', ())
    yield ('unexpected_error', 'error.html', ())

  @classmethod
  def _CreateExpectations(cls):
    expectations = gpu_test_expectations.GpuTestExpectations()
    expectations.Fail('expected_failure')
    expectations.Flaky('expected_flaky', max_num_retries=3)
    expectations.Skip('expected_skip')
    return expectations

  @classmethod
  def StartBrowser(cls):
    super(SimpleIntegrationUnittest, cls).StartBrowser()
    cls._num_browser_starts += 1

  def RunActualGpuTest(self, file_path, *args):
    if file_path == 'failure.html':
      self.fail('Expected failure')
    elif file_path == 'flaky.html':
      if self.__class__._num_flaky_runs_to_fail > 0:
        self.__class__._num_flaky_runs_to_fail -= 1
        self.fail('Expected flaky failure')
    elif file_path == 'error.html':
      raise Exception('Expected exception')


class BrowserStartFailureIntegrationUnittest(
  gpu_integration_test.GpuIntegrationTest):

  _num_browser_crashes = 0
  _num_browser_starts = 0

  @classmethod
  def setUpClass(cls):
    cls._fake_browser_options = \
        fakes.CreateBrowserFinderOptions(execute_on_startup=cls.CrashOnStart)
    cls._fake_browser_options.browser_options.platform = \
        fakes.FakeLinuxPlatform()
    cls._fake_browser_options.output_formats = ['none']
    cls._fake_browser_options.suppress_gtest_report = True
    cls._fake_browser_options.output_dir = None
    cls._fake_browser_options .upload_bucket = 'public'
    cls._fake_browser_options .upload_results = False
    cls._finder_options = cls._fake_browser_options
    cls.platform = None
    cls.browser = None
    cls.SetBrowserOptions(cls._finder_options)
    cls.StartBrowser()

  @classmethod
  def _CreateExpectations(cls):
    expectations = gpu_test_expectations.GpuTestExpectations()
    expectations.Fail('expected_failure')
    expectations.Flaky('expected_flaky', max_num_retries=3)
    expectations.Skip('expected_skip')
    return expectations

  @classmethod
  def CrashOnStart(cls):
    cls._num_browser_starts += 1
    if cls._num_browser_crashes < 2:
      cls._num_browser_crashes += 1
      raise

  @classmethod
  def Name(cls):
    return 'browser_start_failure_integration_unittest'

  @classmethod
  def GenerateGpuTests(cls, options):
    # This test causes the browser to try and restart the browser 3 times.
    yield ('restart', 'restart.html', ())

  def RunActualGpuTest(self, file_path, *args):
    # The logic of this test is run when the browser starts, it fails twice
    # and then succeeds on the third time so we are just testing that this
    # is successful based on the parameters.
    pass


class GpuIntegrationTestUnittest(unittest.TestCase):
  @mock.patch('telemetry.internal.util.binary_manager.InitDependencyManager')
  def testSimpleIntegrationUnittest(self, mockInitDependencyManager):
    self._RunIntegrationTest(
      'simple_integration_unittest', [
          'expected_failure',
          'setup',
          'unexpected_error',
          'unexpected_failure'], ['expected_flaky'])
    # It might be nice to be more precise about the order of operations
    # with these browser restarts, but this is at least a start.
    self.assertEquals(SimpleIntegrationUnittest._num_browser_starts, 6)

  @mock.patch('telemetry.internal.util.binary_manager.InitDependencyManager')
  def testIntegrationUnittestWithBrowserFailure(
      self, mockInitDependencyManager):
    self._RunIntegrationTest(
      'browser_start_failure_integration_unittest', [], ['restart'])
    self.assertEquals( \
      BrowserStartFailureIntegrationUnittest._num_browser_crashes, 2)
    self.assertEquals( \
      BrowserStartFailureIntegrationUnittest._num_browser_starts, 3)

  def _RunIntegrationTest(self, test_name, failures, successes):
    options = browser_test_runner.TestRunOptions()
    # Suppress printing out information for passing tests.
    options.verbosity = 0
    config = gpu_project_config.CONFIG
    temp_file = tempfile.NamedTemporaryFile(delete=False)
    temp_file.close()
    temp_file_name = temp_file.name
    try:
      browser_test_runner.Run(
          config, options,
          [test_name,
           '--write-abbreviated-json-results-to=%s' % temp_file_name])
      with open(temp_file_name) as f:
        test_result = json.load(f)
      self.assertEquals(test_result['failures'], failures)
      self.assertEquals(test_result['successes'], successes)
      self.assertEquals(test_result['valid'], True)

    finally:
      os.remove(temp_file_name)
