# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for hwtest_results."""

from __future__ import print_function

from chromite.lib import constants
from chromite.lib import cros_test_lib
from chromite.lib import fake_cidb
from chromite.lib import hwtest_results


class HWTestResultTest(cros_test_lib.MockTestCase):
  """Tests for HWTestResult."""

  def testNormalizeTestName(self):
    """Test NormalizeTestName."""
    self.assertEqual(hwtest_results.HWTestResult.NormalizeTestName(
        'Suite job'), None)
    self.assertEqual(hwtest_results.HWTestResult.NormalizeTestName(
        'cheets_CTS.com.android.cts.dram'), 'cheets_CTS')
    self.assertEqual(hwtest_results.HWTestResult.NormalizeTestName(
        'security_NetworkListeners'), 'security_NetworkListeners')


class HWTestResultManagerTest(cros_test_lib.MockTestCase):
  """Tests for HWTestResultManager."""

  def setUp(self):
    self.manager = hwtest_results.HWTestResultManager()
    self.db = fake_cidb.FakeCIDBConnection()

  def _ReportHWTestResults(self):
    build_1 = self.db.InsertBuild('build_1', constants.WATERFALL_INTERNAL, 1,
                                  'build_1', 'bot_hostname')
    build_2 = self.db.InsertBuild('build_2', constants.WATERFALL_INTERNAL, 2,
                                  'build_2', 'bot_hostname')
    r1 = hwtest_results.HWTestResult.FromReport(
        build_1, 'Suite job', 'pass')
    r2 = hwtest_results.HWTestResult.FromReport(
        build_1, 'test_b.test', 'fail')
    r3 = hwtest_results.HWTestResult.FromReport(
        build_1, 'test_c.test', 'abort')
    r4 = hwtest_results.HWTestResult.FromReport(
        build_2, 'test_d.test', 'other')
    r5 = hwtest_results.HWTestResult.FromReport(
        build_2, 'test_e', 'pass')

    self.db.InsertHWTestResults([r1, r2, r3, r4, r5])

    return ((build_1, build_2), (r1, r2, r3, r4, r5))

  def GetHWTestResultsFromCIDB(self):
    """Test GetHWTestResultsFromCIDB."""
    (build_1, build_2), _ = self._ReportHWTestResults()

    expect_r1 = hwtest_results.HWTestResult(0, build_1, 'Suite job', 'pass')
    expect_r2 = hwtest_results.HWTestResult(1, build_1, 'test_b.test', 'fail')
    expect_r3 = hwtest_results.HWTestResult(2, build_1, 'test_c.test', 'abort')
    expect_r4 = hwtest_results.HWTestResult(3, build_2, 'test_d.test', 'other')
    expect_r5 = hwtest_results.HWTestResult(4, build_2, 'test_e', 'pass')

    results = self.manager.GetHWTestResultsFromCIDB(self.db, [build_1])
    self.assertItemsEqual(results, [expect_r1, expect_r2, expect_r3])

    results = self.manager.GetHWTestResultsFromCIDB(
        self.db, [build_1], test_statues=constants.HWTEST_STATUES_NOT_PASSED)
    self.assertItemsEqual(results, [expect_r2, expect_r3])

    results = self.manager.GetHWTestResultsFromCIDB(self.db, [build_1, build_2])
    self.assertItemsEqual(
        results, [expect_r1, expect_r2, expect_r3, expect_r4, expect_r5])

    results = self.manager.GetHWTestResultsFromCIDB(
        self.db, [build_1, build_2],
        test_statues=constants.HWTEST_STATUES_NOT_PASSED)
    self.assertItemsEqual(results, [expect_r2, expect_r3, expect_r4])

  def testGetFailedHWTestsFromCIDB(self):
    """Test GetFailedHWTestsFromCIDB."""
    (build_1, build_2), (r1, r2, r3, r4, r5) = self._ReportHWTestResults()
    mock_get_hwtest_results = self.PatchObject(
        hwtest_results.HWTestResultManager, 'GetHWTestResultsFromCIDB',
        return_value=[r1, r2, r3, r4, r5])
    failed_tests = self.manager.GetFailedHWTestsFromCIDB(
        self.db, [build_1, build_2])

    self.assertItemsEqual(failed_tests,
                          ['test_b', 'test_c', 'test_d', 'test_e'])
    mock_get_hwtest_results.assert_called_once_with(
        self.db, [build_1, build_2],
        test_statues=constants.HWTEST_STATUES_NOT_PASSED)
