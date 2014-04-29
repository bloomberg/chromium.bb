# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os

from telemetry import test as test_module
from telemetry.core import util
from telemetry.page import page_set
from telemetry.page import page_test

data_path = os.path.join(
    util.GetChromiumSrcDir(), 'content', 'test', 'data', 'gpu')

wait_timeout = 20  # seconds

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

  window.domAutomationController = domAutomationController;
  console.log("Harness injected.");
"""

class _ContextLostValidator(page_test.PageTest):
  def __init__(self):
    # Strictly speaking this test doesn't yet need a browser restart
    # after each run, but if more tests are added which crash the GPU
    # process, then it will.
    super(_ContextLostValidator, self).__init__(
      'ValidatePage', needs_browser_restart_after_each_page=True)

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs(
        '--disable-domain-blocking-for-3d-apis')
    options.AppendExtraBrowserArgs(
        '--disable-gpu-process-crash-limit')
    # Required for about:gpucrash handling from Telemetry.
    options.AppendExtraBrowserArgs('--enable-gpu-benchmarking')

  def ValidatePage(self, page, tab, results):
    if page.kill_gpu_process:
      # Doing the GPU process kill operation cooperatively -- in the
      # same page's context -- is much more stressful than restarting
      # the browser every time.
      for x in range(page.number_of_gpu_process_kills):
        if not tab.browser.supports_tab_control:
          raise page_test.Failure('Browser must support tab control')
        # Reset the test's state.
        tab.EvaluateJavaScript(
          'window.domAutomationController._succeeded = false');
        tab.EvaluateJavaScript(
          'window.domAutomationController._finished = false');
        # Crash the GPU process.
        new_tab = tab.browser.tabs.New()
        # To access these debug URLs from Telemetry, they have to be
        # written using the chrome:// scheme.
        new_tab.Navigate('chrome://gpucrash')
        # Activate the original tab and wait for completion.
        tab.Activate()
        completed = False
        try:
          util.WaitFor(lambda: tab.EvaluateJavaScript(
              'window.domAutomationController._finished'), wait_timeout)
          completed = True
        except util.TimeoutException:
          pass
        new_tab.Close()
        if not completed:
          raise page_test.Failure(
              'Test didn\'t complete (no context lost event?)')
        if not tab.EvaluateJavaScript(
          'window.domAutomationController._succeeded'):
          raise page_test.Failure(
            'Test failed (context not restored properly?)')

class ContextLost(test_module.Test):
  enabled = True
  test = _ContextLostValidator
  # For the record, this would have been another way to get the pages
  # to repeat. pageset_repeat would be another option.
  # options = {'page_repeat': 5}
  def CreatePageSet(self, options):
    page_set_dict = {
      'description': 'Test cases for real and synthetic context lost events',
      'user_agent_type': 'desktop',
      'serving_dirs': [''],
      'pages': [
        {
          'name': 'ContextLost.WebGLContextLostFromGPUProcessExit',
          'url': 'file://webgl.html?query=kill_after_notification',
          'script_to_evaluate_on_commit': harness_script,
          'navigate_steps': [
            { 'action': 'navigate' },
            { 'action': 'wait',
              'javascript': 'window.domAutomationController._loaded' }
          ],
          'kill_gpu_process': True,
          'number_of_gpu_process_kills': 1,
        },
        {
          'name': 'ContextLost.WebGLContextLostFromLoseContextExtension',
          'url': 'file://webgl.html?query=WEBGL_lose_context',
          'script_to_evaluate_on_commit': harness_script,
          'navigate_steps': [
            { 'action': 'navigate' },
            { 'action': 'wait',
              'javascript': 'window.domAutomationController._finished' }
          ],
          'kill_gpu_process': False
        },
      ]
    }
    return page_set.PageSet.FromDict(page_set_dict, data_path)
