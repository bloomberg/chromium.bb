#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from datetime import datetime
from datetime import timedelta
import time
import unittest


from test_expectations_history import TestExpectationsHistory


class TestTestExpectationsHistory(unittest.TestCase):
  """Unit tests for the TestExpectationsHistory class."""

  def AssertTestName(self, result_list, testname):
    """Assert test name in the result_list.

    Args:
      result_list: a result list of tuples returned by
          |GetDiffBetweenTimesOnly1Diff()|. Each tuple consists of
          (old_rev, new_rev, author, date, message, lines) where
          |lines| are the entries in the test expectation file.
      testname: a testname string.

    Returns:
      True if the result contains the testname, False otherwise.
    """
    for (_, _, _, _, _, lines) in result_list:
      if any([testname in line for line in lines]):
        return True
    return False

  def testGetDiffBetweenTimes(self):
    t = (2011, 8, 20, 0, 0, 0, 0, 0, 0)
    ctime = time.mktime(t)
    t = (2011, 8, 19, 0, 0, 0, 0, 0, 0)
    ptime = time.mktime(t)
    testname = 'fast/css/getComputedStyle/computed-style-without-renderer.html'
    testname_list = [testname]
    result_list = TestExpectationsHistory.GetDiffBetweenTimes(
        ptime, ctime, testname_list)
    self.assertTrue(self.AssertTestName(result_list, testname))

  def testGetDiffBetweenTimesOnly1Diff(self):
    ptime = datetime.strptime('2011-08-19-23', '%Y-%m-%d-%H')
    ptime = time.mktime(ptime.timetuple())
    ctime = datetime.strptime('2011-08-20-00', '%Y-%m-%d-%H')
    ctime = time.mktime(ctime.timetuple())
    testname = 'fast/css/getComputedStyle/computed-style-without-renderer.html'
    testname_list = [testname]
    result_list = TestExpectationsHistory.GetDiffBetweenTimes(
        ptime, ctime, testname_list)
    self.assertTrue(self.AssertTestName(result_list, testname))

  def testGetDiffBetweenTimesOnly1DiffWithGobackSeveralDays(self):
    ptime = datetime.strptime('2011-09-11-18', '%Y-%m-%d-%H')
    ptime = time.mktime(ptime.timetuple())
    ctime = datetime.strptime('2011-09-11-19', '%Y-%m-%d-%H')
    ctime = time.mktime(ctime.timetuple())
    testname = 'media/video-zoom-controls.html'
    testname_list = [testname]
    result_list = TestExpectationsHistory.GetDiffBetweenTimes(
        ptime, ctime, testname_list)
    self.assertTrue(self.AssertTestName(result_list, testname))


if __name__ == '__main__':
  unittest.main()
