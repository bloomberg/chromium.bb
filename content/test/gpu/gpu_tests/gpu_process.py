# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from gpu_tests import gpu_process_expectations as expectations
from gpu_tests import gpu_test_base
import page_sets

from telemetry.page import legacy_page_test

test_harness_script = r"""
  var domAutomationController = {};
  domAutomationController._finished = false;
  domAutomationController.setAutomationId = function(id) {}
  domAutomationController.send = function(msg) {
    domAutomationController._finished = true;
  }

  window.domAutomationController = domAutomationController;

  function GetDriverBugWorkarounds() {
    var query_result = document.querySelector('.workarounds-list');
    var browser_list = []
    for (var i=0; i < query_result.childElementCount; i++)
      browser_list.push(query_result.children[i].textContent);
    return browser_list;
  };
"""

class GpuProcessValidator(gpu_test_base.ValidatorBase):
  def __init__(self):
    super(GpuProcessValidator, self).__init__(
        needs_browser_restart_after_each_page=True)

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-gpu-benchmarking')

  def ValidateAndMeasurePage(self, page, tab, results):
    if hasattr(page, 'Validate'):
      page.Validate(tab, results)
    else:
      has_gpu_channel_js = 'chrome.gpuBenchmarking.hasGpuChannel()'
      has_gpu_channel = tab.EvaluateJavaScript(has_gpu_channel_js)
      if not has_gpu_channel:
        raise legacy_page_test.Failure('No GPU channel detected')

class GpuProcess(gpu_test_base.TestBase):
  """Tests that accelerated content triggers the creation of a GPU process"""
  test = GpuProcessValidator

  @classmethod
  def Name(cls):
    return 'gpu_process'

  def _CreateExpectations(self):
    return expectations.GpuProcessExpectations()

  def CreateStorySet(self, options):
    browser_type = options.browser_options.browser_type
    is_platform_android = browser_type.startswith('android')
    story_set = page_sets.GpuProcessTestsStorySet(self.GetExpectations(),
                                                  is_platform_android)
    for page in story_set:
      page.script_to_evaluate_on_commit = test_harness_script
    return story_set
