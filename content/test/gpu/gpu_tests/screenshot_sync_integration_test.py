# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import random
import sys

from gpu_tests import gpu_integration_test
from gpu_tests import path_util
from gpu_tests import screenshot_sync_expectations

from telemetry.util import image_util
from telemetry.util import rgba_color

data_path = os.path.join(
    path_util.GetChromiumSrcDir(), 'content', 'test', 'data', 'gpu')

class ScreenshotSyncIntegrationTest(gpu_integration_test.GpuIntegrationTest):
  """Tests that screenshots are properly synchronized with the frame on
  which they were requested.
  """

  # We store a deep copy of the original browser finder options in
  # order to be able to restart the browser multiple times, with a
  # different set of command line arguments each time.
  _original_finder_options = None

  # We keep track of the set of command line arguments used to launch
  # the browser most recently in order to figure out whether we need
  # to relaunch it, if a new pixel test requires a different set of
  # arguments.
  _last_launched_browser_args = set()

  @classmethod
  def Name(cls):
    """The name by which this test is invoked on the command line."""
    return 'screenshot_sync'

  @classmethod
  def SetUpProcess(cls):
    super(cls, ScreenshotSyncIntegrationTest).SetUpProcess()
    cls._original_finder_options = cls._finder_options.Copy()
    cls.CustomizeBrowserArgs([])
    cls.StartBrowser()
    cls.SetStaticServerDirs([data_path])

  @classmethod
  def CustomizeBrowserArgs(cls, browser_args):
    if not browser_args:
      browser_args = []
    cls._finder_options = cls._original_finder_options.Copy()
    browser_options = cls._finder_options.browser_options
    # All tests receive the following options. They aren't recorded in
    # the _last_launched_browser_args.
    #
    # --test-type=gpu is used only to suppress the "Google API Keys are missing"
    # infobar, which causes flakiness in tests.
    browser_options.AppendExtraBrowserArgs(['--test-type=gpu'])
    # Append the new arguments.
    browser_options.AppendExtraBrowserArgs(browser_args)
    cls._last_launched_browser_args = set(browser_args)
    cls.SetBrowserOptions(cls._finder_options)

  @classmethod
  def RestartBrowserIfNecessaryWithArgs(cls, browser_args):
    if not browser_args:
      browser_args = []
    if set(browser_args) != cls._last_launched_browser_args:
      logging.info('Restarting browser with arguments: ' + str(browser_args))
      cls.StopBrowser()
      cls.CustomizeBrowserArgs(browser_args)
      cls.StartBrowser()

  @classmethod
  def _CreateExpectations(cls):
    return screenshot_sync_expectations.ScreenshotSyncExpectations()

  @classmethod
  def GenerateGpuTests(cls, options):
    yield('ScreenshotSync_SWRasterWithCanvas',
          'screenshot_sync_canvas.html',
          ('--disable-gpu-rasterization'))
    yield('ScreenshotSync_SWRasterWithDivs',
          'screenshot_sync_divs.html',
          ('--disable-gpu-rasterization'))
    yield('ScreenshotSync_GPURasterWithCanvas',
          'screenshot_sync_canvas.html',
          ('--force-gpu-rasterization'))
    yield('ScreenshotSync_GPURasterWithDivs',
          'screenshot_sync_divs.html',
          ('--force-gpu-rasterization'))

  def _Navigate(self, test_path):
    url = self.UrlOfStaticFilePath(test_path)
    # It's crucial to use the action_runner, rather than the tab's
    # Navigate method directly. It waits for the document ready state
    # to become interactive or better, avoiding critical race
    # conditions.
    self.tab.action_runner.Navigate(url)

  def _CheckColorMatchAtLocation(self, expectedRGB, screenshot, x, y):
    pixel_value = image_util.GetPixelColor(screenshot, x, y)
    if not expectedRGB.IsEqual(pixel_value):
      error_message = ('Color mismatch at (%d, %d): expected (%d, %d, %d), ' +
                       'got (%d, %d, %d)') % (
                         x, y, expectedRGB.r, expectedRGB.g, expectedRGB.b,
                         pixel_value.r, pixel_value.g, pixel_value.b)
      self.fail(error_message)

  def _CheckScreenshot(self):
    canvasRGB = rgba_color.RgbaColor(random.randint(0, 255),
                                     random.randint(0, 255),
                                     random.randint(0, 255),
                                     255)
    tab = self.tab
    tab.EvaluateJavaScript(
        "window.draw({{ red }}, {{ green }}, {{ blue }});",
        red=canvasRGB.r, green=canvasRGB.g, blue=canvasRGB.b)
    screenshot = tab.Screenshot(5)
    start_x = 10
    start_y = 0
    outer_size = 256
    skip = 10
    for y in range(start_y, outer_size, skip):
      for x in range(start_x, outer_size, skip):
        self._CheckColorMatchAtLocation(canvasRGB, screenshot, x, y)

  def RunActualGpuTest(self, test_path, *args):
    browser_arg = args[0]
    self.RestartBrowserIfNecessaryWithArgs([browser_arg])
    self._Navigate(test_path)
    repetitions = 20
    for _ in range(0, repetitions):
      self._CheckScreenshot()

def load_tests(loader, tests, pattern):
  del loader, tests, pattern  # Unused.
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])
