# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re

from telemetry import test
from telemetry.page import page_test

SINGLE_TAB_LIMIT_MB = 128
WIGGLE_ROOM_MB = 4


class MemoryValidator(page_test.PageTest):
  def __init__(self):
    super(MemoryValidator, self).__init__('ValidatePage')
    with open(os.path.join(os.path.dirname(__file__), 'memory.js'), 'r') as f:
      script = f.read()
      m = re.search('var MEMORY_LIMIT_MB = (\d+);', script)
      if not m:
        raise page_test.Failure('Fail to query MEMORY_LIMIT_MB from memory.js')
      self.MEMORY_LIMIT_MB = int(m.group(1))


  def InjectJavascript(self):
    return [os.path.join(os.path.dirname(__file__), 'memory.js')]

  def ValidatePage(self, page, tab, results):
    mb_used = MemoryValidator.GpuMemoryUsageMbytes(tab)

    if mb_used + WIGGLE_ROOM_MB < SINGLE_TAB_LIMIT_MB:
      raise page_test.Failure('Memory allocation too low')

    if mb_used - WIGGLE_ROOM_MB > self.MEMORY_LIMIT_MB:
      raise page_test.Failure('Memory allocation too high')

  @staticmethod
  def GpuMemoryUsageMbytes(tab):
    gpu_rendering_stats_js = 'chrome.gpuBenchmarking.gpuRenderingStats()'
    gpu_rendering_stats = tab.EvaluateJavaScript(gpu_rendering_stats_js)
    return gpu_rendering_stats['globalVideoMemoryBytesAllocated'] / 1048576

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-logging')
    options.AppendExtraBrowserArgs(
        '--force-gpu-mem-available-mb=%s' % self.MEMORY_LIMIT_MB)
    options.AppendExtraBrowserArgs('--enable-gpu-benchmarking')

class Memory(test.Test):
  """Tests GPU memory limits"""
  test = MemoryValidator
  page_set = 'page_sets/memory_tests.json'
