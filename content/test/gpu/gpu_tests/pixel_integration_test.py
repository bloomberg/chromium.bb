# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import glob
import logging
import os
import re
import sys

from gpu_tests import cloud_storage_integration_test_base
from gpu_tests import pixel_expectations
from gpu_tests import pixel_test_pages

from py_utils import cloud_storage
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


class PixelIntegrationTest(
    cloud_storage_integration_test_base.CloudStorageIntegrationTestBase):

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
    return 'pixel'

  @classmethod
  def setUpClass(cls):
    super(cls, PixelIntegrationTest).setUpClass()
    cls._original_finder_options = cls._finder_options.Copy()
    cls.CustomizeBrowserArgs([])
    cls.StartBrowser()
    cls.SetStaticServerDirs([test_data_dir])

  @classmethod
  def CustomizeBrowserArgs(cls, browser_args):
    if not browser_args:
      browser_args = []
    cls._finder_options = cls._original_finder_options.Copy()
    browser_options = cls._finder_options.browser_options
    # All tests receive these options. They aren't recorded in the
    # _last_launched_browser_args.
    browser_options.AppendExtraBrowserArgs(['--enable-gpu-benchmarking',
                                            '--test-type=gpu'])
    # Append the new arguments.
    browser_options.AppendExtraBrowserArgs(browser_args)
    cls._last_launched_browser_args = set(browser_args)
    cls.SetBrowserOptions(cls._finder_options)

  @classmethod
  def RestartBrowserIfNecessaryWithArgs(cls, browser_args):
    if not browser_args:
      browser_args = []
    if set(browser_args) != cls._last_launched_browser_args:
      logging.warning('Restarting browser with arguments: ' + str(browser_args))
      cls.StopBrowser()
      cls.ResetGpuInfo()
      cls.CustomizeBrowserArgs(browser_args)
      cls.StartBrowser()
      cls.tab = cls.browser.tabs[0]

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
    pages += pixel_test_pages.ExperimentalCanvasFeaturesPages(name)
    if sys.platform.startswith('darwin'):
      pages += pixel_test_pages.MacSpecificPages(name)
    for p in pages:
      yield(p.name, p.url, (p))

  def RunActualGpuTest(self, test_path, *args):
    page = args[0]
    # Some pixel tests require non-standard browser arguments. Need to
    # check before running each page that it can run in the current
    # browser instance.
    self.RestartBrowserIfNecessaryWithArgs(page.browser_args)
    url = self.UrlOfStaticFilePath(test_path)
    # This property actually comes off the class, not 'self'.
    tab = self.tab
    tab.Navigate(url, script_to_evaluate_on_commit=test_harness_script)
    tab.action_runner.WaitForJavaScriptCondition(
      'domAutomationController._finished', timeout_in_seconds=300)
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
          screenshot, page.test_rect[0] * dpr, page.test_rect[1] * dpr,
          page.test_rect[2] * dpr, page.test_rect[3] * dpr)
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
