# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import time

from gpu_tests import gpu_integration_test
from gpu_tests import context_lost_expectations
from gpu_tests import path_util

from telemetry.core import exceptions

data_path = os.path.join(
    path_util.GetChromiumSrcDir(), 'content', 'test', 'data', 'gpu')

wait_timeout = 60  # seconds

harness_script = r"""
  var domAutomationController = {};

  domAutomationController._loaded = false;
  domAutomationController._succeeded = false;
  domAutomationController._finished = false;

  domAutomationController.setAutomationId = function(id) {}

  domAutomationController.send = function(msg) {
    msg = msg.toLowerCase()
    if (msg == "loaded") {
      domAutomationController._loaded = true;
    } else if (msg == "success") {
      /* Don't squelch earlier failures! */
      if (!domAutomationController._finished) {
        domAutomationController._succeeded = true;
      }
      domAutomationController._finished = true;
    } else {
      /* Always record failures. */
      domAutomationController._succeeded = false;
      domAutomationController._finished = true;
    }
  }

  domAutomationController.reset = function() {
    domAutomationController._succeeded = false;
    domAutomationController._finished = false;
  }

  window.domAutomationController = domAutomationController;
  console.log("Harness injected.");
"""

class ContextLostIntegrationTest(gpu_integration_test.GpuIntegrationTest):

  @classmethod
  def Name(cls):
    return 'context_lost'

  @classmethod
  def CustomizeOptions(cls):
    options = cls._finder_options.browser_options
    options.AppendExtraBrowserArgs(
        '--disable-domain-blocking-for-3d-apis')
    options.AppendExtraBrowserArgs(
        '--disable-gpu-process-crash-limit')
    # Required for about:gpucrash handling from Telemetry.
    options.AppendExtraBrowserArgs('--enable-gpu-benchmarking')

  @classmethod
  def GenerateGpuTests(cls, options):
    tests = (('GpuCrash_GPUProcessCrashesExactlyOncePerVisitToAboutGpuCrash',
              'gpu_process_crash.html'),
             ('ContextLost_WebGLContextLostFromGPUProcessExit',
              'webgl.html?query=kill_after_notification'),
             ('ContextLost_WebGLContextLostFromLoseContextExtension',
              'webgl.html?query=WEBGL_lose_context'),
             ('ContextLost_WebGLContextLostFromQuantity',
              'webgl.html?query=forced_quantity_loss'),
             ('ContextLost_WebGLContextLostFromSelectElement',
              'webgl_with_select_element.html'),
             ('ContextLost_WebGLContextLostInHiddenTab',
              'webgl.html?query=kill_after_notification'))
    for t in tests:
      yield (t[0], t[1], ('_' + t[0]))

  def RunActualGpuTest(self, test_path, *args):
    test_name = args[0]
    tab = self.tab
    if not tab.browser.supports_tab_control:
      self.fail('Browser must support tab control')
    getattr(self, test_name)(test_path)

  @classmethod
  def _CreateExpectations(cls):
    return context_lost_expectations.ContextLostExpectations()

  @classmethod
  def SetUpProcess(cls):
    super(cls, ContextLostIntegrationTest).SetUpProcess()
    cls.CustomizeOptions()
    cls.SetBrowserOptions(cls._finder_options)
    cls.StartBrowser()
    cls.SetStaticServerDirs([data_path])

  def _WaitForPageToFinish(self, tab):
    try:
      tab.WaitForJavaScriptCondition(
        'window.domAutomationController._finished', timeout=wait_timeout)
      return True
    except exceptions.TimeoutException:
      return False

  def _KillGPUProcess(self, number_of_gpu_process_kills,
                      check_crash_count):
    tab = self.tab
    # Doing the GPU process kill operation cooperatively -- in the
    # same page's context -- is much more stressful than restarting
    # the browser every time.
    for x in range(number_of_gpu_process_kills):
      expected_kills = x + 1

      # Reset the test's state.
      tab.EvaluateJavaScript(
        'window.domAutomationController.reset()')

      # If we're running the GPU process crash test, we need the test
      # to have fully reset before crashing the GPU process.
      if check_crash_count:
        tab.WaitForJavaScriptCondition(
          'window.domAutomationController._finished', timeout=wait_timeout)

      # Crash the GPU process.
      gpucrash_tab = tab.browser.tabs.New()
      # To access these debug URLs from Telemetry, they have to be
      # written using the chrome:// scheme.
      # The try/except is a workaround for crbug.com/368107.
      try:
        gpucrash_tab.Navigate('chrome://gpucrash')
      except Exception:
        print 'Tab crashed while navigating to chrome://gpucrash'
      # Activate the original tab and wait for completion.
      tab.Activate()
      completed = self._WaitForPageToFinish(tab)

      if check_crash_count:
        self._CheckCrashCount(tab, expected_kills)

      # The try/except is a workaround for crbug.com/368107.
      try:
        gpucrash_tab.Close()
      except Exception:
        print 'Tab crashed while closing chrome://gpucrash'
      if not completed:
        self.fail('Test didn\'t complete (no context lost event?)')
      if not tab.EvaluateJavaScript(
        'window.domAutomationController._succeeded'):
        self.fail('Test failed (context not restored properly?)')

  def _CheckCrashCount(self, tab, expected_kills):
    if not tab.browser.supports_system_info:
      self.fail('Browser must support system info')

    if not tab.EvaluateJavaScript(
        'window.domAutomationController._succeeded'):
      self.fail('Test failed (didn\'t render content properly?)')

    number_of_crashes = -1
    # To allow time for a gpucrash to complete, wait up to 20s,
    # polling repeatedly.
    start_time = time.time()
    current_time = time.time()
    while current_time - start_time < 20:
      system_info = tab.browser.GetSystemInfo()
      number_of_crashes = \
          system_info.gpu.aux_attributes[u'process_crash_count']
      if number_of_crashes >= expected_kills:
        break
      time.sleep(1)
      current_time = time.time()

    # Wait 5 more seconds and re-read process_crash_count, in
    # attempt to catch latent process crashes.
    time.sleep(5)
    system_info = tab.browser.GetSystemInfo()
    number_of_crashes = \
        system_info.gpu.aux_attributes[u'process_crash_count']

    if number_of_crashes < expected_kills:
      self.fail('Timed out waiting for a gpu process crash')
    elif number_of_crashes != expected_kills:
      self.fail('Expected %d gpu process crashes; got: %d' %
                (expected_kills, number_of_crashes))

  def _NavigateAndWaitForLoad(self, test_path):
    url = self.UrlOfStaticFilePath(test_path)
    tab = self.tab
    tab.Navigate(url, script_to_evaluate_on_commit=harness_script)
    tab.action_runner.WaitForJavaScriptCondition(
      'window.domAutomationController._loaded')

  def _WaitForTabAndCheckCompletion(self):
    tab = self.tab
    completed = self._WaitForPageToFinish(tab)
    if not completed:
      self.fail('Test didn\'t complete (no context restored event?)')
    if not tab.EvaluateJavaScript('window.domAutomationController._succeeded'):
      self.fail('Test failed (context not restored properly?)')

  # The browser test runner synthesizes methods with the exact name
  # given in GenerateGpuTests, so in order to hand-write our tests but
  # also go through the _RunGpuTest trampoline, the test needs to be
  # slightly differently named.
  def _GpuCrash_GPUProcessCrashesExactlyOncePerVisitToAboutGpuCrash(
      self, test_path):
    self._NavigateAndWaitForLoad(test_path)
    self._KillGPUProcess(2, True)
    self._RestartBrowser('must restart after tests that kill the GPU process')

  def _ContextLost_WebGLContextLostFromGPUProcessExit(self, test_path):
    self._NavigateAndWaitForLoad(test_path)
    self._KillGPUProcess(1, False)
    self._RestartBrowser('must restart after tests that kill the GPU process')

  def _ContextLost_WebGLContextLostFromLoseContextExtension(self, test_path):
    url = self.UrlOfStaticFilePath(test_path)
    tab = self.tab
    tab.Navigate(url, script_to_evaluate_on_commit=harness_script)
    tab.action_runner.WaitForJavaScriptCondition(
      'window.domAutomationController._finished')

  def _ContextLost_WebGLContextLostFromQuantity(self, test_path):
    self._NavigateAndWaitForLoad(test_path)
    # Try to coerce GC to clean up any contexts not attached to the page.
    # This method seems unreliable, so the page will also attempt to
    # force GC through excessive allocations.
    self.tab.CollectGarbage()
    self._WaitForTabAndCheckCompletion()

  def _ContextLost_WebGLContextLostFromSelectElement(self, test_path):
    self._NavigateAndWaitForLoad(test_path)
    self._WaitForTabAndCheckCompletion()

  def _ContextLost_WebGLContextLostInHiddenTab(self, test_path):
    self._NavigateAndWaitForLoad(test_path)
    # Test losing a context in a hidden tab. This test passes if the tab
    # doesn't crash.
    tab = self.tab
    dummy_tab = tab.browser.tabs.New()
    tab.EvaluateJavaScript('loseContextUsingExtension()')
    tab.Activate()
    self._WaitForTabAndCheckCompletion()

def load_tests(loader, tests, pattern):
  del loader, tests, pattern  # Unused.
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])
