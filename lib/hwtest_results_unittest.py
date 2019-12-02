# -*- coding: utf-8 -*-
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for hwtest_results."""

from __future__ import print_function

import mock

from chromite.lib import cros_test_lib
from chromite.lib import fake_cidb
from chromite.lib import git
from chromite.lib import hwtest_results
from chromite.lib import patch_unittest


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
    self._patch_factory = patch_unittest.MockPatchFactory()

  def testGetFailedHwtestsAffectedByChange(self):
    """Test GetFailedHwtestsAffectedByChange."""
    manifest = mock.Mock()
    mock_change = mock.Mock()
    diffs = {'client/site_tests/graphics_dEQP/graphics_dEQP.py': 'M',
             'client/site_tests/graphics_Gbm/graphics_Gbm.py': 'M'}
    mock_change.GetDiffStatus.return_value = diffs
    self.PatchObject(git.ProjectCheckout, 'GetPath')

    failed_hwtests = {'graphics_dEQP'}
    self.assertCountEqual(self.manager.GetFailedHwtestsAffectedByChange(
        mock_change, manifest, failed_hwtests), failed_hwtests)

    failed_hwtests = {'graphics_Gbm'}
    self.assertCountEqual(self.manager.GetFailedHwtestsAffectedByChange(
        mock_change, manifest, failed_hwtests), failed_hwtests)

    failed_hwtests = {'graphics_dEQP', 'graphics_Gbm'}
    self.assertCountEqual(self.manager.GetFailedHwtestsAffectedByChange(
        mock_change, manifest, failed_hwtests), failed_hwtests)

    failed_hwtests = {'audio_ActiveStreamStress'}
    self.assertCountEqual(self.manager.GetFailedHwtestsAffectedByChange(
        mock_change, manifest, failed_hwtests), set())

    failed_hwtests = {'graphics_dEQP', 'graphics_Gbm',
                      'audio_ActiveStreamStress'}
    self.assertCountEqual(self.manager.GetFailedHwtestsAffectedByChange(
        mock_change, manifest, failed_hwtests),
                          {'graphics_dEQP', 'graphics_Gbm'})

  def testFindHWTestFailureSuspects(self):
    """Test FindHWTestFailureSuspects."""
    self.PatchObject(git.ManifestCheckout, 'Cached')
    c1 = self._patch_factory.MockPatch(change_id=1, patch_number=1)
    c2 = self._patch_factory.MockPatch(change_id=2, patch_number=1)
    test_1 = mock.Mock()
    test_2 = mock.Mock()
    self.PatchObject(hwtest_results.HWTestResultManager,
                     'GetFailedHwtestsAffectedByChange',
                     return_value={test_1})
    suspects, no_assignee_hwtests = self.manager.FindHWTestFailureSuspects(
        [c1, c2], mock.Mock(), {test_1, test_2})

    self.assertCountEqual(suspects, {c1, c2})
    self.assertTrue(no_assignee_hwtests)

  def testFindHWTestFailureSuspectsNoAssignees(self):
    """Test FindHWTestFailureSuspects when failures don't have assignees."""
    self.PatchObject(git.ManifestCheckout, 'Cached')
    c1 = self._patch_factory.MockPatch(change_id=1, patch_number=1)
    c2 = self._patch_factory.MockPatch(change_id=2, patch_number=1)
    test_1 = mock.Mock()
    test_2 = mock.Mock()
    self.PatchObject(hwtest_results.HWTestResultManager,
                     'GetFailedHwtestsAffectedByChange',
                     return_value=set())
    suspects, no_assignee_hwtests = self.manager.FindHWTestFailureSuspects(
        [c1, c2], mock.Mock(), {test_1, test_2})

    self.assertCountEqual(suspects, set())
    self.assertTrue(no_assignee_hwtests)

  def testFindHWTestFailureSuspectsNoFailedHWTests(self):
    """Test FindHWTestFailureSuspects with empty failed HWTests."""
    self.PatchObject(git.ManifestCheckout, 'Cached')
    c1 = self._patch_factory.MockPatch(change_id=1, patch_number=1)
    c2 = self._patch_factory.MockPatch(change_id=2, patch_number=1)
    suspects, no_assignee_hwtests = self.manager.FindHWTestFailureSuspects(
        [c1, c2], mock.Mock(), set())

    self.assertCountEqual(suspects, set())
    self.assertFalse(no_assignee_hwtests)
