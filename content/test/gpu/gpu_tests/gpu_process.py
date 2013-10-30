# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import gpu_process_expectations as expectations

from telemetry import test
from telemetry.page import page_set
from telemetry.page import page_test

class GpuProcessValidator(page_test.PageTest):
  def __init__(self):
    super(GpuProcessValidator, self).__init__('ValidatePage',
        needs_browser_restart_after_each_run=True)

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-gpu-benchmarking')

  def ValidatePage(self, page, tab, results):
    has_gpu_process_js = 'chrome.gpuBenchmarking.hasGpuProcess()'
    has_gpu_process = tab.EvaluateJavaScript(has_gpu_process_js)
    if not has_gpu_process:
      raise page_test.Failure('No GPU process detected')

class GpuProcess(test.Test):
  """Tests that accelerated content triggers the creation of a GPU process"""
  test = GpuProcessValidator
  page_set = 'page_sets/gpu_process_tests.json'

  def CreateExpectations(self, page_set):
    return expectations.GpuProcessExpectations()
