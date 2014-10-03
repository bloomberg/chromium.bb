#!/usr/bin/python
# Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for the retry_stats.py module."""

from __future__ import print_function

import os
import StringIO
import sys
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))


from chromite.lib import cros_test_lib
from chromite.lib import retry_stats


# We access internal members to help with testing.
# pylint: disable=W0212


class TestRetryException(Exception):
  """Used when testing failure cases."""

class TestRetryStats(cros_test_lib.TestCase):
  """This contains test cases for the retry_stats module."""

  CAT = 'Test Service A'
  CAT_B = 'Test Service B'

  SUCCESS_RESULT = 'success result'

  def setUp(self):
    retry_stats._ResetStatsForUnittests()

  def handlerNoRetry(self, _e):
    return False

  def handlerRetry(self, _e):
    return True

  def callSuccess(self):
    return self.SUCCESS_RESULT

  def callFailure(self):
    raise TestRetryException()


  def _verifyStats(self, category, success=0, failure=0, retry=0):
    """Verify that the given category has the specified values collected."""
    stats = retry_stats._STATS_COLLECTION[category]

    self.assertEqual(stats.success.value, success)
    self.assertEqual(stats.failure.value, failure)
    self.assertEqual(stats.retry.value, retry)

  def testSetupStats(self):
    """Verify that we do something when we setup a new stats category."""
    # We start out empty.
    self.assertEqual(retry_stats._STATS_COLLECTION, {})

    # We add something else
    retry_stats.SetupStats()
    self.assertItemsEqual(
        retry_stats._STATS_COLLECTION.keys(),
        retry_stats.CATEGORIES)

    for category in retry_stats.CATEGORIES:
      self._verifyStats(category)

  def testSetupStatsExplicit(self):
    """Verify that we do something when we setup a new stats category."""
    # We start out empty.
    self.assertEqual(retry_stats._STATS_COLLECTION, {})

    # We add something else
    retry_stats.SetupStats([self.CAT, self.CAT_B])
    self.assertEqual(len(retry_stats._STATS_COLLECTION), 2)
    self.assertTrue(self.CAT_B in retry_stats._STATS_COLLECTION)
    self._verifyStats(self.CAT)
    self._verifyStats(self.CAT_B)

  def testReportStatsEmpty(self):
    retry_stats.SetupStats([self.CAT])

    out = StringIO.StringIO()

    retry_stats.ReportStats(out)

    expected = """************************************************************
** Performance Statistics for Test Service A
**
** Success: 0
** Failure: 0
** Retries: 0
** Total: 0
************************************************************
"""

    self.assertEqual(out.getvalue(), expected)

  def testSuccessNoSetup(self):
    self.assertEqual(retry_stats._STATS_COLLECTION, {})

    result = retry_stats.RetryWithStats(
        self.CAT, self.handlerNoRetry, 3, self.callSuccess)
    self.assertEqual(result, self.SUCCESS_RESULT)

    result = retry_stats.RetryWithStats(
        self.CAT, self.handlerNoRetry, 3, self.callSuccess)
    self.assertEqual(result, self.SUCCESS_RESULT)

    self.assertEqual(retry_stats._STATS_COLLECTION, {})


  def testFailureNoRetryNoSetup(self):
    self.assertEqual(retry_stats._STATS_COLLECTION, {})

    self.assertRaises(TestRetryException,
                      retry_stats.RetryWithStats,
                      self.CAT, self.handlerNoRetry, 3, self.callFailure)

    self.assertRaises(TestRetryException,
                      retry_stats.RetryWithStats,
                      self.CAT, self.handlerNoRetry, 3, self.callFailure)

    self.assertEqual(retry_stats._STATS_COLLECTION, {})

  def testSuccess(self):
    retry_stats.SetupStats([self.CAT])
    self._verifyStats(self.CAT)

    # Succeed once.
    result = retry_stats.RetryWithStats(
        self.CAT, self.handlerNoRetry, 3, self.callSuccess)
    self.assertEqual(result, self.SUCCESS_RESULT)
    self._verifyStats(self.CAT, success=1)

    # Succeed twice.
    result = retry_stats.RetryWithStats(
        self.CAT, self.handlerNoRetry, 3, self.callSuccess)
    self.assertEqual(result, self.SUCCESS_RESULT)
    self._verifyStats(self.CAT, success=2)

  def testSuccessRetry(self):
    retry_stats.SetupStats([self.CAT])
    self._verifyStats(self.CAT)

    # Use this scoped list as a persistent counter.
    call_counter = ['fail 1', 'fail 2']

    def callRetrySuccess():
      if call_counter:
        raise TestRetryException(call_counter.pop())
      else:
        return self.SUCCESS_RESULT

    # Retry twice, then succeed.
    result = retry_stats.RetryWithStats(
        self.CAT, self.handlerRetry, 3, callRetrySuccess)
    self.assertEqual(result, self.SUCCESS_RESULT)
    self._verifyStats(self.CAT, success=1, retry=2)

  def testFailureNoRetry(self):
    retry_stats.SetupStats([self.CAT])
    self._verifyStats(self.CAT)

    # Fail once without retries.
    self.assertRaises(TestRetryException,
                      retry_stats.RetryWithStats,
                      self.CAT, self.handlerNoRetry, 3, self.callFailure)
    self._verifyStats(self.CAT, failure=1)

    # Fail twice without retries.
    self.assertRaises(TestRetryException,
                      retry_stats.RetryWithStats,
                      self.CAT, self.handlerNoRetry, 3, self.callFailure)
    self._verifyStats(self.CAT, failure=2)

  def testFailureRetry(self):
    retry_stats.SetupStats([self.CAT])
    self._verifyStats(self.CAT)

    # Fail once with exhausted retries.
    self.assertRaises(TestRetryException,
                      retry_stats.RetryWithStats,
                      self.CAT, self.handlerRetry, 3, self.callFailure)
    self._verifyStats(self.CAT, failure=1, retry=3) # 3 retries = 4 attempts.

    # Fail twice with exhausted retries.
    self.assertRaises(TestRetryException,
                      retry_stats.RetryWithStats,
                      self.CAT, self.handlerRetry, 3, self.callFailure)
    self._verifyStats(self.CAT, failure=2, retry=6)


if __name__ == '__main__':
  cros_test_lib.main()
