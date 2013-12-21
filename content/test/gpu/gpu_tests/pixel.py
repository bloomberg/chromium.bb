# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from datetime import datetime
import glob
import optparse
import os
import re
import tempfile

from telemetry import test
from telemetry.core import bitmap
from telemetry.page import cloud_storage
from telemetry.page import page_test

test_data_dir = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..', 'data', 'gpu'))

default_generated_data_dir = os.path.join(test_data_dir, 'generated')
default_reference_image_dir = os.path.join(test_data_dir, 'gpu_reference')

error_image_cloud_storage_bucket = 'chromium-browser-gpu-tests'

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
    # Parameters for cloud storage reference images.
    self.vendor_id = None
    self.device_id = None
    self.vendor_string = None
    self.device_string = None

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
      ref_png = self._DownloadFromCloudStorage(image_name, page, tab)
    else:
      # Legacy path using on-disk results.
      ref_png = PixelValidator.GetReferenceImage(self.options.reference_dir,
          image_name, page.revision, screenshot)

    # Test new snapshot against existing reference image
    if not ref_png.IsEqual(screenshot, tolerance=2):
      if self.options.test_machine_name:
        self._UploadErrorImagesToCloudStorage(image_name, ref_png, screenshot)
      else:
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

  def _ComputeGpuInfo(self, tab):
    if ((self.vendor_id and self.device_id) or
        (self.vendor_string and self.device_string)):
      return
    browser = tab.browser
    if not browser.supports_system_info:
      raise Exception('System info must be supported by the browser')
    system_info = browser.GetSystemInfo()
    if not system_info.gpu:
      raise Exception('GPU information was absent')
    device = system_info.gpu.devices[0]
    if device.vendor_id and device.device_id:
      self.vendor_id = device.vendor_id
      self.device_id = device.device_id
    elif device.vendor_string and device.device_string:
      self.vendor_string = device.vendor_string
      self.device_string = device.device_string
    else:
      raise Exception('GPU device information was incomplete')

  def _FormatGpuInfo(self, tab):
    self._ComputeGpuInfo(tab)
    if self.vendor_id:
      return '%s_%04x_%04x' % (
        self.options.os_type, self.vendor_id, self.device_id)
    else:
      return '%s_%s_%s' % (
        self.options.os_type, self.vendor_string, self.device_string)

  def _FormatReferenceImageName(self, img_name, page, tab):
    return '%s_v%s_%s.png' % (
      img_name,
      page.revision,
      self._FormatGpuInfo(tab))

  def _UploadBitmapToCloudStorage(self, bucket, name, bitmap, public=False):
    # This sequence of steps works on all platforms to write a temporary
    # PNG to disk, following the pattern in bitmap_unittest.py. The key to
    # avoiding PermissionErrors seems to be to not actually try to write to
    # the temporary file object, but to re-open its name for all operations.
    f = tempfile.NamedTemporaryFile()
    bitmap.WritePngFile(f.name)
    cloud_storage.Insert(bucket, name, f.name, publicly_readable=public)
    f.close()

  def _ConditionallyUploadToCloudStorage(self, img_name, page, tab, screenshot):
    """Uploads the screenshot to cloud storage as the reference image
    for this test, unless it already exists. Returns True if the
    upload was actually performed."""
    if not self.options.refimg_cloud_storage_bucket:
      raise Exception('--refimg-cloud-storage-bucket argument is required')
    cloud_name = self._FormatReferenceImageName(img_name, page, tab)
    if not cloud_storage.Exists(self.options.refimg_cloud_storage_bucket,
                                cloud_name):
      self._UploadBitmapToCloudStorage(self.options.refimg_cloud_storage_bucket,
                                       cloud_name,
                                       screenshot)
      return True
    return False

  def _DownloadFromCloudStorage(self, img_name, page, tab):
    """Downloads the reference image for the given test from cloud
    storage, returning it as a Telemetry Bitmap object."""
    # TODO(kbr): there's a race condition between the deletion of the
    # temporary file and gsutil's overwriting it.
    if not self.options.refimg_cloud_storage_bucket:
      raise Exception('--refimg-cloud-storage-bucket argument is required')
    f = tempfile.NamedTemporaryFile()
    filename = f.name
    f.close()
    cloud_storage.Get(self.options.refimg_cloud_storage_bucket,
                      self._FormatReferenceImageName(img_name, page, tab),
                      filename)
    return bitmap.Bitmap.FromPngFile(filename)

  def _UploadErrorImagesToCloudStorage(self, image_name, ref_img, screenshot):
    """For a failing run, uploads the reference image, failing image,
    and diff image to cloud storage. This subsumes the functionality
    of the archive_gpu_pixel_test_results.py script."""
    machine_name = re.sub('\W+', '_', self.options.test_machine_name)
    upload_dir = '%s_%s_telemetry' % (self.options.build_revision, machine_name)
    base_bucket = '%s/runs/%s' % (error_image_cloud_storage_bucket, upload_dir)
    image_name_with_revision = '%s_%s.png' % (
      image_name, self.options.build_revision)
    self._UploadBitmapToCloudStorage(
      base_bucket + '/ref', image_name_with_revision, ref_img, public=True)
    self._UploadBitmapToCloudStorage(
      base_bucket + '/gen', image_name_with_revision, screenshot,
      public=True)
    diff_img = screenshot.Diff(ref_img)
    self._UploadBitmapToCloudStorage(
      base_bucket + '/diff', image_name_with_revision, diff_img,
      public=True)
    print ('See http://%s.commondatastorage.googleapis.com/'
           'view_test_results.html?%s for this run\'s test results') % (
      error_image_cloud_storage_bucket, upload_dir)

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
    group.add_option('--upload-refimg-to-cloud-storage',
        dest='upload_refimg_to_cloud_storage',
        action='store_true', default=False,
        help='Upload resulting images to cloud storage as reference images')
    group.add_option('--download-refimg-from-cloud-storage',
        dest='download_refimg_from_cloud_storage',
        action='store_true', default=False,
        help='Download reference images from cloud storage')
    group.add_option('--refimg-cloud-storage-bucket',
        help='Name of the cloud storage bucket to use for reference images; '
        'required with --upload-refimg-to-cloud-storage and '
        '--download-refimg-from-cloud-storage. Example: '
        '"chromium-gpu-archive/reference-images"')
    group.add_option('--os-type',
        help='Type of operating system on which the pixel test is being run, '
        'used only to distinguish different operating systems with the same '
        'graphics card. Any value is acceptable, but canonical values are '
        '"win", "mac", and "linux", and probably, eventually, "chromeos" '
        'and "android").',
        default='')
    group.add_option('--test-machine-name',
        help='Name of the test machine. Specifying this argument causes this '
        'script to upload failure images and diffs to cloud storage directly, '
        'instead of relying on the archive_gpu_pixel_test_results.py script.',
        default='')
    parser.add_option_group(group)

  def CreatePageSet(self, options):
    page_set = super(Pixel, self).CreatePageSet(options)
    for page in page_set.pages:
      page.script_to_evaluate_on_commit = test_harness_script
    return page_set
