#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
from datetime import datetime
import os
import pickle
import time
import unittest


import layouttest_analyzer_helpers


class TestLayoutTestAnalyzerHelpers(unittest.TestCase):

  def testFindLatestTime(self):
    time_list = ['2011-08-18-19', '2011-08-18-22', '2011-08-18-21',
                 '2012-01-11-21', '.foo']
    self.assertEquals(layouttest_analyzer_helpers.FindLatestTime(time_list),
                      '2012-01-11-21')

  def testFindLatestTimeWithEmptyList(self):
    time_list = []
    self.assertEquals(layouttest_analyzer_helpers.FindLatestTime(time_list),
                      None)

  def testFindLatestTimeWithNoValidStringInList(self):
    time_list = ['.foo1', '232232']
    self.assertEquals(layouttest_analyzer_helpers.FindLatestTime(time_list),
                      None)

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

  def RunTestGetRevisionString(self, current_time_str, prev_time_str,
                               expected_rev_str, expected_simple_rev_str,
                               testname, diff_map_none=False):
    current_time = datetime.strptime(current_time_str, '%Y-%m-%d-%H')
    current_time = time.mktime(current_time.timetuple())
    prev_time = datetime.strptime(prev_time_str, '%Y-%m-%d-%H')
    prev_time = time.mktime(prev_time.timetuple())
    if diff_map_none:
      diff_map = None
    else:
      diff_map = {
        'whole': [[], []],
        'skip': [[(testname, 'te_info1')], []],
        'nonskip': [[], []],
      }
    (rev_str, simple_rev_str) = (
       layouttest_analyzer_helpers.GetRevisionString(prev_time,
                                                     current_time, diff_map))

    self.assertEquals(rev_str, expected_rev_str)
    self.assertEquals(simple_rev_str, expected_simple_rev_str)

  def testGetRevisionString(self):
    expected_rev_str = ('<ul><a href="http://trac.webkit.org/changeset?'
                        'new=94377@trunk/LayoutTests/platform/chromium/'
                        'test_expectations.txt&old=94366@trunk/LayoutTests/'
                        'platform/chromium/test_expectations.txt">94366->'
                        '94377</a>\n'
                        '<li>jamesr@google.com</li>\n'
                        '<li>2011-09-01 18:00:23</li>\n'
                        '<ul><li>-BUGWK63878 : fast/dom/dom-constructors.html'
                        ' = TEXT</li>\n'
                        '</ul></ul>')
    expected_simple_rev_str = ('<a href="http://trac.webkit.org/changeset?'
                               'new=94377@trunk/LayoutTests/platform/chromium/'
                               'test_expectations.txt&old=94366@trunk/'
                               'LayoutTests/platform/chromium/'
                               'test_expectations.txt">94366->94377</a>,')
    self.RunTestGetRevisionString('2011-09-02-00', '2011-09-01-00',
                                  expected_rev_str, expected_simple_rev_str,
                                  'fast/dom/dom-constructors.html')

  def testGetRevisionStringNoneDiffMap(self):
    self.RunTestGetRevisionString('2011-09-02-00', '2011-09-01-00', '', '',
                                  '', diff_map_none=True)

  def testGetRevisionStringNoMatchingTest(self):
    self.RunTestGetRevisionString('2011-09-01-00', '2011-09-02-00', '', '',
                                  'foo1.html')


if __name__ == '__main__':
  unittest.main()
