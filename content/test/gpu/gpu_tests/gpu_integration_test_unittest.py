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


class SimpleIntegrationUnittest(gpu_integration_test.GpuIntegrationTest):
  # Must be class-scoped since instances aren't reused across runs.
  _num_flaky_runs_to_fail = 2

  _num_browser_starts = 0

  @classmethod
  def Name(cls):
    return 'simple_integration_unittest'

  @classmethod
  def setUpClass(cls):
    finder_options = fakes.CreateBrowserFinderOptions()
    finder_options.browser_options.platform = fakes.FakeLinuxPlatform()
    finder_options.output_formats = ['none']
    finder_options.suppress_gtest_report = True
    finder_options.output_dir = None
    finder_options.upload_bucket = 'public'
    finder_options.upload_results = False
    cls._finder_options = finder_options
    cls.platform = None
    cls.browser = None
    cls.StartBrowser(cls._finder_options)

  @classmethod
  def GenerateGpuTests(cls, options):
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
  def StartBrowser(cls, options):
    super(SimpleIntegrationUnittest, cls).StartBrowser(options)
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


class GpuIntegrationTestUnittest(unittest.TestCase):
  @mock.patch('telemetry.internal.util.binary_manager.InitDependencyManager')
  def testSimpleIntegrationUnittest(self, mockInitDependencyManager):
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
          ['simple_integration_unittest',
           '--write-abbreviated-json-results-to=%s' % temp_file_name])
      with open(temp_file_name) as f:
        test_result = json.load(f)
      self.assertEquals(test_result['failures'], [
          'unexpected_error',
          'unexpected_failure'])
      self.assertEquals(test_result['successes'], [
          'expected_failure',
          'expected_flaky'])
      self.assertEquals(test_result['valid'], True)
      # It might be nice to be more precise about the order of operations
      # with these browser restarts, but this is at least a start.
      self.assertEquals(SimpleIntegrationUnittest._num_browser_starts, 5)
    finally:
      os.remove(temp_file_name)
