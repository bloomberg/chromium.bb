#!/usr/bin/env python
#
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import unittest
from PIL import Image

import chrome_utils
from ..common import cloud_bucket
from ..common import mock_cloud_bucket


class ChromeUtilsTest(unittest.TestCase):
  """Unittest for ChromeUtils."""

  def setUp(self):
    self.cloud_bucket = mock_cloud_bucket.MockCloudBucket()
    self.chrome_util = chrome_utils.ChromeUtils(self.cloud_bucket)
    self.white_img = Image.new('RGBA', (10, 10), (255, 255, 255, 255))
    self.black_img = Image.new('RGBA', (10, 10), (0, 0, 0, 255))

  def testGenerateExpectationsRunComparison(self):
    self.chrome_util.GenerateExpectation(
        'device', 'test', '1.1.1.1', 'versions.json',
        [self.white_img, self.white_img])
    self.chrome_util.UpdateExpectationVersion('1.1.1.1', 'versions.json')
    self.chrome_util.PerformComparison(
        'test1', 'device', 'test', '1.1.1.1', 'versions.json', self.white_img)
    expect_name = self.chrome_util._CreateExpectationName(
        'device', 'test', '1.1.1.1')
    self.assertFalse(self.chrome_util._ispy.FailureExists('test1', expect_name))
    self.chrome_util.PerformComparison(
        'test2', 'device', 'test', '1.1.1.1','versions.json', self.black_img)
    self.assertTrue(self.chrome_util._ispy.FailureExists('test2', expect_name))

  def testUpdateExpectationVersion(self):
    self.chrome_util.UpdateExpectationVersion('1.0.0.0', 'versions.json')
    self.chrome_util.UpdateExpectationVersion('1.0.4.0', 'versions.json')
    self.chrome_util.UpdateExpectationVersion('2.1.5.0', 'versions.json')
    self.chrome_util.UpdateExpectationVersion('1.1.5.0', 'versions.json')
    self.chrome_util.UpdateExpectationVersion('0.0.0.0', 'versions.json')
    self.chrome_util.UpdateExpectationVersion('1.1.5.0', 'versions.json')
    self.chrome_util.UpdateExpectationVersion('0.0.0.1', 'versions.json')
    versions = json.loads(self.cloud_bucket.DownloadFile('versions.json'))
    self.assertEqual(versions,
        ['2.1.5.0', '1.1.5.0', '1.0.4.0', '1.0.0.0', '0.0.0.1', '0.0.0.0'])

  def testPerformComparisonAndPrepareExpectation(self):
    self.assertFalse(self.chrome_util.CanRebaselineToTestRun('test'))
    self.assertRaises(
        cloud_bucket.FileNotFoundError,
        self.chrome_util.PerformComparisonAndPrepareExpectation,
        'test', 'device', 'expect', '1.0', 'versions.json',
        [self.white_img, self.white_img])
    self.assertTrue(self.chrome_util.CanRebaselineToTestRun('test'))
    self.chrome_util.RebaselineToTestRun('test')
    versions = json.loads(self.cloud_bucket.DownloadFile('versions.json'))
    self.assertEqual(versions, ['1.0'])
    self.chrome_util.PerformComparisonAndPrepareExpectation(
        'test1', 'device', 'expect', '1.1', 'versions.json',
        [self.white_img, self.white_img])



if __name__ == '__main__':
  unittest.main()
