# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from telemetry.testing import serially_executed_browser_test_case

from gpu_tests import exception_formatter
from gpu_tests import gpu_test_expectations


# Temporary class which bridges the gap beween these serially executed
# browser tests and the old benchmark-based tests. As soon as all of
# the tests have been coverted over, this will be deleted.
class _EmulatedPage(object):
  def __init__(self, url, name):
    self.url = url
    self.name = name


class GpuIntegrationTest(
    serially_executed_browser_test_case.SeriallyBrowserTestCase):

  _cached_expectations = None

  @classmethod
  def GenerateTestCases__RunGpuTest(cls, options):
    for test_name, url, args in cls.GenerateGpuTests(options):
      yield test_name, (url, test_name, args)

  def _RestartBrowser(self, reason):
    logging.warning('Restarting browser due to ' + reason)
    self.StopBrowser()
    self.StartBrowser(self._finder_options)
    self.tab = self.browser.tabs[0]

  def _RunGpuTest(self, url, test_name, args):
    temp_page = _EmulatedPage(url, test_name)
    expectations = self.__class__.GetExpectations()
    expectation = expectations.GetExpectationForPage(
      self.browser, temp_page)
    if expectation == 'skip':
      # skipTest in Python's unittest harness raises an exception, so
      # aborts the control flow here.
      self.skipTest('SKIPPING TEST due to test expectations')
    try:
      self.RunActualGpuTest(url, *args)
    except Exception:
      if expectation == 'pass':
        # This is not an expected exception or test failure, so print
        # the detail to the console.
        exception_formatter.PrintFormattedException()
        # This failure might have been caused by a browser or renderer
        # crash, so restart the browser to make sure any state doesn't
        # propagate to the next test iteration.
        self._RestartBrowser('unexpected test failure')
        raise
      elif expectation == 'fail':
        msg = 'Expected exception while running %s' % test_name
        exception_formatter.PrintFormattedException(msg=msg)
        return
      if expectation != 'flaky':
        logging.warning(
          'Unknown expectation %s while handling exception for %s',
          expectation, test_name)
        raise
      # Flaky tests are handled here.
      num_retries = expectations.GetFlakyRetriesForPage(
        self.browser, temp_page)
      if not num_retries:
        # Re-raise the exception.
        raise
      # Re-run the test up to |num_retries| times.
      for ii in xrange(0, num_retries):
        print 'FLAKY TEST FAILURE, retrying: ' + test_name
        try:
          # For robustness, shut down the browser and restart it
          # between flaky test failures, to make sure any state
          # doesn't propagate to the next iteration.
          self._RestartBrowser('flaky test failure')
          self.RunActualGpuTest(url, *args)
          break
        except Exception:
          # Squelch any exceptions from any but the last retry.
          if ii == num_retries - 1:
            # Restart the browser after the last failure to make sure
            # any state doesn't propagate to the next iteration.
            self._RestartBrowser('excessive flaky test failures')
            raise
    else:
      if expectation == 'fail':
        logging.warning(
            '%s was expected to fail, but passed.\n', test_name)

  @classmethod
  def GenerateGpuTests(cls, options):
    """Subclasses must implement this to yield (test_name, url, args)
    tuples of tests to run."""
    raise NotImplementedError

  def RunActualGpuTest(self, file_path, *args):
    """Subclasses must override this to run the actual test at the given
    URL. file_path is a path on the local file system that may need to
    be resolved via UrlOfStaticFilePath.
    """
    raise NotImplementedError

  @classmethod
  def GetExpectations(cls):
    if not cls._cached_expectations:
      cls._cached_expectations = cls._CreateExpectations()
    if not isinstance(cls._cached_expectations,
                      gpu_test_expectations.GpuTestExpectations):
      raise Exception(
          'gpu_integration_test requires use of GpuTestExpectations')
    return cls._cached_expectations

  @classmethod
  def _CreateExpectations(cls):
    # Subclasses **must** override this in order to provide their test
    # expectations to the harness.
    #
    # Do not call this directly. Call GetExpectations where necessary.
    raise NotImplementedError

  def setUp(self):
    self.tab = self.browser.tabs[0]
