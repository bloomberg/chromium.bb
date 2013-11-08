# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import os
import time
from distutils.version import LooseVersion
from PIL import Image

from ..common import cloud_bucket
from ..common import ispy_utils


class ChromeUtils(object):
  """A utility for using ISpy with Chrome."""

  def __init__(self, cloud_bucket, version_file, screenshot_func):
    """Initializes the utility class.

    Args:
      cloud_bucket: a BaseCloudBucket in which to the version file,
          expectations and results are to be stored.
      version_file: path to the version file in the cloud bucket. The version
          file contains a json list of ordered Chrome versions for which
          expectations exist.
      screenshot_func: a function that returns a PIL.Image.
    """
    self._cloud_bucket = cloud_bucket
    self._version_file = version_file
    self._screenshot_func = screenshot_func
    self._ispy = ispy_utils.ISpyUtils(self._cloud_bucket)
    with open(
        os.path.join(os.path.dirname(__file__), 'wait_on_ajax.js'), 'r') as f:
      self._wait_for_unchanging_dom_script = f.read()

  def UpdateExpectationVersion(self, chrome_version):
    """Updates the most recent expectation version to the Chrome version.

    Should be called after generating a new set of expectations.

    Args:
      chrome_version: the chrome version as a string of the form "31.0.123.4".
    """
    insert_pos = 0
    expectation_versions = []
    try:
      expectation_versions = self._GetExpectationVersionList()
      if expectation_versions:
        try:
          version = self._GetExpectationVersion(
              chrome_version, expectation_versions)
          if version == chrome_version:
            return
          insert_pos = expectation_versions.index(version)
        except:
          insert_pos = len(expectation_versions)
    except cloud_bucket.FileNotFoundError:
      pass
    expectation_versions.insert(insert_pos, chrome_version)
    logging.info('Updating expectation version...')
    self._cloud_bucket.UploadFile(
        self._version_file, json.dumps(expectation_versions),
        'application/json')

  def _GetExpectationVersion(self, chrome_version, expectation_versions):
    """Returns the expectation version for the given Chrome version.

    Args:
      chrome_version: the chrome version as a string of the form "31.0.123.4".
      expectation_versions: Ordered list of Chrome versions for which
        expectations exist, as stored in the version file.

    Returns:
      Expectation version string.
    """
    # Find the closest version that is not greater than the chrome version.
    for version in expectation_versions:
      if LooseVersion(version) <= LooseVersion(chrome_version):
        return version
    raise Exception('No expectation exists for Chrome %s' % chrome_version)

  def _GetExpectationVersionList(self):
    """Gets the list of expectation versions from google storage."""
    return json.loads(self._cloud_bucket.DownloadFile(self._version_file))

  def _GetExpectationNameWithVersion(self, device_type, expectation,
                                     chrome_version):
    """Get the expectation to be used with the current Chrome version.

    Args:
      device_type: string identifier for the device type.
      expectation: name for the expectation to generate.
      chrome_version: the chrome version as a string of the form "31.0.123.4".

    Returns:
      Version as an integer.
    """
    version = self._GetExpectationVersion(
        chrome_version, self._GetExpectationVersionList())
    return self._CreateExpectationName(device_type, expectation, version)

  def _CreateExpectationName(self, device_type, expectation, version):
    """Create the full expectation name from the expectation and version.

    Args:
      device_type: string identifier for the device type, example: mako
      expectation: base name for the expectation, example: google.com
      version: expectation version, example: 31.0.23.1

    Returns:
      Full expectation name as a string, example: mako:google.com(31.0.23.1)
    """
    return '%s:%s(%s)' % (device_type, expectation, version)

  def GenerateExpectation(self, device_type, expectation, chrome_version):
    """Take screenshots and store as an expectation in I-Spy.

    Args:
      device_type: string identifier for the device type.
      expectation: name for the expectation to generate.
      chrome_version: the chrome version as a string of the form "31.0.123.4".
    """
    # https://code.google.com/p/chromedriver/issues/detail?id=463
    time.sleep(1)
    expectation_with_version = self._CreateExpectationName(
        device_type, expectation, chrome_version)
    if self._ispy.ExpectationExists(expectation_with_version):
      logging.warning(
          'I-Spy expectation \'%s\' already exists, overwriting.',
          expectation_with_version)
    screenshots = [self._screenshot_func() for _ in range(8)]
    logging.info('Generating I-Spy expectation...')
    self._ispy.GenerateExpectation(expectation_with_version, screenshots)

  def PerformComparison(self, test_run, device_type, expectation,
                        chrome_version):
    """Take a screenshot and compare it with the given expectation in I-Spy.

    Args:
      test_run: name for the test run.
      device_type: string identifier for the device type.
      expectation: name for the expectation to compare against.
      chrome_version: the chrome version as a string of the form "31.0.123.4".
    """
    # https://code.google.com/p/chromedriver/issues/detail?id=463
    time.sleep(1)
    screenshot = self._screenshot_func()
    logging.info('Performing I-Spy comparison...')
    self._ispy.PerformComparison(
        test_run,
        self._GetExpectationNameWithVersion(
            device_type, expectation, chrome_version),
        screenshot)

  def GetScriptToWaitForUnchangingDOM(self):
    """Returns a JavaScript script that waits for the DOM to stop changing."""
    return self._wait_for_unchanging_dom_script

