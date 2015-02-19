# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import random

import screenshot_sync_expectations as expectations

from telemetry import benchmark
from telemetry.core import util
from telemetry.image_processing import image_util
from telemetry.page import page
from telemetry.page import page_set
from telemetry.page import page_test

data_path = os.path.join(
    util.GetChromiumSrcDir(), 'content', 'test', 'data', 'gpu')

class _ScreenshotSyncValidator(page_test.PageTest):
  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--force-gpu-rasterization')

  def ValidateAndMeasurePage(self, page, tab, results):
    if not tab.screenshot_supported:
      raise page_test.Failure('Browser does not support screenshot capture')

    def CheckColorMatch(canvasRGB, screenshotRGB):
      for i in range(0, 3):
        if abs(canvasRGB[i] - screenshotRGB[i]) > 1:
          raise page_test.Failure('Color mismatch in component #%d: %d vs %d' %
              (i, canvasRGB[i], screenshotRGB[i]))

    def CheckScreenshot():
      canvasRGB = [];
      for i in range(0, 3):
        canvasRGB.append(random.randint(0, 255))
      tab.EvaluateJavaScript("window.draw(%d, %d, %d);" % tuple(canvasRGB))
      screenshot = tab.Screenshot(5)
      CheckColorMatch(canvasRGB, image_util.Pixels(screenshot))

    repetitions = 50
    for n in range(0, repetitions):
      CheckScreenshot()

class ScreenshotSyncPage(page.Page):
  def __init__(self, page_set, base_dir):
    super(ScreenshotSyncPage, self).__init__(
      url='file://screenshot_sync.html',
      page_set=page_set,
      base_dir=base_dir,
      name='ScreenshotSync')
    self.user_agent_type = 'desktop'

  def RunNavigateSteps(self, action_runner):
    super(ScreenshotSyncPage, self).RunNavigateSteps(action_runner)


@benchmark.Disabled('linux', 'mac', 'win')
class ScreenshotSyncProcess(benchmark.Benchmark):
  """Tests that screenhots are properly synchronized with the frame one which
  they were requested"""
  test = _ScreenshotSyncValidator

  @classmethod
  def Name(cls):
    return 'screenshot_sync'

  def CreateExpectations(self):
    return expectations.ScreenshotSyncExpectations()

  def CreatePageSet(self, options):
    ps = page_set.PageSet(file_path=data_path, serving_dirs=[''])
    ps.AddUserStory(ScreenshotSyncPage(ps, ps.base_dir))
    return ps
