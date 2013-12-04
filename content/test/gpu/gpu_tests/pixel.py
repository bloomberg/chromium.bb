# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from datetime import datetime
import glob
import optparse
import os
import re

from telemetry import test
from telemetry.core import bitmap
from telemetry.page import page_test

test_data_dir = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..', 'data', 'gpu'))

default_generated_data_dir = os.path.join(test_data_dir, 'generated')
default_reference_image_dir = os.path.join(test_data_dir, 'gpu_reference')

test_harness_script = r"""
  var domAutomationController = {};

  domAutomationController._succeeded = false;
  domAutomationController._finished = false;

  domAutomationController.setAutomationId = function(id) {}

  domAutomationController.send = function(msg) {
    domAutomationController._finished = true;

    if(msg.toLowerCase() == "success") {
      domAutomationController._succeeded = true;
    } else {
      domAutomationController._succeeded = false;
    }
  }

  window.domAutomationController = domAutomationController;
"""

class PixelTestFailure(Exception):
  pass

def _DidTestSucceed(tab):
  return tab.EvaluateJavaScript('domAutomationController._succeeded')

class PixelValidator(page_test.PageTest):
  def __init__(self):
    super(PixelValidator, self).__init__('ValidatePage')

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-gpu-benchmarking')

  def ValidatePage(self, page, tab, results):
    if not _DidTestSucceed(tab):
      raise page_test.Failure('Page indicated a failure')

    if not tab.screenshot_supported:
      raise page_test.Failure('Browser does not support screenshot capture')

    screenshot = tab.Screenshot(5)

    if not screenshot:
      raise page_test.Failure('Could not capture screenshot')

    if hasattr(page, 'test_rect'):
      screenshot = screenshot.Crop(
          page.test_rect[0], page.test_rect[1],
          page.test_rect[2], page.test_rect[3])

    image_name = PixelValidator.UrlToImageName(page.display_name)

    ref_png = PixelValidator.GetReferenceImage(self.options.reference_dir,
        image_name, page.revision, screenshot)

    # Test new snapshot against existing reference image
    if not ref_png.IsEqual(screenshot, tolerance=2):
      PixelValidator.WriteErrorImages(self.options.generated_dir, image_name,
          self.options.build_revision, screenshot, ref_png)
      raise page_test.Failure('Reference image did not match captured screen')

  @staticmethod
  def UrlToImageName(url):
    image_name = re.sub(r'^(http|https|file)://(/*)', '', url)
    image_name = re.sub(r'\.\./', '', image_name)
    image_name = re.sub(r'(\.|/|-)', '_', image_name)
    return image_name

  @staticmethod
  def DeleteOldReferenceImages(ref_image_path, cur_revision):
    if not cur_revision:
      return

    old_revisions = glob.glob(ref_image_path + "_*.png")
    for rev_path in old_revisions:
      m = re.match(r'^.*_(\d+)\.png$', rev_path)
      if m and int(m.group(1)) < cur_revision:
        print 'Found deprecated reference image. Deleting rev ' + m.group(1)
        os.remove(rev_path)

  @staticmethod
  def GetReferenceImage(img_dir, img_name, cur_revision, screenshot):
    if not cur_revision:
      cur_revision = 0

    image_path = os.path.join(img_dir, img_name)

    PixelValidator.DeleteOldReferenceImages(image_path, cur_revision)

    image_path = image_path + '_' + str(cur_revision) + '.png'

    try:
      ref_png = bitmap.Bitmap.FromPngFile(image_path)
    except IOError:
      ref_png = None

    if ref_png:
      return ref_png

    print 'Reference image not found. Writing tab contents as reference.'

    PixelValidator.WriteImage(image_path, screenshot)
    return screenshot

  @staticmethod
  def WriteErrorImages(img_dir, img_name, build_revision, screenshot, ref_png):
    full_image_name = img_name + '_' + str(build_revision)
    full_image_name = full_image_name + '.png'

    # Save the reference image
    # This ensures that we get the right revision number
    PixelValidator.WriteImage(
        os.path.join(img_dir, full_image_name), ref_png)

    PixelValidator.WriteImage(
        os.path.join(img_dir, 'FAIL_' + full_image_name), screenshot)

    diff_png = screenshot.Diff(ref_png)
    PixelValidator.WriteImage(
        os.path.join(img_dir, 'DIFF_' + full_image_name), diff_png)

  @staticmethod
  def WriteImage(image_path, png_image):
    output_dir = os.path.dirname(image_path)
    if not os.path.exists(output_dir):
      os.makedirs(output_dir)

    png_image.WritePngFile(image_path)

class Pixel(test.Test):
  test = PixelValidator
  page_set = 'page_sets/pixel_tests.json'

  @staticmethod
  def AddTestCommandLineOptions(parser):
    group = optparse.OptionGroup(parser, 'Pixel test options')
    group.add_option('--generated-dir',
        help='Overrides the default location for generated test images that do '
        'not match reference images',
        default=default_generated_data_dir)
    group.add_option('--reference-dir',
        help='Overrides the default location for reference images',
        default=default_reference_image_dir)
    group.add_option('--build-revision',
        help='Chrome revision being tested.',
        default="unknownrev")
    parser.add_option_group(group)

  def CreatePageSet(self, options):
    page_set = super(Pixel, self).CreatePageSet(options)
    for page in page_set.pages:
      page.script_to_evaluate_on_commit = test_harness_script
    return page_set
