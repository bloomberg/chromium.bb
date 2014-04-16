# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry import test
from telemetry.page import page_test
from telemetry.core.timeline import counter
from telemetry.core.timeline import model

MEMORY_LIMIT_MB = 256
SINGLE_TAB_LIMIT_MB = 128
WIGGLE_ROOM_MB = 4

test_harness_script = r"""
  var domAutomationController = {};
  domAutomationController._finished = false;

  domAutomationController.send = function(msg) {
    // This should wait until all effects of memory management complete.
    // We will need to wait until all
    // 1. pending commits from the main thread to the impl thread in the
    //    compositor complete (for visible compositors).
    // 2. allocations that the renderer's impl thread will make due to the
    //    compositor and WebGL are completed.
    // 3. pending GpuMemoryManager::Manage() calls to manage are made.
    // 4. renderers' OnMemoryAllocationChanged callbacks in response to
    //    manager are made.
    // Each step in this sequence can cause trigger the next (as a 1-2-3-4-1
    // cycle), so we will need to pump this cycle until it stabilizes.

    // Pump the cycle 8 times (in principle it could take an infinite number
    // of iterations to settle).

    var rafCount = 0;
    var totalRafCount = 8;

    function pumpRAF() {
      if (rafCount == totalRafCount) {
        domAutomationController._finished = true;
        return;
      }
      ++rafCount;
      window.requestAnimationFrame(pumpRAF);
    }
    pumpRAF();
  }

  window.domAutomationController = domAutomationController;

  window.addEventListener("load", function() {
    useGpuMemory(%d);
  }, false);
""" % MEMORY_LIMIT_MB

class _MemoryValidator(page_test.PageTest):
  def __init__(self):
    super(_MemoryValidator, self).__init__('ValidatePage')

  def ValidatePage(self, page, tab, results):
    timeline_data = tab.browser.StopTracing()
    timeline_model = model.TimelineModel(timeline_data)
    for process in timeline_model.GetAllProcesses():
      if 'gpu.GpuMemoryUsage' in process.counters:
        counter = process.GetCounter('gpu', 'GpuMemoryUsage')
        mb_used = counter.samples[-1] / 1048576

    if mb_used + WIGGLE_ROOM_MB < SINGLE_TAB_LIMIT_MB:
      raise page_test.Failure('Memory allocation too low')

    if mb_used - WIGGLE_ROOM_MB > MEMORY_LIMIT_MB:
      raise page_test.Failure('Memory allocation too high')

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-logging')
    options.AppendExtraBrowserArgs(
        '--force-gpu-mem-available-mb=%s' % MEMORY_LIMIT_MB)

  def WillNavigateToPage(self, page, tab):
    custom_categories = ['webkit.console', 'gpu']
    tab.browser.StartTracing(','.join(custom_categories), 60)

class Memory(test.Test):
  """Tests GPU memory limits"""
  test = _MemoryValidator
  page_set = 'page_sets/memory_tests.py'

  def CreatePageSet(self, options):
    page_set = super(Memory, self).CreatePageSet(options)
    for page in page_set.pages:
      page.script_to_evaluate_on_commit = test_harness_script
    return page_set
