# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs a Google Maps pixel test.
Performs several common navigation actions on the map (pan, zoom, rotate) then
captures a screenshot and compares selected pixels against expected values"""

import json
import os
import re

from telemetry import test
from telemetry.core.backends import png_bitmap
from telemetry.core import util
from telemetry.page import page_test
from telemetry.page import page_set

class MapsValidator(page_test.PageTest):
  def __init__(self):
    super(MapsValidator, self).__init__('ValidatePage')

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-gpu-benchmarking')

  def ValidatePage(self, page, tab, results):
    if not tab.screenshot_supported:
      raise page_test.Failure('Browser does not support screenshot capture')
    screenshot = tab.Screenshot(5)
    if not screenshot:
      raise page_test.Failure('Could not capture screenshot')

    expected = MapsValidator.ReadPixelExpectations(page)
    MapsValidator.CompareToExpectations(screenshot, expected)

  @staticmethod
  def ReadPixelExpectations(page):
    expectations_path = os.path.join(page._base_dir, page.pixel_expectations)
    with open(expectations_path, 'r') as f:
      json_contents = json.load(f)
    return json_contents

  @staticmethod
  def CompareToExpectations(screenshot, expectations):
    for expectation in expectations:
      location = expectation["location"]
      pixel_color = screenshot.GetPixelColor(location[0], location[1])
      expect_color = png_bitmap.PngColor(
          expectation["color"][0],
          expectation["color"][1],
          expectation["color"][2])
      iter_result = pixel_color.IsEqual(expect_color, expectation["tolerance"])
      if not iter_result:
        raise page_test.Failure('FAILURE: Expected pixel at ' + str(location) +
            ' to be ' +
            str(expectation["color"]) + " but got [" +
            str(pixel_color.r) + ", " +
            str(pixel_color.g) + ", " +
            str(pixel_color.b) + "]")

class Maps(test.Test):
  """Google Maps pixel tests."""
  test = MapsValidator

  def CreatePageSet(self, options):
    page_set_path = os.path.join(
        util.GetChromiumSrcDir(), 'content', 'test', 'gpu', 'page_sets')
    page_set_dict = {
      'archive_data_file': 'data/maps.json',
      'make_javascript_deterministic': False,
      'pages': [
        {
          'url': 'http://localhost:10020/tracker.html',
          'navigate_steps': [
            { 'action': 'navigate' },
            { 'action': 'wait', 'javascript': 'window.testDone' }
          ],
          'pixel_expectations': 'data/maps_001_expectations.json'
        }
      ]
    }

    return page_set.PageSet.FromDict(page_set_dict, page_set_path)
