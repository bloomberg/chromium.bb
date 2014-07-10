# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs a Google Maps pixel test.
Performs several common navigation actions on the map (pan, zoom, rotate) then
captures a screenshot and compares selected pixels against expected values"""

import json
import optparse
import os

import cloud_storage_test_base
import maps_expectations

from telemetry import benchmark
from telemetry.core import bitmap
from telemetry.core import util
from telemetry.page import page
from telemetry.page import page_set
from telemetry.page import page_test

class _MapsValidator(cloud_storage_test_base.ValidatorBase):
  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-gpu-benchmarking')

  def ValidatePage(self, page, tab, results):
    # TODO: This should not be necessary, but it's not clear if the test is
    # failing on the bots in it's absence. Remove once we can verify that it's
    # safe to do so.
    _MapsValidator.SpinWaitOnRAF(tab, 3)

    if not tab.screenshot_supported:
      raise page_test.Failure('Browser does not support screenshot capture')
    screenshot = tab.Screenshot(5)
    if not screenshot:
      raise page_test.Failure('Could not capture screenshot')

    dpr = tab.EvaluateJavaScript('window.devicePixelRatio')
    expected = self._ReadPixelExpectations(page)
    self._ValidateScreenshotSamples(
        page.display_name, screenshot, expected, dpr)

  @staticmethod
  def SpinWaitOnRAF(tab, iterations, timeout = 60):
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


class MapsPage(page.Page):
  def __init__(self, page_set, base_dir):
    super(MapsPage, self).__init__(
      url='http://localhost:10020/tracker.html',
      page_set=page_set,
      base_dir=base_dir,
      name='Maps.maps_002')
    self.pixel_expectations = 'data/maps_002_expectations.json'

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.WaitForJavaScriptCondition(
        'window.testDone', timeout_in_seconds=180)


class Maps(cloud_storage_test_base.TestBase):
  """Google Maps pixel tests."""
  test = _MapsValidator

  def CreateExpectations(self, page_set):
    return maps_expectations.MapsExpectations()

  def CreatePageSet(self, options):
    page_set_path = os.path.join(
        util.GetChromiumSrcDir(), 'content', 'test', 'gpu', 'page_sets')
    ps = page_set.PageSet(archive_data_file='data/maps.json',
                          make_javascript_deterministic=False,
                          file_path=page_set_path)
    ps.AddPage(MapsPage(ps, ps.base_dir))
    return ps
