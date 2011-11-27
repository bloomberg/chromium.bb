#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import random
import unittest

from media_test_matrix import MediaTestMatrix


class MediaTestMatrixTest(unittest.TestCase):
  """Unit test for MediaTestMatrix class."""

  def _ReadCSVFile(self):
    """Reads CSV file for test and returns a media test matrix object.

    Returns:
      a media test matrix.
    """
    matrix = MediaTestMatrix()
    # Pyauto.DataDir() method is not used to avoid dependency to pyauto.
    data_file = os.path.join(os.pardir, os.pardir, 'data', 'media', 'csv',
                             'media_matrix_data.csv')
    matrix.ReadData(data_file)
    return matrix

  def testGenerateAllMediaInfosInCompactForm(self):
    matrix = self._ReadCSVFile()
    # Get video data only case.
    media_test_matrix_list = matrix.GenerateAllMediaInfosInCompactForm(True)
    self.assertEqual(len(media_test_matrix_list), 240,
                     'total number of elements in the returned list is wrong')
    # Get all media data.
    media_test_matrix_list = matrix.GenerateAllMediaInfosInCompactForm(False)
    self.assertEqual(len(media_test_matrix_list), 449,
                     'total number of elements in the returned list is wrong')
    mobile_test_info = MediaTestMatrix.LookForMediaInfoInCompactFormByNickName(
        media_test_matrix_list, 'mobile0.webm')
    self.assertEqual(mobile_test_info[2], 'mobile0.webm',
                     'the nickname of the returned element is wrong')

  def testGenerateAllMediaInfos(self):
    matrix = self._ReadCSVFile()
    # Get video data only case.
    media_test_matrix_list = matrix.GenerateAllMediaInfos(True)
    self.assertEqual(len(media_test_matrix_list), 240,
                     'total number of elements in the returned list is wrong')
    # Get all media data.
    media_test_matrix_list = matrix.GenerateAllMediaInfos(False)
    self.assertEqual(len(media_test_matrix_list), 449,
                     'total number of elements in the returned list is wrong')
    (info, url, link, tag, is_video, nickname) = (
        matrix.GenerateRandomMediaInfo())
    self.assertTrue(info is not None,
                    'randomly generated media info does not have spec')
    self.assertTrue(url is not None,
                    'randomly generated media info does not have url')
    self.assertTrue(link is not None,
                    'randomly generated media info does not have link')
    self.assertTrue(tag is not None,
                    'randomly generated media info does not have tag')
    self.assertTrue(nickname is not None,
                    'randomly generated media info does not have nickname')


if __name__ == '__main__':
    unittest.main()
