# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import random

from gpu_tests import gpu_test_base
from gpu_tests import path_util
from gpu_tests import screenshot_sync_expectations

from telemetry.page import legacy_page_test
from telemetry.story import story_set as story_set_module
from telemetry.util import image_util
from telemetry.util import rgba_color

data_path = os.path.join(
    path_util.GetChromiumSrcDir(), 'content', 'test', 'data', 'gpu')


class SoftwareRasterSharedPageState(gpu_test_base.GpuSharedPageState):
  def __init__(self, test, finder_options, story_set):
    super(SoftwareRasterSharedPageState, self).__init__(
      test, finder_options, story_set)
    finder_options.browser_options.AppendExtraBrowserArgs(
      ['--disable-gpu-rasterization'])


class GPURasterSharedPageState(gpu_test_base.GpuSharedPageState):
  def __init__(self, test, finder_options, story_set):
    super(GPURasterSharedPageState, self).__init__(
      test, finder_options, story_set)
    finder_options.browser_options.AppendExtraBrowserArgs(
      ['--force-gpu-rasterization'])


class ScreenshotSyncValidator(gpu_test_base.ValidatorBase):
  def CustomizeBrowserOptions(self, options):
    # --test-type=gpu is used only to suppress the "Google API Keys are missing"
    # infobar, which causes flakiness in tests.
    options.AppendExtraBrowserArgs(['--test-type=gpu'])

  def ValidateAndMeasurePage(self, page, tab, results):
    if not tab.screenshot_supported:
      raise legacy_page_test.Failure(
          'Browser does not support screenshot capture')

    def CheckColorMatchAtLocation(expectedRGB, screenshot, x, y):
      pixel_value = image_util.GetPixelColor(screenshot, x, y)
      if not expectedRGB.IsEqual(pixel_value):
        error_message = ('Color mismatch at (%d, %d): expected (%d, %d, %d), ' +
                         'got (%d, %d, %d)') % (
                             x, y, expectedRGB.r, expectedRGB.g, expectedRGB.b,
                             pixel_value.r, pixel_value.g, pixel_value.b)
        raise legacy_page_test.Failure(error_message)

    def CheckScreenshot():
      canvasRGB = rgba_color.RgbaColor(random.randint(0, 255),
                                       random.randint(0, 255),
                                       random.randint(0, 255),
                                       255)
      tab.EvaluateJavaScript("window.draw(%d, %d, %d);" % (
          canvasRGB.r, canvasRGB.g, canvasRGB.b))
      screenshot = tab.Screenshot(5)
      start_x = 10
      start_y = 0
      outer_size = 256
      skip = 10
      for y in range(start_y, outer_size, skip):
        for x in range(start_x, outer_size, skip):
          CheckColorMatchAtLocation(canvasRGB, screenshot, x, y)

    repetitions = 20
    for _ in range(0, repetitions):
      CheckScreenshot()


class ScreenshotSyncPage(gpu_test_base.PageBase):
  def __init__(self, story_set, base_dir,
               shared_page_state_class,
               url, name, expectations):
    super(ScreenshotSyncPage, self).__init__(
      url=url,
      page_set=story_set,
      base_dir=base_dir,
      name=name,
      shared_page_state_class=shared_page_state_class,
      expectations=expectations)


class ScreenshotSyncStorySet(story_set_module.StorySet):
  """Test cases for screenshots being in sync with content updates."""
  def __init__(self, base_dir=None, serving_dirs=None):
    super(ScreenshotSyncStorySet, self).__init__(
      base_dir=base_dir, serving_dirs=serving_dirs)

  @property
  def allow_mixed_story_states(self):
    # Return True here in order to be able to run the same tests with
    # both software and GPU rasterization.
    return True


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
    ps = ScreenshotSyncStorySet(base_dir=data_path, serving_dirs=[''])
    ps.AddStory(ScreenshotSyncPage(ps, ps.base_dir,
                                   SoftwareRasterSharedPageState,
                                   'file://screenshot_sync_canvas.html',
                                   'ScreenshotSync.SWRasterWithCanvas',
                                   self.GetExpectations()))
    ps.AddStory(ScreenshotSyncPage(ps, ps.base_dir,
                                   SoftwareRasterSharedPageState,
                                   'file://screenshot_sync_divs.html',
                                   'ScreenshotSync.SWRasterWithDivs',
                                   self.GetExpectations()))
    ps.AddStory(ScreenshotSyncPage(ps, ps.base_dir,
                                   GPURasterSharedPageState,
                                   'file://screenshot_sync_canvas.html',
                                   'ScreenshotSync.GPURasterWithCanvas',
                                   self.GetExpectations()))
    ps.AddStory(ScreenshotSyncPage(ps, ps.base_dir,
                                   GPURasterSharedPageState,
                                   'file://screenshot_sync_divs.html',
                                   'ScreenshotSync.GPURasterWithDivs',
                                   self.GetExpectations()))
    return ps
