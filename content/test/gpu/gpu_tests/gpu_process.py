# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import gpu_process_expectations as expectations
import page_sets

from telemetry import benchmark
from telemetry.page import page_set
from telemetry.page import page_test

test_harness_script = r"""
  var domAutomationController = {};
  domAutomationController._finished = false;
  domAutomationController.setAutomationId = function(id) {}
  domAutomationController.send = function(msg) {
    domAutomationController._finished = true;
  }

  window.domAutomationController = domAutomationController;
"""

class _GpuProcessValidator(page_test.PageTest):
  def __init__(self):
    super(_GpuProcessValidator, self).__init__(
        needs_browser_restart_after_each_page=True)

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-gpu-benchmarking')

  def ValidateAndMeasurePage(self, page, tab, results):
    has_gpu_process_js = 'chrome.gpuBenchmarking.hasGpuProcess()'
    has_gpu_process = tab.EvaluateJavaScript(has_gpu_process_js)
    if not has_gpu_process:
      raise page_test.Failure('No GPU process detected')

class GpuProcess(benchmark.Benchmark):
  """Tests that accelerated content triggers the creation of a GPU process"""
  test = _GpuProcessValidator

  def CreateExpectations(self, page_set):
    return expectations.GpuProcessExpectations()

  def CreatePageSet(self, options):
    page_set = page_sets.GpuProcessTestsPageSet()
    for page in page_set.pages:
      page.script_to_evaluate_on_commit = test_harness_script
    return page_set
