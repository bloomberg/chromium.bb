# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilities for managing I-Spy test results in Google Cloud Storage."""

import collections
import itertools
import json
import os
import sys

import image_tools


def GetTestPath(test_run, test_name, file_name=''):
  """Get the path to a test file in the given test run and test.

  Args:
    test_run: name of the test run.
    test_name: name of the test.
    file_name: name of the file.

  Returns:
    the path as a string relative to the bucket.
  """
  return 'tests/%s/%s/%s' % (test_run, test_name, file_name)


def GetFailurePath(test_run, test_name, file_name=''):
  """Get the path to a failure file in the given test run and test.

  Args:
    test_run: name of the test run.
    test_name: name of the test.
    file_name: name of the file.

  Returns:
    the path as a string relative to the bucket.
  """
  return 'failures/%s/%s/%s' % (test_run, test_name, file_name)


class ISpyUtils(object):
  """Utility functions for working with an I-Spy google storage bucket."""

  def __init__(self, cloud_bucket):
    """Initialize with a cloud bucket instance to supply GS functionality.

    Args:
      cloud_bucket: An object implementing the cloud_bucket.BaseCloudBucket
        interface.
    """
    self.cloud_bucket = cloud_bucket

  def UploadImage(self, full_path, image):
    """Uploads an image to a location in GS.

    Args:
      full_path: the path to the file in GS including the file extension.
      image: a RGB PIL.Image to be uploaded.
    """
    self.cloud_bucket.UploadFile(
        full_path, image_tools.EncodePNG(image), 'image/png')

  def DownloadImage(self, full_path):
    """Downloads an image from a location in GS.

    Args:
      full_path: the path to the file in GS including the file extension.

    Returns:
      The downloaded RGB PIL.Image.

    Raises:
      cloud_bucket.NotFoundError: if the path to the image is not valid.
    """
    return image_tools.DecodePNG(self.cloud_bucket.DownloadFile(full_path))

  def UpdateImage(self, full_path, image):
    """Updates an existing image in GS, preserving permissions and metadata.

    Args:
      full_path: the path to the file in GS including the file extension.
      image: a RGB PIL.Image.
    """
    self.cloud_bucket.UpdateFile(full_path, image_tools.EncodePNG(image))


  def UploadTest(self, test_run, test_name, images):
    """Creates and uploads a test to GS from a set of images and name.

    This method generates a mask from the uploaded images, then
      uploads the mask and first of the images to GS as a test.

    Args:
      test_run: the name of the test_run.
      test_name: the name of the test.
      images: a list of RGB encoded PIL.Images
    """
    mask = image_tools.InflateMask(image_tools.CreateMask(images), 7)
    self.UploadImage(
        GetTestPath(test_run, test_name, 'expected.png'), images[0])
    self.UploadImage(GetTestPath(test_run, test_name, 'mask.png'), mask)

  def RunTest(self, test_run, test_name, actual):
    """Runs an image comparison, and uploads discrepancies to GS.

    Args:
      test_run: the name of the test_run.
      test_name: the name of the test to run.
      actual: an RGB-encoded PIL.Image that is the actual result of the
        test.

    Raises:
      cloud_bucket.NotFoundError: if the given test_name is not found.
    """
    test = self.GetTest(test_run, test_name)
    if not image_tools.SameImage(actual, test.expected, mask=test.mask):
      self.UploadImage(
          GetFailurePath(test_run, test_name, 'actual.png'), actual)
      diff, diff_pxls = image_tools.VisualizeImageDifferences(
          test.expected, actual, mask=test.mask)
      self.UploadImage(GetFailurePath(test_run, test_name, 'diff.png'), diff)
      self.cloud_bucket.UploadFile(
          GetFailurePath(test_run, test_name, 'info.txt'),
          json.dumps({
            'different_pixels': diff_pxls,
            'fraction_different':
                diff_pxls / float(actual.size[0] * actual.size[1])}),
          'application/json')

  def GetTest(self, test_run, test_name):
    """Returns given test from GS.

    Args:
      test_run: the name of the test_run.
      test_name: the name of the test to get from GS.

    Returns:
      A named tuple: 'Test', containing two images: expected and mask.

    Raises:
      cloud_bucket.NotFoundError: if the test is not found in GS.
    """
    Test = collections.namedtuple('Test', ['expected', 'mask'])
    return Test(self.DownloadImage(GetTestPath(test_run, test_name,
                                               'expected.png')),
                self.DownloadImage(GetTestPath(test_run, test_name,
                                               'mask.png')))

  def TestExists(self, test_run, test_name):
    """Returns whether the given test exists in GS.

    Args:
      test_run: the name of the test_run.
      test_name: the name of the test to look for.

    Returns:
      A boolean indicating whether the test exists.
    """
    expected_image_exists = self.cloud_bucket.FileExists(
        GetTestPath(test_run, test_name, 'expected.png'))
    mask_image_exists = self.cloud_bucket.FileExists(
        GetTestPath(test_run, test_name, 'mask.png'))
    return expected_image_exists and mask_image_exists

  def FailureExists(self, test_run, test_name):
    """Returns whether the given run exists in GS.

    Args:
      test_run: the name of the test_run.
      test_name: the name of the test that failed.

    Returns:
      A boolean indicating whether the failure exists.
    """
    actual_image_exists = self.cloud_bucket.FileExists(
        GetFailurePath(test_run, test_name, 'actual.png'))
    test_exists = self.TestExists(test_run, test_name)
    info_exists = self.cloud_bucket.FileExists(
        GetFailurePath(test_run, test_name, 'info.txt'))
    return test_exists and actual_image_exists and info_exists

  def RemoveTest(self, test_run, test_name):
    """Removes a Test from GS, and all associated failures with that test.

    Args:
      test_run: the name of the test_run.
      test_name: the name of the test to remove.
    """
    test_paths = self.cloud_bucket.GetAllPaths(
        GetTestPath(test_run, test_name))
    failure_paths = self.cloud_bucket.GetAllPaths(
        GetFailurePath(test_run, test_name))
    for path in itertools.chain(failure_paths, test_paths):
      self.cloud_bucket.RemoveFile(path)

  def UploadTestPinkOut(self, test_run, test_name, images, pint_out, rgb):
    """Uploads an ispy-test to GS with the pink_out workaround.

    Args:
      test_name: The name of the test to be uploaded.
      images: a json encoded list of base64 encoded png images.
      pink_out: an image.
      RGB: a json list representing the RGB values of a color to mask out.
    """
    # convert the pink_out into a mask
    black = (0, 0, 0, 255)
    white = (255, 255, 255, 255)
    pink_out.putdata(
        [black if px == (rgb[0], rgb[1], rgb[2], 255) else white
         for px in pink_out.getdata()])
    mask = image_tools.CreateMask(images)
    mask = image_tools.InflateMask(image_tools.CreateMask(images), 7)
    combined_mask = image_tools.AddMasks([mask, pink_out])
    self.UploadImage(GetTestPath(test_run, test_name, 'expected.png'),
                     images[0])
    self.UploadImage(GetTestPath(test_run, test_name, 'mask.png'),
                     combined_mask)

  def RemoveFailure(self, test_run, test_name):
    """Removes a failure from GS.

    Args:
      test_run: the name of the test_run.
      test_name: the test on which the failure to be removed occured.
    """
    failure_paths = self.cloud_bucket.GetAllPaths(
        GetFailurePath(test_run, test_name))
    for path in failure_paths:
      self.cloud_bucket.RemoveFile(path)

  def GetFailure(self, test_run, test_name):
    """Returns a given test failure's expected, diff, and actual images.

    Args:
      test_run: the name of the test_run.
      test_name: the name of the test the result corresponds to.

    Returns:
      A named tuple: Failure containing three images: expected, diff, and
        actual.

    Raises:
      cloud_bucket.NotFoundError: if the result is not found in GS.
    """
    test_path = GetTestPath(test_run, test_name)
    failure_path = GetFailurePath(test_run, test_name)
    expected = self.DownloadImage(
        GetTestPath(test_run, test_name, 'expected.png'))
    actual = self.DownloadImage(
        GetFailurePath(test_run, test_name, 'actual.png'))
    diff = self.DownloadImage(
        GetFailurePath(test_run, test_name, 'diff.png'))
    info = json.loads(self.cloud_bucket.DownloadFile(
        GetFailurePath(test_run, test_name, 'info.txt')))
    Failure = collections.namedtuple(
        'Failure', ['expected', 'diff', 'actual', 'info'])
    return Failure(expected, diff, actual, info)

  def GetAllPaths(self, prefix):
    """Gets urls to all files in GS whose path starts with a given prefix.

    Args:
      prefix: the prefix to filter files in GS by.

    Returns:
      a list containing urls to all objects that started with
         the prefix.
    """
    return self.cloud_bucket.GetAllPaths(prefix)

