# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import glob
import os
import re
import sys

from gpu_tests import gpu_integration_test
from gpu_tests import cloud_storage_integration_test_base
from gpu_tests import path_util
from gpu_tests import pixel_expectations
from gpu_tests import pixel_test_pages
from gpu_tests import color_profile_manager

from py_utils import cloud_storage
from telemetry.util import image_util

gpu_relative_path = "content/test/data/gpu/"
gpu_data_dir = os.path.join(path_util.GetChromiumSrcDir(), gpu_relative_path)

default_reference_image_dir = os.path.join(gpu_data_dir, 'gpu_reference')

test_data_dirs = [gpu_data_dir,
                  os.path.join(
                      path_util.GetChromiumSrcDir(), 'media/test/data')]

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


class PixelIntegrationTest(
    cloud_storage_integration_test_base.CloudStorageIntegrationTestBase):
  @classmethod
  def Name(cls):
    """The name by which this test is invoked on the command line."""
    return 'pixel'

  @classmethod
  def SetUpProcess(cls):
    color_profile_manager.ForceUntilExitSRGB()
    super(PixelIntegrationTest, cls).SetUpProcess()
    cls.CustomizeBrowserArgs(cls._AddDefaultArgs([]))
    cls.StartBrowser()
    cls.SetStaticServerDirs(test_data_dirs)

  @staticmethod
  def _AddDefaultArgs(browser_args):
    if not browser_args:
      browser_args = []
    # All tests receive the following options.
    return [
      '--force-color-profile=srgb',
      '--enable-gpu-benchmarking',
      '--test-type=gpu'] + browser_args

  @classmethod
  def StopBrowser(cls):
    super(PixelIntegrationTest, cls).StopBrowser()
    cls.ResetGpuInfo()

  @classmethod
  def AddCommandlineArgs(cls, parser):
    super(PixelIntegrationTest, cls).AddCommandlineArgs(parser)
    parser.add_option(
      '--reference-dir',
      help='Overrides the default on-disk location for reference images '
      '(only used for local testing without a cloud storage account)',
      default=default_reference_image_dir)

  @classmethod
  def _CreateExpectations(cls):
    return pixel_expectations.PixelExpectations()

  @classmethod
  def GenerateGpuTests(cls, options):
    cls.SetParsedCommandLineOptions(options)
    name = 'Pixel'
    pages = pixel_test_pages.DefaultPages(name)
    pages += pixel_test_pages.GpuRasterizationPages(name)
    pages += pixel_test_pages.ExperimentalCanvasFeaturesPages(name)
    if sys.platform.startswith('darwin'):
      pages += pixel_test_pages.MacSpecificPages(name)
    if sys.platform.startswith('win'):
      pages += pixel_test_pages.DirectCompositionPages(name)
    for p in pages:
      yield(p.name, gpu_relative_path + p.url, (p))

  def RunActualGpuTest(self, test_path, *args):
    page = args[0]
    # Some pixel tests require non-standard browser arguments. Need to
    # check before running each page that it can run in the current
    # browser instance.
    self.RestartBrowserIfNecessaryWithArgs(self._AddDefaultArgs(
      page.browser_args))
    url = self.UrlOfStaticFilePath(test_path)
    # This property actually comes off the class, not 'self'.
    tab = self.tab
    tab.Navigate(url, script_to_evaluate_on_commit=test_harness_script)
    tab.action_runner.WaitForJavaScriptCondition(
      'domAutomationController._finished', timeout=300)
    if not tab.EvaluateJavaScript('domAutomationController._succeeded'):
      self.fail('page indicated test failure')
    if not tab.screenshot_supported:
      self.fail('Browser does not support screenshot capture')
    screenshot = tab.Screenshot(5)
    if screenshot is None:
      self.fail('Could not capture screenshot')
    dpr = tab.EvaluateJavaScript('window.devicePixelRatio')
    if page.test_rect:
      screenshot = image_util.Crop(
          screenshot, int(page.test_rect[0] * dpr),
          int(page.test_rect[1] * dpr), int(page.test_rect[2] * dpr),
          int(page.test_rect[3] * dpr))
    if page.expected_colors:
      # Use expected colors instead of ref images for validation.
      self._ValidateScreenshotSamples(
          tab, page.name, screenshot, page.expected_colors, dpr)
      return
    image_name = self._UrlToImageName(page.name)
    if self.GetParsedCommandLineOptions().upload_refimg_to_cloud_storage:
      if self._ConditionallyUploadToCloudStorage(image_name, page, tab,
                                                 screenshot):
        # This is the new reference image; there's nothing to compare against.
        ref_png = screenshot
      else:
        # There was a preexisting reference image, so we might as well
        # compare against it.
        ref_png = self._DownloadFromCloudStorage(image_name, page, tab)
    elif self.GetParsedCommandLineOptions().download_refimg_from_cloud_storage:
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
        self.fail('Could not find image %s in cloud storage' % image_name)
    else:
      # Legacy path using on-disk results.
      ref_png = self._GetReferenceImage(
        self.GetParsedCommandLineOptions().reference_dir,
        image_name, page.revision, screenshot)

    # Test new snapshot against existing reference image
    if not image_util.AreEqual(ref_png, screenshot, tolerance=page.tolerance):
      if self.GetParsedCommandLineOptions().test_machine_name:
        self._UploadErrorImagesToCloudStorage(image_name, screenshot, ref_png)
      else:
        self._WriteErrorImages(
          self.GetParsedCommandLineOptions().generated_dir, image_name,
          screenshot, ref_png)
      self.fail('Reference image did not match captured screen')

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

    image_path = image_path + '_v' + str(cur_revision) + '.png'

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

def load_tests(loader, tests, pattern):
  del loader, tests, pattern  # Unused.
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])
