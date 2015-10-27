# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import time

from gpu_tests import context_lost_expectations
from gpu_tests import gpu_test_base
from gpu_tests import path_util

from telemetry.core import exceptions
from telemetry.core import util
from telemetry.page import page_test
from telemetry.story import story_set as story_set_module

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
      domAutomationController._succeeded = true;
      domAutomationController._finished = true;
    } else {
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

class ContextLostValidator(gpu_test_base.ValidatorBase):
  def __init__(self):
    # Strictly speaking this test doesn't yet need a browser restart
    # after each run, but if more tests are added which crash the GPU
    # process, then it will.
    super(ContextLostValidator, self).__init__()

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs(
        '--disable-domain-blocking-for-3d-apis')
    options.AppendExtraBrowserArgs(
        '--disable-gpu-process-crash-limit')
    # Required for about:gpucrash handling from Telemetry.
    options.AppendExtraBrowserArgs('--enable-gpu-benchmarking')

  def ValidateAndMeasurePage(self, page, tab, results):
    def WaitForPageToFinish():
      print "Waiting for page to finish."
      try:
        util.WaitFor(lambda: tab.EvaluateJavaScript(
            'window.domAutomationController._finished'), wait_timeout)
        return True
      except exceptions.TimeoutException:
        return False

    if page.kill_gpu_process:
      # Doing the GPU process kill operation cooperatively -- in the
      # same page's context -- is much more stressful than restarting
      # the browser every time.
      for x in range(page.number_of_gpu_process_kills):
        if not tab.browser.supports_tab_control:
          raise page_test.Failure('Browser must support tab control')

        expected_kills = x + 1

        # Reset the test's state.
        tab.EvaluateJavaScript(
          'window.domAutomationController.reset()')

        # If we're running the GPU process crash test, we need the
        # test to have fully reset before crashing the GPU process.
        if page.check_crash_count:
          util.WaitFor(lambda: tab.EvaluateJavaScript(
              'window.domAutomationController._finished'), wait_timeout)

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
        completed = WaitForPageToFinish()

        if page.check_crash_count:
          if not tab.browser.supports_system_info:
            raise page_test.Failure('Browser must support system info')

          if not tab.EvaluateJavaScript(
            'window.domAutomationController._succeeded'):
            raise page_test.Failure(
              'Test failed (didn\'t render content properly?)')

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
            raise page_test.Failure(
                'Timed out waiting for a gpu process crash')
          elif number_of_crashes != expected_kills:
            raise page_test.Failure(
                'Expected %d gpu process crashes; got: %d' %
                (expected_kills, number_of_crashes))

        # The try/except is a workaround for crbug.com/368107.
        try:
          gpucrash_tab.Close()
        except Exception:
          print 'Tab crashed while closing chrome://gpucrash'
        if not completed:
          raise page_test.Failure(
              'Test didn\'t complete (no context lost event?)')
        if not tab.EvaluateJavaScript(
          'window.domAutomationController._succeeded'):
          raise page_test.Failure(
            'Test failed (context not restored properly?)')
    elif page.force_garbage_collection:
      # Try to corce GC to clean up any contexts not attached to the page.
      # This method seem unreliable, so the page will also attempt to force
      # GC through excessive allocations.
      tab.CollectGarbage()
      completed = WaitForPageToFinish()

      if not completed:
        raise page_test.Failure(
            'Test didn\'t complete (no context restored event?)')
      if not tab.EvaluateJavaScript(
        'window.domAutomationController._succeeded'):
        raise page_test.Failure(
          'Test failed (context not restored properly?)')
    elif page.hide_tab_and_lose_context:
      if not tab.browser.supports_tab_control:
        raise page_test.Failure('Browser must support tab control')

      # Test losing a context in a hidden tab. This test passes if the tab
      # doesn't crash.
      dummy_tab = tab.browser.tabs.New()
      tab.EvaluateJavaScript('loseContextUsingExtension()')
      tab.Activate()

      completed = WaitForPageToFinish()

      if not completed:
        raise page_test.Failure('Test didn\'t complete')
      if not tab.EvaluateJavaScript(
        'window.domAutomationController._succeeded'):
        raise page_test.Failure('Test failed')
    else:
      completed = WaitForPageToFinish()

      if not completed:
        raise page_test.Failure('Test didn\'t complete')
      if not tab.EvaluateJavaScript(
        'window.domAutomationController._succeeded'):
        raise page_test.Failure('Test failed')

# Test that navigating to chrome://gpucrash causes the GPU process to crash
# exactly one time per navigation.
class GPUProcessCrashesExactlyOnce(gpu_test_base.PageBase):
  def __init__(self, story_set, base_dir, expectations):
    super(GPUProcessCrashesExactlyOnce, self).__init__(
      url='file://gpu_process_crash.html',
      page_set=story_set,
      base_dir=base_dir,
      name='GpuCrash.GPUProcessCrashesExactlyOnce',
      expectations=expectations)
    self.script_to_evaluate_on_commit = harness_script
    self.kill_gpu_process = True
    self.number_of_gpu_process_kills = 2
    self.check_crash_count = True
    self.force_garbage_collection = False
    self.hide_tab_and_lose_context = False

  def RunNavigateSteps(self, action_runner):
    super(GPUProcessCrashesExactlyOnce, self).RunNavigateSteps(
      action_runner)
    action_runner.WaitForJavaScriptCondition(
        'window.domAutomationController._loaded')

class WebGLContextLostFromGPUProcessExitPage(gpu_test_base.PageBase):
  def __init__(self, story_set, base_dir, expectations):
    super(WebGLContextLostFromGPUProcessExitPage, self).__init__(
      url='file://webgl.html?query=kill_after_notification',
      page_set=story_set,
      base_dir=base_dir,
      name='ContextLost.WebGLContextLostFromGPUProcessExit',
      expectations=expectations)
    self.script_to_evaluate_on_commit = harness_script
    self.kill_gpu_process = True
    self.check_crash_count = False
    self.number_of_gpu_process_kills = 1
    self.force_garbage_collection = False
    self.hide_tab_and_lose_context = False

  def RunNavigateSteps(self, action_runner):
    super(WebGLContextLostFromGPUProcessExitPage, self).RunNavigateSteps(
      action_runner)
    action_runner.WaitForJavaScriptCondition(
        'window.domAutomationController._loaded')


class WebGLContextLostFromLoseContextExtensionPage(gpu_test_base.PageBase):
  def __init__(self, story_set, base_dir, expectations):
    super(WebGLContextLostFromLoseContextExtensionPage, self).__init__(
      url='file://webgl.html?query=WEBGL_lose_context',
      page_set=story_set,
      base_dir=base_dir,
      name='ContextLost.WebGLContextLostFromLoseContextExtension',
      expectations=expectations)
    self.script_to_evaluate_on_commit = harness_script
    self.kill_gpu_process = False
    self.check_crash_count = False
    self.force_garbage_collection = False
    self.hide_tab_and_lose_context = False

  def RunNavigateSteps(self, action_runner):
    super(WebGLContextLostFromLoseContextExtensionPage, self).RunNavigateSteps(
      action_runner)
    action_runner.WaitForJavaScriptCondition(
        'window.domAutomationController._finished')


class WebGLContextLostInHiddenTabPage(gpu_test_base.PageBase):
  def __init__(self, story_set, base_dir, expectations):
    super(WebGLContextLostInHiddenTabPage, self).__init__(
      url='file://webgl.html?query=kill_after_notification',
      page_set=story_set,
      base_dir=base_dir,
      name='ContextLost.WebGLContextLostInHiddenTab',
      expectations=expectations)
    self.script_to_evaluate_on_commit = harness_script
    self.kill_gpu_process = False
    self.check_crash_count = False
    self.force_garbage_collection = False
    self.hide_tab_and_lose_context = True

  def RunNavigateSteps(self, action_runner):
    super(WebGLContextLostInHiddenTabPage, self).RunNavigateSteps(
      action_runner)
    action_runner.WaitForJavaScriptCondition(
        'window.domAutomationController._loaded')


class WebGLContextLostFromQuantityPage(gpu_test_base.PageBase):
  def __init__(self, story_set, base_dir, expectations):
    super(WebGLContextLostFromQuantityPage, self).__init__(
      url='file://webgl.html?query=forced_quantity_loss',
      page_set=story_set,
      base_dir=base_dir,
      name='ContextLost.WebGLContextLostFromQuantity',
      expectations=expectations)
    self.script_to_evaluate_on_commit = harness_script
    self.kill_gpu_process = False
    self.check_crash_count = False
    self.force_garbage_collection = True
    self.hide_tab_and_lose_context = False

  def RunNavigateSteps(self, action_runner):
    super(WebGLContextLostFromQuantityPage, self).RunNavigateSteps(
      action_runner)
    action_runner.WaitForJavaScriptCondition(
        'window.domAutomationController._loaded')

class WebGLContextLostFromSelectElementPage(gpu_test_base.PageBase):
  def __init__(self, story_set, base_dir, expectations):
    super(WebGLContextLostFromSelectElementPage, self).__init__(
      url='file://webgl_with_select_element.html',
      page_set=story_set,
      base_dir=base_dir,
      name='ContextLost.WebGLContextLostFromSelectElement',
      expectations=expectations)
    self.script_to_evaluate_on_commit = harness_script
    self.kill_gpu_process = False
    self.check_crash_count = False
    self.force_garbage_collection = False
    self.hide_tab_and_lose_context = False

  def RunNavigateSteps(self, action_runner):
    super(WebGLContextLostFromSelectElementPage, self).RunNavigateSteps(
      action_runner)
    action_runner.WaitForJavaScriptCondition(
        'window.domAutomationController._loaded')

class ContextLost(gpu_test_base.TestBase):
  enabled = True
  test = ContextLostValidator

  @classmethod
  def Name(cls):
    return 'context_lost'

  @classmethod
  def ShouldTearDownStateAfterEachStoryRun(cls):
    return True

  def _CreateExpectations(self):
    return context_lost_expectations.ContextLostExpectations()

  # For the record, this would have been another way to get the pages
  # to repeat. pageset_repeat would be another option.
  # options = {'page_repeat': 5}
  def CreateStorySet(self, options):
    ps = story_set_module.StorySet(
      base_dir=data_path,
      serving_dirs=set(['']))
    ps.AddStory(GPUProcessCrashesExactlyOnce(
        ps, ps.base_dir, self.GetExpectations()))
    ps.AddStory(WebGLContextLostFromGPUProcessExitPage(
        ps, ps.base_dir, self.GetExpectations()))
    ps.AddStory(WebGLContextLostFromLoseContextExtensionPage(
        ps, ps.base_dir, self.GetExpectations()))
    ps.AddStory(WebGLContextLostFromQuantityPage(
        ps, ps.base_dir, self.GetExpectations()))
    ps.AddStory(WebGLContextLostFromSelectElementPage(
        ps, ps.base_dir, self.GetExpectations()))
    ps.AddStory(WebGLContextLostInHiddenTabPage(
        ps, ps.base_dir, self.GetExpectations()))
    return ps
