# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from telemetry.testing import serially_executed_browser_test_case
from telemetry.util import screenshot
from typ import json_results

from gpu_tests import exception_formatter
from gpu_tests import gpu_test_expectations

_START_BROWSER_RETRIES = 3

ResultType = json_results.ResultType

# Please expand the following lists when we expand to new bot configs.
_SUPPORTED_WIN_VERSIONS = ['win7', 'win10']
_SUPPORTED_WIN_VERSIONS_WITH_DIRECT_COMPOSITION = ['win10']
_SUPPORTED_WIN_GPU_VENDORS = [0x8086, 0x10de, 0x1002]
_SUPPORTED_WIN_INTEL_GPUS = [0x5912]
_SUPPORTED_WIN_INTEL_GPUS_WITH_YUY2_OVERLAYS = [0x5912]
_SUPPORTED_WIN_INTEL_GPUS_WITH_NV12_OVERLAYS = [0x5912]

class GpuIntegrationTest(
    serially_executed_browser_test_case.SeriallyExecutedBrowserTestCase):

  _cached_expectations = None
  _also_run_disabled_tests = False
  _disable_log_uploads = False

  # Several of the tests in this directory need to be able to relaunch
  # the browser on demand with a new set of command line arguments
  # than were originally specified. To enable this, the necessary
  # static state is hoisted here.

  # We store a deep copy of the original browser finder options in
  # order to be able to restart the browser multiple times, with a
  # different set of command line arguments each time.
  _original_finder_options = None

  # We keep track of the set of command line arguments used to launch
  # the browser most recently in order to figure out whether we need
  # to relaunch it, if a new pixel test requires a different set of
  # arguments.
  _last_launched_browser_args = set()

  @classmethod
  def SetUpProcess(cls):
    super(GpuIntegrationTest, cls).SetUpProcess()
    cls._original_finder_options = cls._finder_options.Copy()

  @classmethod
  def AddCommandlineArgs(cls, parser):
    """Adds command line arguments understood by the test harness.

    Subclasses overriding this method must invoke the superclass's
    version!"""
    parser.add_option(
      '--also-run-disabled-tests',
      dest='also_run_disabled_tests',
      action='store_true', default=False,
      help='Run disabled tests, ignoring Skip expectations')
    parser.add_option(
      '--disable-log-uploads',
      dest='disable_log_uploads',
      action='store_true', default=False,
      help='Disables uploads of logs to cloud storage')

  @classmethod
  def CustomizeBrowserArgs(cls, browser_args):
    """Customizes the browser's command line arguments.

    NOTE that redefining this method in subclasses will NOT do what
    you expect! Do not attempt to redefine this method!
    """
    if not browser_args:
      browser_args = []
    cls._finder_options = cls._original_finder_options.Copy()
    browser_options = cls._finder_options.browser_options

    # If requested, disable uploading of failure logs to cloud storage.
    if cls._disable_log_uploads:
      browser_options.logs_cloud_bucket = None

    # A non-sandboxed, 15-seconds-delayed gpu process is currently running in
    # the browser to collect gpu info. A command line switch is added here to
    # skip this gpu process for all gpu integration tests to prevent any
    # interference with the test results.
    browser_args.append(
      '--disable-gpu-process-for-dx12-vulkan-info-collection')

    # Append the new arguments.
    browser_options.AppendExtraBrowserArgs(browser_args)
    cls._last_launched_browser_args = set(browser_args)
    cls.SetBrowserOptions(cls._finder_options)

  @classmethod
  def RestartBrowserIfNecessaryWithArgs(cls, browser_args, force_restart=False):
    if not browser_args:
      browser_args = []
    elif '--disable-gpu' in browser_args:
      # Some platforms require GPU process, so browser fails to launch with
      # --disable-gpu mode, therefore, even test expectations fail to evaluate.
      browser_args = list(browser_args)
      os_name = cls.browser.platform.GetOSName()
      if os_name == 'android' or os_name == 'chromeos':
        browser_args.remove('--disable-gpu')
    if force_restart or set(browser_args) != cls._last_launched_browser_args:
      logging.info('Restarting browser with arguments: ' + str(browser_args))
      cls.StopBrowser()
      cls.CustomizeBrowserArgs(browser_args)
      cls.StartBrowser()

  @classmethod
  def RestartBrowserWithArgs(cls, browser_args):
    cls.RestartBrowserIfNecessaryWithArgs(browser_args, force_restart=True)

  # The following is the rest of the framework for the GPU integration tests.

  @classmethod
  def GenerateTestCases__RunGpuTest(cls, options):
    cls._also_run_disabled_tests = options.also_run_disabled_tests
    cls._disable_log_uploads = options.disable_log_uploads
    for test_name, url, args in cls.GenerateGpuTests(options):
      yield test_name, (url, test_name, args)

  @classmethod
  def StartBrowser(cls):
    # We still need to retry the browser's launch even though
    # desktop_browser_finder does so too, because it wasn't possible
    # to push the fetch of the first tab into the lower retry loop
    # without breaking Telemetry's unit tests, and that hook is used
    # to implement the gpu_integration_test_unittests.
    for x in range(1, _START_BROWSER_RETRIES+1):  # Index from 1 instead of 0.
      try:
        super(GpuIntegrationTest, cls).StartBrowser()
        cls.tab = cls.browser.tabs[0]
        return
      except Exception:
        logging.exception('Browser start failed (attempt %d of %d). Backtrace:',
                          x, _START_BROWSER_RETRIES)
        # If we are on the last try and there is an exception take a screenshot
        # to try and capture more about the browser failure and raise
        if x == _START_BROWSER_RETRIES:
          url = screenshot.TryCaptureScreenShotAndUploadToCloudStorage(
            cls.platform)
          if url is not None:
            logging.info("GpuIntegrationTest screenshot of browser failure " +
              "located at " + url)
          else:
            logging.warning("GpuIntegrationTest unable to take screenshot.")
        # Stop the browser to make sure it's in an
        # acceptable state to try restarting it.
        if cls.browser:
          cls.StopBrowser()
    # Re-raise the last exception thrown. Only happens if all the retries
    # fail.
    raise

  @classmethod
  def _RestartBrowser(cls, reason):
    logging.warning('Restarting browser due to '+ reason)
    cls.StopBrowser()
    cls.SetBrowserOptions(cls._finder_options)
    cls.StartBrowser()

  def _RunGpuTestWithExpectationsFiles(self, url, test_name, *args):
    expected_results, should_retry_on_failure = (
        self.GetExpectationsForTest())
    try:
      # TODO(nednguyen): For some reason the arguments are getting wrapped
      # in another tuple sometimes (like in the WebGL extension tests).
      # Perhaps only if multiple arguments are yielded in the test
      # generator?
      if len(args) == 1 and isinstance(args[0], tuple):
        args = args[0]
      self.RunActualGpuTest(url, *args)
    except Exception:
      if ResultType.Failure in expected_results or should_retry_on_failure:
        if should_retry_on_failure:
          # For robustness, shut down the browser and restart it
          # between flaky test failures, to make sure any state
          # doesn't propagate to the next iteration.
          self._RestartBrowser('flaky test failure')
        else:
          msg = 'Expected exception while running %s' % test_name
          exception_formatter.PrintFormattedException(msg=msg)
          # Even though this is a known failure, the browser might still
          # be in a bad state; for example, certain kinds of timeouts
          # will affect the next test. Restart the browser to prevent
          # these kinds of failures propagating to the next test.
          self._RestartBrowser('expected test failure')
      else:
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
        self.browser.LogSymbolizedUnsymbolizedMinidumps(logging.ERROR)
        # This failure might have been caused by a browser or renderer
        # crash, so restart the browser to make sure any state doesn't
        # propagate to the next test iteration.
        self._RestartBrowser('unexpected test failure')
      self.fail()
    else:
      if ResultType.Failure in expected_results:
        logging.warning(
          '%s was expected to fail, but passed.\n', test_name)

  def _RunGpuTest(self, url, test_name, *args):
    cls = self.__class__
    if cls.ExpectationsFiles():
      self._RunGpuTestWithExpectationsFiles(url, test_name, *args)
      return
    expectations = cls.GetExpectations()
    expectation = expectations.GetExpectationForTest(
      self.browser, url, test_name)
    if expectation == 'skip':
      if self.__class__._also_run_disabled_tests:
        # Ignore test expectations if the user has requested it.
        expectation = 'pass'
      else:
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
        self.browser.LogSymbolizedUnsymbolizedMinidumps(logging.ERROR)
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

  def GetOverlayBotConfig(self):
    """Returns expected bot config for DirectComposition and overlay support.

    This is only meaningful on Windows platform.

    The rules to determine bot config are:
      1) Only win10 or newer supports DirectComposition
      2) Only Intel supports hardware overlays with DirectComposition
      3) Currently the Win/Intel GPU bot supports YUY2 and NV12 overlays
    """
    if self.browser is None:
      raise Exception("Browser doesn't exist")
    system_info = self.browser.GetSystemInfo()
    if system_info is None:
      raise Exception("Browser doesn't support GetSystemInfo")
    gpu = system_info.gpu.devices[0]
    if gpu is None:
      raise Exception("System Info doesn't have a gpu")
    gpu_vendor_id = gpu.vendor_id
    gpu_device_id = gpu.device_id
    os_version = self.browser.platform.GetOSVersionName()
    if os_version is None:
      raise Exception("browser.platform.GetOSVersionName() returns None")
    os_version = os_version.lower()

    config = {
      'direct_composition': False,
      'supports_overlays': False,
      'overlay_cap_yuy2': 'NONE',
      'overlay_cap_nv12': 'NONE',
    }
    assert os_version in _SUPPORTED_WIN_VERSIONS
    assert gpu_vendor_id in _SUPPORTED_WIN_GPU_VENDORS
    if os_version in _SUPPORTED_WIN_VERSIONS_WITH_DIRECT_COMPOSITION:
      config['direct_composition'] = True
      if gpu_vendor_id == 0x8086:
        config['supports_overlays'] = True
        assert gpu_device_id in _SUPPORTED_WIN_INTEL_GPUS
        if gpu_device_id in _SUPPORTED_WIN_INTEL_GPUS_WITH_YUY2_OVERLAYS:
          config['overlay_cap_yuy2'] = 'SCALING'
        if gpu_device_id in _SUPPORTED_WIN_INTEL_GPUS_WITH_NV12_OVERLAYS:
          config['overlay_cap_nv12'] = 'SCALING'
    return config

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
