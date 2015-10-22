# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Base classes for a test and validator which upload results
(reference images, error images) to cloud storage."""

import os
import re
import tempfile

from catapult_base import cloud_storage
from telemetry.page import page_test
from telemetry.util import image_util
from telemetry.util import rgba_color

from gpu_tests import gpu_test_base

test_data_dir = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..', 'data', 'gpu'))

default_generated_data_dir = os.path.join(test_data_dir, 'generated')

error_image_cloud_storage_bucket = 'chromium-browser-gpu-tests'

def _CompareScreenshotSamples(screenshot, expectations, device_pixel_ratio):
  for expectation in expectations:
    location = expectation["location"]
    size = expectation["size"]
    x0 = int(location[0] * device_pixel_ratio)
    x1 = int((location[0] + size[0]) * device_pixel_ratio)
    y0 = int(location[1] * device_pixel_ratio)
    y1 = int((location[1] + size[1]) * device_pixel_ratio)
    for x in range(x0, x1):
      for y in range(y0, y1):
        if (x < 0 or y < 0 or x >= image_util.Width(screenshot) or
            y >= image_util.Height(screenshot)):
          raise page_test.Failure(
              ('Expected pixel location [%d, %d] is out of range on ' +
               '[%d, %d] image') %
              (x, y, image_util.Width(screenshot),
               image_util.Height(screenshot)))

        actual_color = image_util.GetPixelColor(screenshot, x, y)
        expected_color = rgba_color.RgbaColor(
            expectation["color"][0],
            expectation["color"][1],
            expectation["color"][2])
        if not actual_color.IsEqual(expected_color, expectation["tolerance"]):
          raise page_test.Failure('Expected pixel at ' + str(location) +
              ' to be ' +
              str(expectation["color"]) + " but got [" +
              str(actual_color.r) + ", " +
              str(actual_color.g) + ", " +
              str(actual_color.b) + "]")

class ValidatorBase(gpu_test_base.ValidatorBase):
  def __init__(self):
    super(ValidatorBase, self).__init__()
    # Parameters for cloud storage reference images.
    self.vendor_id = None
    self.device_id = None
    self.vendor_string = None
    self.device_string = None
    self.msaa = False

  ###
  ### Routines working with the local disk (only used for local
  ### testing without a cloud storage account -- the bots do not use
  ### this code path).
  ###

  def _UrlToImageName(self, url):
    image_name = re.sub(r'^(http|https|file)://(/*)', '', url)
    image_name = re.sub(r'\.\./', '', image_name)
    image_name = re.sub(r'(\.|/|-)', '_', image_name)
    return image_name

  def _WriteImage(self, image_path, png_image):
    output_dir = os.path.dirname(image_path)
    if not os.path.exists(output_dir):
      os.makedirs(output_dir)
    image_util.WritePngFile(png_image, image_path)

  def _WriteErrorImages(self, img_dir, img_name, screenshot, ref_png):
    full_image_name = img_name + '_' + str(self.options.build_revision)
    full_image_name = full_image_name + '.png'

    # Always write the failing image.
    self._WriteImage(
        os.path.join(img_dir, 'FAIL_' + full_image_name), screenshot)

    if ref_png is not None:
      # Save the reference image.
      # This ensures that we get the right revision number.
      self._WriteImage(
          os.path.join(img_dir, full_image_name), ref_png)

      # Save the difference image.
      diff_png = image_util.Diff(screenshot, ref_png)
      self._WriteImage(
          os.path.join(img_dir, 'DIFF_' + full_image_name), diff_png)

  ###
  ### Cloud storage code path -- the bots use this.
  ###

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
    # TODO(senorblanco): This should probably be checking
    # for the presence of the extensions in system_info.gpu_aux_attributes
    # in order to check for MSAA, rather than sniffing the blacklist.
    self.msaa = not (
        ('disable_chromium_framebuffer_multisample' in
          system_info.gpu.driver_bug_workarounds) or
        ('disable_multisample_render_to_texture' in
          system_info.gpu.driver_bug_workarounds))

  def _FormatGpuInfo(self, tab):
    self._ComputeGpuInfo(tab)
    msaa_string = '_msaa' if self.msaa else '_non_msaa'
    if self.vendor_id:
      return '%s_%04x_%04x%s' % (
        self.options.os_type, self.vendor_id, self.device_id, msaa_string)
    else:
      return '%s_%s_%s%s' % (
        self.options.os_type, self.vendor_string, self.device_string,
        msaa_string)

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
    temp_file = tempfile.NamedTemporaryFile(suffix='.png').name
    image_util.WritePngFile(bitmap, temp_file)
    cloud_storage.Insert(bucket, name, temp_file, publicly_readable=public)

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
    temp_file = tempfile.NamedTemporaryFile(suffix='.png').name
    cloud_storage.Get(self.options.refimg_cloud_storage_bucket,
                      self._FormatReferenceImageName(img_name, page, tab),
                      temp_file)
    return image_util.FromPngFile(temp_file)

  def _UploadErrorImagesToCloudStorage(self, image_name, screenshot, ref_img):
    """For a failing run, uploads the failing image, reference image (if
    supplied), and diff image (if reference image was supplied) to cloud
    storage. This subsumes the functionality of the
    archive_gpu_pixel_test_results.py script."""
    machine_name = re.sub(r'\W+', '_', self.options.test_machine_name)
    upload_dir = '%s_%s_telemetry' % (self.options.build_revision, machine_name)
    base_bucket = '%s/runs/%s' % (error_image_cloud_storage_bucket, upload_dir)
    image_name_with_revision = '%s_%s.png' % (
      image_name, self.options.build_revision)
    self._UploadBitmapToCloudStorage(
      base_bucket + '/gen', image_name_with_revision, screenshot,
      public=True)
    if ref_img is not None:
      self._UploadBitmapToCloudStorage(
        base_bucket + '/ref', image_name_with_revision, ref_img, public=True)
      diff_img = image_util.Diff(screenshot, ref_img)
      self._UploadBitmapToCloudStorage(
        base_bucket + '/diff', image_name_with_revision, diff_img,
        public=True)
    print ('See http://%s.commondatastorage.googleapis.com/'
           'view_test_results.html?%s for this run\'s test results') % (
      error_image_cloud_storage_bucket, upload_dir)

  def _ValidateScreenshotSamples(self, url,
                                 screenshot, expectations, device_pixel_ratio):
    """Samples the given screenshot and verifies pixel color values.
       The sample locations and expected color values are given in expectations.
       In case any of the samples do not match the expected color, it raises
       a Failure and dumps the screenshot locally or cloud storage depending on
       what machine the test is being run."""
    try:
      _CompareScreenshotSamples(screenshot, expectations, device_pixel_ratio)
    except page_test.Failure:
      image_name = self._UrlToImageName(url)
      if self.options.test_machine_name:
        self._UploadErrorImagesToCloudStorage(image_name, screenshot, None)
      else:
        self._WriteErrorImages(self.options.generated_dir, image_name,
                               screenshot, None)
      raise


class TestBase(gpu_test_base.TestBase):
  @classmethod
  def AddBenchmarkCommandLineArgs(cls, group):
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
    group.add_option('--generated-dir',
        help='Overrides the default on-disk location for generated test images '
        '(only used for local testing without a cloud storage account)',
        default=default_generated_data_dir)
