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

from telemetry import test
from telemetry.core import bitmap
from telemetry.core import util
from telemetry.page import page_test
from telemetry.page import page_set

class _MapsValidator(cloud_storage_test_base.ValidatorBase):
  def __init__(self):
    super(_MapsValidator, self).__init__('ValidatePage')

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

class Maps(cloud_storage_test_base.TestBase):
  """Google Maps pixel tests."""
  test = _MapsValidator

  def CreateExpectations(self, page_set):
    return maps_expectations.MapsExpectations()

  def CreatePageSet(self, options):
    page_set_path = os.path.join(
        util.GetChromiumSrcDir(), 'content', 'test', 'gpu', 'page_sets')
    page_set_dict = {
      'archive_data_file': 'data/maps.json',
      'make_javascript_deterministic': False,
      'pages': [
        {
          'name': 'Maps.maps_001',
          'url': 'http://localhost:10020/tracker.html',
          # TODO: Hack to prevent maps from scaling due to window size.
          # Remove when the maps team provides a better way of overriding this
          # behavior
          'script_to_evaluate_on_commit': 'window.screen = null;',
          'navigate_steps': [
            { 'action': 'navigate' },
            { 'action': 'wait', 'javascript': 'window.testDone' }
          ],
          'pixel_expectations': 'data/maps_001_expectations.json'
        }
      ]
    }

    return page_set.PageSet.FromDict(page_set_dict, page_set_path)
