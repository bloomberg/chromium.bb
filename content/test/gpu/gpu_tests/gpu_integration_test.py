# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from telemetry.testing import serially_executed_browser_test_case
from telemetry.util import screenshot

from gpu_tests import exception_formatter
from gpu_tests import gpu_test_expectations


class GpuIntegrationTest(
    serially_executed_browser_test_case.SeriallyExecutedBrowserTestCase):

  _cached_expectations = None

  @classmethod
  def GenerateTestCases__RunGpuTest(cls, options):
    for test_name, url, args in cls.GenerateGpuTests(options):
      yield test_name, (url, test_name, args)

  @classmethod
  def StartBrowser(cls):
    for x in range(0, 3):
      try:
        restart = 'Starting browser, attempt %d of 3' % (x + 1)
        logging.warning(restart)
        super(GpuIntegrationTest, cls).StartBrowser()
        cls.tab = cls.browser.tabs[0]
        logging.warning('Started browser successfully.')
        return
      except Exception:
        # If we are on the last try and there is an exception take a screenshot
        # to try and capture more about the browser failure and raise
        if x == 2:
          url = screenshot.TryCaptureScreenShotAndUploadToCloudStorage(
            cls.platform)
          if url is not None:
            logging.info("GpuIntegrationTest screenshot of browser failure " +
              "located at " + url)
          else:
            logging.warning("GpuIntegrationTest unable to take screenshot")
          raise
        # Otherwise, stop the browser to make sure it's in an
        # acceptable state to try restarting it.
        if cls.browser:
          cls.StopBrowser()

  @classmethod
  def _RestartBrowser(cls, reason):
    logging.warning('Restarting browser due to '+ reason)
    cls.StopBrowser()
    cls.SetBrowserOptions(cls._finder_options)
    cls.StartBrowser()

  def _RunGpuTest(self, url, test_name, *args):
    expectations = self.__class__.GetExpectations()
    expectation = expectations.GetExpectationForTest(
      self.browser, url, test_name)
    if expectation == 'skip':
      # skipTest in Python's unittest harness raises an exception, so
      # aborts the control flow here.
      self.skipTest('SKIPPING TEST due to test expectations')
    try:
      # TODO(nednguyen): For some reason the arguments are getting wrapped
      # in another tuple sometimes (like in the WebGL extension tests).
      # Perhaps only if multiple arguments are yielded in the test
      # generator?
      if len(args) == 1 and isinstance(args[0], tuple):
        args = args[0]
      self.RunActualGpuTest(url, *args)
    except Exception:
      if expectation == 'pass':
        # This is not an expected exception or test failure, so print
        # the detail to the console.
        exception_formatter.PrintFormattedException()
        # Symbolize any crash dump (like from the GPU process) that
        # might have happened but wasn't detected above. Note we don't
        # do this for either 'fail' or 'flaky' expectations because
        # there are still quite a few flaky failures in the WebGL test
        # expectations, and since minidump symbolization is slow
        # (upwards of one minute on a fast laptop), symbolizing all the
        # stacks could slow down the tests' running time unacceptably.
        self._SymbolizeUnsymbolizedMinidumps()
        # This failure might have been caused by a browser or renderer
        # crash, so restart the browser to make sure any state doesn't
        # propagate to the next test iteration.
        self._RestartBrowser('unexpected test failure')
        raise
      elif expectation == 'fail':
        msg = 'Expected exception while running %s' % test_name
        exception_formatter.PrintFormattedException(msg=msg)
        # Even though this is a known failure, the browser might still
        # be in a bad state; for example, certain kinds of timeouts
        # will affect the next test. Restart the browser to prevent
        # these kinds of failures propagating to the next test.
        self._RestartBrowser('expected test failure')
        return
      if expectation != 'flaky':
        logging.warning(
          'Unknown expectation %s while handling exception for %s',
          expectation, test_name)
        raise
      # Flaky tests are handled here.
      num_retries = expectations.GetFlakyRetriesForTest(
        self.browser, url, test_name)
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

  def _SymbolizeUnsymbolizedMinidumps(self):
    # The fakes used for unit tests don't mock this entry point yet.
    if not hasattr(self.browser, 'GetAllUnsymbolizedMinidumpPaths'):
      return
    i = 10
    if self.browser.GetAllUnsymbolizedMinidumpPaths():
      logging.error('Symbolizing minidump paths: ' + str(
        self.browser.GetAllUnsymbolizedMinidumpPaths()))
    else:
      logging.error('No minidump paths to symbolize')
    while i > 0 and self.browser.GetAllUnsymbolizedMinidumpPaths():
      i = i - 1
      sym = self.browser.SymbolizeMinidump(
        self.browser.GetAllUnsymbolizedMinidumpPaths()[0])
      if sym[0]:
        logging.error('Symbolized minidump:\n' + sym[1])
      else:
        logging.error('Minidump symbolization failed:\n' + sym[1])

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

  @classmethod
  def _EnsureTabIsAvailable(cls):
    try:
      cls.tab = cls.browser.tabs[0]
    except Exception:
      # restart the browser to make sure a failure in a test doesn't
      # propagate to the next test iteration.
      logging.exception("Failure during browser startup")
      cls._RestartBrowser('failure in setup')
      raise

  def setUp(self):
    self._EnsureTabIsAvailable()

def LoadAllTestsInModule(module):
  # Just delegates to serially_executed_browser_test_case to reduce the
  # number of imports in other files.
  return serially_executed_browser_test_case.LoadAllTestsInModule(module)
