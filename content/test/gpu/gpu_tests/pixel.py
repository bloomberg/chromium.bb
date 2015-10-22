# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import glob
import json
import os
import re

from gpu_tests import cloud_storage_test_base
from gpu_tests import pixel_expectations
import page_sets

from catapult_base import cloud_storage
from telemetry.page import page_test
from telemetry.util import image_util


test_data_dir = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..', 'data', 'gpu'))

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

class PixelValidator(cloud_storage_test_base.ValidatorBase):
  def CustomizeBrowserOptions(self, options):
    # --test-type=gpu is used only to suppress the "Google API Keys are missing"
    # infobar, which causes flakiness in tests.
    options.AppendExtraBrowserArgs(['--enable-gpu-benchmarking',
                                    '--test-type=gpu'])

  def ValidateAndMeasurePage(self, page, tab, results):
    if not _DidTestSucceed(tab):
      raise page_test.Failure('Page indicated a failure')

    if not tab.screenshot_supported:
      raise page_test.Failure('Browser does not support screenshot capture')

    screenshot = tab.Screenshot(5)

    if screenshot is None:
      raise page_test.Failure('Could not capture screenshot')

    dpr = tab.EvaluateJavaScript('window.devicePixelRatio')

    if hasattr(page, 'test_rect'):
      screenshot = image_util.Crop(
          screenshot, page.test_rect[0] * dpr, page.test_rect[1] * dpr,
          page.test_rect[2] * dpr, page.test_rect[3] * dpr)

    if hasattr(page, 'expected_colors'):
      # Use expected pixels instead of ref images for validation.
      expected_colors_file = os.path.abspath(os.path.join(
          os.path.dirname(__file__), page.expected_colors))
      expected_colors = self._ReadPixelExpectations(expected_colors_file)
      self._ValidateScreenshotSamples(
          page.display_name, screenshot, expected_colors, dpr)
      return

    image_name = self._UrlToImageName(page.display_name)

    if self.options.upload_refimg_to_cloud_storage:
      if self._ConditionallyUploadToCloudStorage(image_name, page, tab,
                                                 screenshot):
        # This is the new reference image; there's nothing to compare against.
        ref_png = screenshot
      else:
        # There was a preexisting reference image, so we might as well
        # compare against it.
        ref_png = self._DownloadFromCloudStorage(image_name, page, tab)
    elif self.options.download_refimg_from_cloud_storage:
      # This bot doesn't have the ability to properly generate a
      # reference image, so download it from cloud storage.
      try:
        ref_png = self._DownloadFromCloudStorage(image_name, page, tab)
      except cloud_storage.NotFoundError:
        # There is no reference image yet in cloud storage. This
        # happens when the revision of the test is incremented or when
        # a new test is added, because the trybots are not allowed to
        # produce reference images, only the bots on the main
        # waterfalls. Report this as a failure so the developer has to
        # take action by explicitly suppressing the failure and
        # removing the suppression once the reference images have been
        # generated. Otherwise silent failures could happen for long
        # periods of time.
        raise page_test.Failure('Could not find image %s in cloud storage' %
                                image_name)
    else:
      # Legacy path using on-disk results.
      ref_png = self._GetReferenceImage(self.options.reference_dir,
          image_name, page.revision, screenshot)

    # Test new snapshot against existing reference image
    if not image_util.AreEqual(ref_png, screenshot, tolerance=2):
      if self.options.test_machine_name:
        self._UploadErrorImagesToCloudStorage(image_name, screenshot, ref_png)
      else:
        self._WriteErrorImages(self.options.generated_dir, image_name,
                               screenshot, ref_png)
      raise page_test.Failure('Reference image did not match captured screen')

  def _DeleteOldReferenceImages(self, ref_image_path, cur_revision):
    if not cur_revision:
      return

    old_revisions = glob.glob(ref_image_path + "_*.png")
    for rev_path in old_revisions:
      m = re.match(r'^.*_(\d+)\.png$', rev_path)
      if m and int(m.group(1)) < cur_revision:
        print 'Found deprecated reference image. Deleting rev ' + m.group(1)
        os.remove(rev_path)

  def _GetReferenceImage(self, img_dir, img_name, cur_revision, screenshot):
    if not cur_revision:
      cur_revision = 0

    image_path = os.path.join(img_dir, img_name)

    self._DeleteOldReferenceImages(image_path, cur_revision)

    image_path = image_path + '_' + str(cur_revision) + '.png'

    try:
      ref_png = image_util.FromPngFile(image_path)
    # This can raise a couple of exceptions including IOError and ValueError.
    except Exception:
      ref_png = None

    if ref_png is not None:
      return ref_png

    print ('Reference image not found. Writing tab contents as reference to: ' +
           image_path)

    self._WriteImage(image_path, screenshot)
    return screenshot

  def _ReadPixelExpectations(self, json_file_path):
    with open(json_file_path, 'r') as f:
      json_contents = json.load(f)
    return json_contents

class Pixel(cloud_storage_test_base.TestBase):
  test = PixelValidator

  @classmethod
  def Name(cls):
    return 'pixel'

  @classmethod
  def AddBenchmarkCommandLineArgs(cls, group):
    super(Pixel, cls).AddBenchmarkCommandLineArgs(group)
    group.add_option('--reference-dir',
        help='Overrides the default on-disk location for reference images '
        '(only used for local testing without a cloud storage account)',
        default=default_reference_image_dir)

  def CreateStorySet(self, options):
    story_set = page_sets.PixelTestsStorySet(self.GetExpectations(),
                                             try_es3=True)
    for page in story_set:
      page.script_to_evaluate_on_commit = test_harness_script
    return story_set

  def _CreateExpectations(self):
    return pixel_expectations.PixelExpectations()
