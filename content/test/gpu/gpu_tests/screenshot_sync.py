# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import random

from gpu_tests import gpu_test_base
from gpu_tests import path_util
from gpu_tests import screenshot_sync_expectations

from telemetry import benchmark
from telemetry.page import page_test
from telemetry.story import story_set as story_set_module
from telemetry.util import image_util

data_path = os.path.join(
    path_util.GetChromiumSrcDir(), 'content', 'test', 'data', 'gpu')

class ScreenshotSyncValidator(gpu_test_base.ValidatorBase):
  def CustomizeBrowserOptions(self, options):
    # --test-type=gpu is used only to suppress the "Google API Keys are missing"
    # infobar, which causes flakiness in tests.
    options.AppendExtraBrowserArgs(['--force-gpu-rasterization',
                                    '--test-type=gpu'])

  def ValidateAndMeasurePage(self, page, tab, results):
    if not tab.screenshot_supported:
      raise page_test.Failure('Browser does not support screenshot capture')

    def CheckColorMatch(canvasRGB, screenshotRGB):
      for i in range(0, 3):
        if abs(canvasRGB[i] - screenshotRGB[i]) > 1:
          raise page_test.Failure('Color mismatch in component #%d: %d vs %d' %
              (i, canvasRGB[i], screenshotRGB[i]))

    def CheckScreenshot():
      canvasRGB = []
      for _ in range(0, 3):
        canvasRGB.append(random.randint(0, 255))
      tab.EvaluateJavaScript("window.draw(%d, %d, %d);" % tuple(canvasRGB))
      screenshot = tab.Screenshot(5)
      CheckColorMatch(canvasRGB, image_util.Pixels(screenshot))

    repetitions = 50
    for _ in range(0, repetitions):
      CheckScreenshot()

class ScreenshotSyncPage(gpu_test_base.PageBase):
  def __init__(self, story_set, base_dir, expectations):
    super(ScreenshotSyncPage, self).__init__(
      url='file://screenshot_sync.html',
      page_set=story_set,
      base_dir=base_dir,
      name='ScreenshotSync',
      expectations=expectations)


@benchmark.Disabled('linux', 'mac', 'win')
class ScreenshotSyncProcess(gpu_test_base.TestBase):
  """Tests that screenhots are properly synchronized with the frame one which
  they were requested"""
  test = ScreenshotSyncValidator

  @classmethod
  def Name(cls):
    return 'screenshot_sync'

  def _CreateExpectations(self):
    return screenshot_sync_expectations.ScreenshotSyncExpectations()

  def CreateStorySet(self, options):
    ps = story_set_module.StorySet(base_dir=data_path, serving_dirs=[''])
    ps.AddStory(ScreenshotSyncPage(ps, ps.base_dir, self.GetExpectations()))
    return ps
