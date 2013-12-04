# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs a Google Maps pixel test.
Performs several common navigation actions on the map (pan, zoom, rotate) then
captures a screenshot and compares selected pixels against expected values"""

import json
import optparse
import os
import re

import maps_expectations

from telemetry import test
from telemetry.core import bitmap
from telemetry.core import util
from telemetry.page import page_test
from telemetry.page import page_set

test_data_dir = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..', 'data', 'gpu'))

default_generated_data_dir = os.path.join(test_data_dir, 'generated')

class MapsValidator(page_test.PageTest):
  def __init__(self):
    super(MapsValidator, self).__init__('ValidatePage')

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-gpu-benchmarking')

  def ValidatePage(self, page, tab, results):
    # TODO: This should not be necessary, but it's not clear if the test is
    # failing on the bots in it's absence. Remove once we can verify that it's
    # safe to do so.
    MapsValidator.SpinWaitOnRAF(tab, 3)

    if not tab.screenshot_supported:
      raise page_test.Failure('Browser does not support screenshot capture')
    screenshot = tab.Screenshot(5)
    if not screenshot:
      raise page_test.Failure('Could not capture screenshot')

    dpr = tab.EvaluateJavaScript('window.devicePixelRatio')
    expected = MapsValidator.ReadPixelExpectations(page)

    try:
      MapsValidator.CompareToExpectations(screenshot, expected, dpr)
    except page_test.Failure:
      image_name = MapsValidator.UrlToImageName(page.display_name)
      MapsValidator.WriteErrorImage(self.options.generated_dir,
          image_name, self.options.build_revision, screenshot)
      raise

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

  @staticmethod
  def ReadPixelExpectations(page):
    expectations_path = os.path.join(page._base_dir, page.pixel_expectations)
    with open(expectations_path, 'r') as f:
      json_contents = json.load(f)
    return json_contents

  @staticmethod
  def CompareToExpectations(screenshot, expectations, devicePixelRatio):
    for expectation in expectations:
      location = expectation["location"]
      x = location[0] * devicePixelRatio
      y = location[1] * devicePixelRatio

      if x < 0 or y < 0 or x > screenshot.width or y > screenshot.height:
        raise page_test.Failure(
          'Expected pixel location [%d, %d] is out of range on [%d, %d] image' %
          (x, y, screenshot.width, screenshot.height))

      pixel_color = screenshot.GetPixelColor(x, y)
      expect_color = bitmap.RgbaColor(
          expectation["color"][0],
          expectation["color"][1],
          expectation["color"][2])
      iter_result = pixel_color.IsEqual(expect_color, expectation["tolerance"])
      if not iter_result:
        raise page_test.Failure('Expected pixel at ' + str(location) +
            ' to be ' +
            str(expectation["color"]) + " but got [" +
            str(pixel_color.r) + ", " +
            str(pixel_color.g) + ", " +
            str(pixel_color.b) + "]")

  @staticmethod
  def UrlToImageName(url):
    image_name = re.sub(r'^(http|https|file)://(/*)', '', url)
    image_name = re.sub(r'\.\./', '', image_name)
    image_name = re.sub(r'(\.|/|-)', '_', image_name)
    return image_name

  @staticmethod
  def WriteErrorImage(img_dir, img_name, build_revision, screenshot):
    full_image_name = img_name + '_' + str(build_revision)
    full_image_name = full_image_name + '.png'

    # This is a nasty and temporary hack: The pixel test archive step will copy
    # DIFF images directly, but for FAIL images it also requires a ref. This
    # allows us to archive the erronous image while the archiving step is being
    # refactored
    image_path = os.path.join(img_dir, 'DIFF_' + full_image_name)

    output_dir = os.path.dirname(image_path)
    if not os.path.exists(output_dir):
      os.makedirs(output_dir)

    screenshot.WritePngFile(image_path)

class Maps(test.Test):
  """Google Maps pixel tests."""
  test = MapsValidator

  @staticmethod
  def AddTestCommandLineOptions(parser):
    group = optparse.OptionGroup(parser, 'Maps test options')
    group.add_option('--generated-dir',
        help='Overrides the default location for generated test images that '
        'fail expectations checks',
        default=default_generated_data_dir)
    group.add_option('--build-revision',
        help='Chrome revision being tested.',
        default="unknownrev")
    parser.add_option_group(group)

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
