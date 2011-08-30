#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import os
import pickle
import unittest

import layouttest_analyzer_helpers


class TestLayoutTestAnalyzerHelpers(unittest.TestCase):

  def testFindLatestTime(self):
    time_list = ['2011-08-18-19', '2011-08-18-22', '2011-08-18-21',
                 '2012-01-11-21']
    self.assertEquals(layouttest_analyzer_helpers.FindLatestTime(time_list),
                      '2012-01-11-21')

  def GenerateTestDataWholeAndSkip(self):
    """You should call this method if you want to generate test data."""
    file_path = os.path.join('test_data', 'base')
    analyzerResultMapBase = (
        layouttest_analyzer_helpers.AnalyzerResultMap.Load(file_path))
    # Remove this first part
    m = analyzerResultMapBase.result_map['whole']
    del m['media/video-source-type.html']
    m = analyzerResultMapBase.result_map['skip']
    del m['media/track/track-webvtt-tc004-magicheader.html']

    file_path = os.path.join('test_data', 'less')
    analyzerResultMapBase.Save(file_path)

    file_path = os.path.join('test_data', 'base')
    analyzerResultMapBase = AnalyzerResultMap.Load(file_path)

    analyzerResultMapBase.result_map['whole']['add1.html'] = True
    analyzerResultMapBase.result_map['skip']['add2.html'] = True

    file_path = os.path.join('test_data', 'more')
    analyzerResultMapBase.Save(file_path)

  def GenerateTestDataNonSkip(self):
    """You should call this method if you want to generate test data."""
    file_path = os.path.join('test_data', 'base')
    analyzerResultMapBase = AnalyzerResultMap.Load(file_path)
    m = analyzerResultMapBase.result_map['nonskip']
    ex = m['media/media-document-audio-repaint.html']
    te_info_map1 = ex['te_info'][0]
    te_info_map2 = copy.copy(te_info_map1)
    te_info_map2['NEWADDED'] = True
    ex['te_info'].append(te_info_map2)
    m = analyzerResultMapBase.result_map['nonskip']

    file_path = os.path.join('test_data', 'more_te_info')
    analyzerResultMapBase.Save(file_path)

  def testCompareResultMapsWholeAndSkip(self):
    file_path = os.path.join('test_data', 'base')
    analyzerResultMapBase = (
        layouttest_analyzer_helpers.AnalyzerResultMap.Load(file_path))

    file_path = os.path.join('test_data', 'less')
    analyzerResultMapLess = (
        layouttest_analyzer_helpers.AnalyzerResultMap.Load(file_path))

    diff = analyzerResultMapBase.CompareToOtherResultMap(analyzerResultMapLess)
    self.assertEquals(diff['skip'][0][0][0],
                      'media/track/track-webvtt-tc004-magicheader.html')
    self.assertEquals(diff['whole'][0][0][0],
                      'media/video-source-type.html')
    file_path = os.path.join('test_data', 'more')
    analyzerResultMapMore = (
        layouttest_analyzer_helpers.AnalyzerResultMap.Load(file_path))
    diff = analyzerResultMapBase.CompareToOtherResultMap(analyzerResultMapMore)
    self.assertEquals(diff['whole'][1][0][0], 'add1.html')
    self.assertEquals(diff['skip'][1][0][0], 'add2.html')

  def testCompareResultMapsNonSkip(self):
    file_path = os.path.join('test_data', 'base')
    analyzerResultMapBase = (
        layouttest_analyzer_helpers.AnalyzerResultMap.Load(file_path))
    file_path = os.path.join('test_data', 'more_te_info')
    analyzerResultMapMoreTEInfo = (
        layouttest_analyzer_helpers.AnalyzerResultMap.Load(file_path))
    m = analyzerResultMapBase.CompareToOtherResultMap(
        analyzerResultMapMoreTEInfo)
    self.assertTrue('NEWADDED' in m['nonskip'][1][0][1][0])

  def testGetListOfBugsForNonSkippedTests(self):
    file_path = os.path.join('test_data', 'base')
    analyzerResultMapBase = (
        layouttest_analyzer_helpers.AnalyzerResultMap.Load(file_path))
    self.assertEquals(
        len(analyzerResultMapBase.GetListOfBugsForNonSkippedTests().keys()),
        10)


if __name__ == '__main__':
  unittest.main()
