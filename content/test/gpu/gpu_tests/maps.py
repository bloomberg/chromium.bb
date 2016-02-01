# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs a Google Maps pixel test.
Performs several common navigation actions on the map (pan, zoom, rotate) then
captures a screenshot and compares selected pixels against expected values"""

import json
import os

from gpu_tests import cloud_storage_test_base
from gpu_tests import gpu_test_base
from gpu_tests import maps_expectations
from gpu_tests import path_util

from telemetry.core import util
from telemetry.page import page_test
from telemetry import story as story_module
from telemetry.story import story_set as story_set_module


class MapsValidator(cloud_storage_test_base.ValidatorBase):
  def __init__(self):
    super(MapsValidator, self).__init__()

  def CustomizeBrowserOptions(self, options):
    # --test-type=gpu is used only to suppress the "Google API Keys are missing"
    # infobar, which causes flakiness in tests.
    options.AppendExtraBrowserArgs(['--enable-gpu-benchmarking',
                                    '--test-type=gpu'])

  def ValidateAndMeasurePage(self, page, tab, results):
    # TODO: This should not be necessary, but it's not clear if the test is
    # failing on the bots in it's absence. Remove once we can verify that it's
    # safe to do so.
    MapsValidator.SpinWaitOnRAF(tab, 3)

    if not tab.screenshot_supported:
      raise page_test.Failure('Browser does not support screenshot capture')
    screenshot = tab.Screenshot(5)
    if screenshot is None:
      raise page_test.Failure('Could not capture screenshot')

    dpr = tab.EvaluateJavaScript('window.devicePixelRatio')
    expected = self._ReadPixelExpectations(page)
    self._ValidateScreenshotSamples(
        page.display_name, screenshot, expected, dpr)

  @staticmethod
  def SpinWaitOnRAF(tab, iterations, timeout=60):
    waitScript = r"""
      window.__spinWaitOnRAFDone = false;
      var iterationsLeft = %d;

      function spin() {
        iterationsLeft--;
        if (iterationsLeft == 0) {
          window.__spinWaitOnRAFDone = true;
          return;
        }
        window.requestAnimationFrame(spin);
      }
      window.requestAnimationFrame(spin);
    """ % iterations

    def IsWaitComplete():
      return tab.EvaluateJavaScript('window.__spinWaitOnRAFDone')

    tab.ExecuteJavaScript(waitScript)
    util.WaitFor(IsWaitComplete, timeout)

  def _ReadPixelExpectations(self, page):
    expectations_path = os.path.join(page._base_dir, page.pixel_expectations)
    with open(expectations_path, 'r') as f:
      json_contents = json.load(f)
    return json_contents


class MapsPage(gpu_test_base.PageBase):
  def __init__(self, story_set, base_dir, expectations):
    super(MapsPage, self).__init__(
        url='http://localhost:10020/tracker.html',
        page_set=story_set,
        base_dir=base_dir,
        name='Maps.maps_002',
        make_javascript_deterministic=False,
        expectations=expectations)
    self.pixel_expectations = 'data/maps_002_expectations.json'

  def RunNavigateSteps(self, action_runner):
    super(MapsPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'window.testDone', timeout_in_seconds=180)


class Maps(cloud_storage_test_base.TestBase):
  """Google Maps pixel tests."""
  test = MapsValidator

  @classmethod
  def Name(cls):
    return 'maps'

  def _CreateExpectations(self):
    return maps_expectations.MapsExpectations()

  def CreateStorySet(self, options):
    story_set_path = os.path.join(
        path_util.GetChromiumSrcDir(), 'content', 'test', 'gpu', 'page_sets')
    ps = story_set_module.StorySet(
        archive_data_file='data/maps.json',
        base_dir=story_set_path,
        cloud_storage_bucket=story_module.PUBLIC_BUCKET)
    ps.AddStory(MapsPage(ps, ps.base_dir, self.GetExpectations()))
    return ps
