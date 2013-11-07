#!/usr/bin/env python
#
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import unittest
from PIL import Image

import chrome_utils
from ..common import mock_cloud_bucket


class ChromeUtilsTest(unittest.TestCase):
  """Unittest for ChromeUtils."""

  def setUp(self):
    self.cloud_bucket = mock_cloud_bucket.MockCloudBucket()
    self.white_utils = chrome_utils.ChromeUtils(
        self.cloud_bucket, 'versions.json',
        lambda: Image.new('RGBA', (10, 10), (255, 255, 255, 255)))
    self.black_utils = chrome_utils.ChromeUtils(
        self.cloud_bucket, 'versions.json',
        lambda: Image.new('RGBA', (10, 10), (0, 0, 0, 255)))

  def testGenerateExpectationsRunComparison(self):
    self.white_utils.GenerateExpectation('device', 'test', '1.1.1.1')
    self.white_utils.UpdateExpectationVersion('1.1.1.1')
    self.white_utils.PerformComparison('test1', 'device', 'test', '1.1.1.1')
    expect_name = self.white_utils._CreateExpectationName(
        'device', 'test', '1.1.1.1')
    self.assertFalse(self.white_utils._ispy.FailureExists('test1', expect_name))
    self.black_utils.PerformComparison('test2', 'device', 'test', '1.1.1.1')
    self.assertTrue(self.white_utils._ispy.FailureExists('test2', expect_name))

  def testUpdateExpectationVersion(self):
    self.white_utils.UpdateExpectationVersion('1.0.0.0')
    self.white_utils.UpdateExpectationVersion('1.0.4.0')
    self.white_utils.UpdateExpectationVersion('2.1.5.0')
    self.white_utils.UpdateExpectationVersion('1.1.5.0')
    self.white_utils.UpdateExpectationVersion('0.0.0.0')
    self.white_utils.UpdateExpectationVersion('1.1.5.0')
    self.white_utils.UpdateExpectationVersion('0.0.0.1')
    versions = json.loads(self.cloud_bucket.DownloadFile('versions.json'))
    self.assertEqual(versions,
        ['2.1.5.0', '1.1.5.0', '1.0.4.0', '1.0.0.0', '0.0.0.1', '0.0.0.0'])


if __name__ == '__main__':
  unittest.main()
