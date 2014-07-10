# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os

import screenshot_sync_expectations as expectations

from telemetry import benchmark
from telemetry.core import util
from telemetry.page import page
from telemetry.page import page_set
from telemetry.page import page_test

data_path = os.path.join(
    util.GetChromiumSrcDir(), 'content', 'test', 'data', 'gpu')

@benchmark.Disabled('mac')
class _ScreenshotSyncValidator(page_test.PageTest):
  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-gpu-benchmarking')

  def ValidatePage(self, page, tab, results):
    test_success = tab.EvaluateJavaScript('window.__testSuccess')
    if not test_success:
      message = tab.EvaluateJavaScript('window.__testMessage')
      raise page_test.Failure(message)


@benchmark.Disabled('mac')
class ScreenshotSyncPage(page.Page):
  def __init__(self, page_set, base_dir):
    super(ScreenshotSyncPage, self).__init__(
      url='file://screenshot_sync.html',
      page_set=page_set,
      base_dir=base_dir,
      name='ScreenshotSync')
    self.user_agent_type = 'desktop'

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.WaitForJavaScriptCondition(
        'window.__testComplete', timeout_in_seconds=120)


@benchmark.Disabled('mac')
class ScreenshotSyncProcess(benchmark.Benchmark):
  """Tests that screenhots are properly synchronized with the frame one which
  they were requested"""
  test = _ScreenshotSyncValidator

  def CreateExpectations(self, page_set):
    return expectations.ScreenshotSyncExpectations()

  def CreatePageSet(self, options):
    ps = page_set.PageSet(file_path=data_path, serving_dirs=[''])
    ps.AddPage(ScreenshotSyncPage(ps, ps.base_dir))
    return ps
