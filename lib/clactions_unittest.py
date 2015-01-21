# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for clactions methods."""

from __future__ import print_function

import datetime
import itertools

from chromite.cbuildbot import constants
from chromite.cbuildbot import metadata_lib
from chromite.cbuildbot import validation_pool
from chromite.lib import fake_cidb
from chromite.lib import clactions
from chromite.lib import cros_test_lib


class CLActionTest(cros_test_lib.TestCase):
  """Placeholder for clactions unit tests."""

  def runTest(self):
    pass


class IntervalsTest(cros_test_lib.TestCase):
  """Placeholder for clactions unit tests."""

  def testIntervals(self):
    self.assertEqual([], clactions.IntersectIntervals([]))
    self.assertEqual([(1, 2)], clactions.IntersectIntervals([[(1, 2)]]))

    test_group_0 = [(1, 10)]
    test_group_1 = [(2, 5), (7, 10)]
    test_group_2 = [(2, 8), (9, 12)]
    self.assertEqual(
        [(2, 5), (7, 8), (9, 10)],
        clactions.IntersectIntervals([test_group_0, test_group_1, test_group_2])
    )

    test_group_0 = [(1, 3), (10, 12)]
    test_group_1 = [(2, 5)]
    self.assertEqual(
        [(2, 3)],
        clactions.IntersectIntervals([test_group_0, test_group_1]))


class TestCLActionHistory(cros_test_lib.TestCase):
  """Tests various methods related to CL action history."""

  def setUp(self):
    self.fake_db = fake_cidb.FakeCIDBConnection()

  def testGetCLHandlingTime(self):
    """Test that we correctly compute a CL's handling time."""
    change = metadata_lib.GerritPatchTuple(1, 1, False)
    launcher_id = self.fake_db.InsertBuild(
        'launcher', constants.WATERFALL_INTERNAL, 1,
        constants.PRE_CQ_LAUNCHER_CONFIG, 'hostname')
    trybot_id = self.fake_db.InsertBuild(
        'banana pre cq', constants.WATERFALL_INTERNAL, 1,
        'banana-pre-cq', 'hostname')
    master_id = self.fake_db.InsertBuild(
        'CQ master', constants.WATERFALL_INTERNAL, 1,
        constants.CQ_MASTER, 'hostname')
    slave_id = self.fake_db.InsertBuild(
        'banana paladin', constants.WATERFALL_INTERNAL, 1,
        'banana-paladin', 'hostname')

    start_time = datetime.datetime.now()
    c = itertools.count()

    def next_time():
      return start_time + datetime.timedelta(seconds=c.next())

    def a(build_id, action, reason=None):
      self._Act(build_id, change, action, reason=reason, timestamp=next_time())

    # Change is screened, picked up, and rejected by the pre-cq,
    # non-speculatively.
    a(launcher_id, constants.CL_ACTION_VALIDATION_PENDING_PRE_CQ,
      reason='banana-pre-cq')
    a(launcher_id, constants.CL_ACTION_SCREENED_FOR_PRE_CQ)
    a(launcher_id, constants.CL_ACTION_TRYBOT_LAUNCHING,
      reason='banana-pre-cq')
    a(trybot_id, constants.CL_ACTION_PICKED_UP)
    a(trybot_id, constants.CL_ACTION_KICKED_OUT)

    # Change is re-marked by developer, picked up again by pre-cq, verified, and
    # marked as passed.
    a(launcher_id, constants.CL_ACTION_REQUEUED)
    a(launcher_id, constants.CL_ACTION_TRYBOT_LAUNCHING,
      reason='banana-pre-cq')
    a(trybot_id, constants.CL_ACTION_PICKED_UP)
    a(trybot_id, constants.CL_ACTION_VERIFIED)
    a(launcher_id, constants.CL_ACTION_PRE_CQ_FULLY_VERIFIED)
    a(launcher_id, constants.CL_ACTION_PRE_CQ_PASSED)

    # Change is picked up by the CQ and rejected.
    a(master_id, constants.CL_ACTION_PICKED_UP)
    a(slave_id, constants.CL_ACTION_PICKED_UP)
    a(master_id, constants.CL_ACTION_KICKED_OUT)

    # Change is re-marked, picked up by the CQ, and forgiven.
    a(launcher_id, constants.CL_ACTION_REQUEUED)
    a(master_id, constants.CL_ACTION_PICKED_UP)
    a(slave_id, constants.CL_ACTION_PICKED_UP)
    a(master_id, constants.CL_ACTION_FORGIVEN)

    # Change is re-marked, picked up by the CQ, and forgiven.
    a(master_id, constants.CL_ACTION_PICKED_UP)
    a(slave_id, constants.CL_ACTION_PICKED_UP)
    a(master_id, constants.CL_ACTION_SUBMITTED)

    action_history = self.fake_db.GetActionsForChanges([change])
    # Note: There are 2 ticks in the total handling time that are not accounted
    # for in the sub-times. These are the time between VALIDATION_PENDING and
    # SCREENED, and the time between FULLY_VERIFIED and PASSED.
    self.assertEqual(18, clactions.GetCLHandlingTime(change, action_history))
    self.assertEqual(7, clactions.GetPreCQTime(change, action_history))
    self.assertEqual(3, clactions.GetCQWaitTime(change, action_history))
    self.assertEqual(6, clactions.GetCQRunTime(change, action_history))

  def _Act(self, build_id, change, action, reason=None, timestamp=None):
    self.fake_db.InsertCLActions(
        build_id,
        [clactions.CLAction.FromGerritPatchAndAction(change, action, reason)],
        timestamp=timestamp)

  def _GetCLStatus(self, change):
    """Helper method to get a CL's pre-CQ status from fake_db."""
    action_history = self.fake_db.GetActionsForChanges([change])
    return clactions.GetCLPreCQStatus(change, action_history)

  def testGetRequeuedOrSpeculative(self):
    """Tests GetRequeuedOrSpeculative function."""
    change = metadata_lib.GerritPatchTuple(1, 1, False)
    speculative_change = metadata_lib.GerritPatchTuple(2, 2, False)
    changes = [change, speculative_change]

    build_id = self.fake_db.InsertBuild('n', 'w', 1, 'c', 'h')

    # A fresh change should not be marked requeued. A fresh specualtive
    # change should be marked as speculative.
    action_history = self.fake_db.GetActionsForChanges(changes)
    a = clactions.GetRequeuedOrSpeculative(change, action_history, False)
    self.assertEqual(a, None)
    a = clactions.GetRequeuedOrSpeculative(speculative_change, action_history,
                                           True)
    self.assertEqual(a, constants.CL_ACTION_SPECULATIVE)
    self._Act(build_id, speculative_change, a)

    # After picking up either change, neither should need an additional
    # requeued or speculative action.
    self._Act(build_id, speculative_change, constants.CL_ACTION_PICKED_UP)
    self._Act(build_id, change, constants.CL_ACTION_PICKED_UP)
    action_history = self.fake_db.GetActionsForChanges(changes)
    a = clactions.GetRequeuedOrSpeculative(change, action_history, False)
    self.assertEqual(a, None)
    a = clactions.GetRequeuedOrSpeculative(speculative_change, action_history,
                                           True)
    self.assertEqual(a, None)

    # After being rejected, both changes need an action (requeued and
    # speculative accordingly).
    self._Act(build_id, speculative_change, constants.CL_ACTION_KICKED_OUT)
    self._Act(build_id, change, constants.CL_ACTION_KICKED_OUT)
    action_history = self.fake_db.GetActionsForChanges(changes)
    a = clactions.GetRequeuedOrSpeculative(change, action_history, False)
    self.assertEqual(a, constants.CL_ACTION_REQUEUED)
    self._Act(build_id, change, a)
    a = clactions.GetRequeuedOrSpeculative(speculative_change, action_history,
                                           True)
    self.assertEqual(a, constants.CL_ACTION_SPECULATIVE)
    self._Act(build_id, speculative_change, a)

    # Once a speculative change becomes un-speculative, it needs a REQUEUD
    # action.
    action_history = self.fake_db.GetActionsForChanges(changes)
    a = clactions.GetRequeuedOrSpeculative(speculative_change, action_history,
                                           False)
    self.assertEqual(a, constants.CL_ACTION_REQUEUED)
    self._Act(build_id, speculative_change, a)

  def testGetCLPreCQStatus(self):
    change = metadata_lib.GerritPatchTuple(1, 1, False)
    # Initial pre-CQ status of a change is None.
    self.assertEqual(self._GetCLStatus(change), None)

    # Builders can update the CL's pre-CQ status.
    build_id = self.fake_db.InsertBuild(
        constants.PRE_CQ_LAUNCHER_NAME, constants.WATERFALL_INTERNAL, 1,
        constants.PRE_CQ_LAUNCHER_CONFIG, 'bot-hostname')

    self._Act(build_id, change, constants.CL_ACTION_PRE_CQ_WAITING)
    self.assertEqual(self._GetCLStatus(change), constants.CL_STATUS_WAITING)

    self._Act(build_id, change, constants.CL_ACTION_PRE_CQ_INFLIGHT)
    self.assertEqual(self._GetCLStatus(change), constants.CL_STATUS_INFLIGHT)

    # Recording a cl action that is not a valid pre-cq status should leave
    # pre-cq status unaffected.
    self._Act(build_id, change, 'polenta')
    self.assertEqual(self._GetCLStatus(change), constants.CL_STATUS_INFLIGHT)

    self._Act(build_id, change, constants.CL_ACTION_PRE_CQ_RESET)
    self.assertEqual(self._GetCLStatus(change), None)

  def testGetCLPreCQProgress(self):
    change = metadata_lib.GerritPatchTuple(1, 1, False)
    s = lambda: clactions.GetCLPreCQProgress(
        change, self.fake_db.GetActionsForChanges([change]))

    self.assertEqual({}, s())

    # Simulate the pre-cq-launcher screening changes for pre-cq configs
    # to test with.
    launcher_build_id = self.fake_db.InsertBuild(
        constants.PRE_CQ_LAUNCHER_NAME, constants.WATERFALL_INTERNAL,
        1, constants.PRE_CQ_LAUNCHER_CONFIG, 'bot hostname 1')

    self._Act(launcher_build_id, change,
              constants.CL_ACTION_VALIDATION_PENDING_PRE_CQ,
              'pineapple-pre-cq')
    self._Act(launcher_build_id, change,
              constants.CL_ACTION_VALIDATION_PENDING_PRE_CQ,
              'banana-pre-cq')

    configs = ['banana-pre-cq', 'pineapple-pre-cq']

    self.assertEqual(configs, sorted(s().keys()))
    for c in configs:
      self.assertEqual(constants.CL_PRECQ_CONFIG_STATUS_PENDING,
                       s()[c][0])

    # Simulate a prior build rejecting change
    self._Act(launcher_build_id, change,
              constants.CL_ACTION_KICKED_OUT,
              'pineapple-pre-cq')
    self.assertEqual(constants.CL_PRECQ_CONFIG_STATUS_FAILED,
                     s()['pineapple-pre-cq'][0])

    # Simulate the pre-cq-launcher launching tryjobs for all pending configs.
    for c in configs:
      self._Act(launcher_build_id, change,
                constants.CL_ACTION_TRYBOT_LAUNCHING, c)
    for c in configs:
      self.assertEqual(constants.CL_PRECQ_CONFIG_STATUS_LAUNCHED,
                       s()[c][0])

    # Simulate the tryjobs launching, and picking up the changes.
    banana_build_id = self.fake_db.InsertBuild(
        'banana', constants.WATERFALL_TRYBOT, 12, 'banana-pre-cq',
        'banana hostname')
    pineapple_build_id = self.fake_db.InsertBuild(
        'pineapple', constants.WATERFALL_TRYBOT, 87, 'pineapple-pre-cq',
        'pineapple hostname')

    self._Act(banana_build_id, change, constants.CL_ACTION_PICKED_UP)
    self._Act(pineapple_build_id, change, constants.CL_ACTION_PICKED_UP)
    for c in configs:
      self.assertEqual(constants.CL_PRECQ_CONFIG_STATUS_INFLIGHT,
                       s()[c][0])

    # Simulate the changes being retried.
    self._Act(banana_build_id, change, constants.CL_ACTION_FORGIVEN)
    self._Act(launcher_build_id, change, constants.CL_ACTION_FORGIVEN,
              'pineapple-pre-cq')
    for c in configs:
      self.assertEqual(constants.CL_PRECQ_CONFIG_STATUS_PENDING,
                       s()[c][0])
    # Simulate the changes being rejected, either by the configs themselves
    # or by the pre-cq-launcher.
    self._Act(banana_build_id, change, constants.CL_ACTION_KICKED_OUT)
    self._Act(launcher_build_id, change, constants.CL_ACTION_KICKED_OUT,
              'pineapple-pre-cq')
    for c in configs:
      self.assertEqual(constants.CL_PRECQ_CONFIG_STATUS_FAILED,
                       s()[c][0])
    # Simulate the tryjobs verifying the changes.
    self._Act(banana_build_id, change, constants.CL_ACTION_VERIFIED)
    self._Act(pineapple_build_id, change, constants.CL_ACTION_VERIFIED)
    for c in configs:
      self.assertEqual(constants.CL_PRECQ_CONFIG_STATUS_VERIFIED,
                       s()[c][0])

    # Simulate the pre-cq status being reset.
    self._Act(launcher_build_id, change, constants.CL_ACTION_PRE_CQ_RESET)
    self.assertEqual({}, s())

  def testGetCLPreCQCategoriesAndPendingCLs(self):
    c1 = metadata_lib.GerritPatchTuple(1, 1, False)
    c2 = metadata_lib.GerritPatchTuple(2, 2, False)
    c3 = metadata_lib.GerritPatchTuple(3, 3, False)
    c4 = metadata_lib.GerritPatchTuple(4, 4, False)
    c5 = metadata_lib.GerritPatchTuple(5, 5, False)

    launcher_build_id = self.fake_db.InsertBuild(
        constants.PRE_CQ_LAUNCHER_NAME, constants.WATERFALL_INTERNAL,
        1, constants.PRE_CQ_LAUNCHER_CONFIG, 'bot hostname 1')
    pineapple_build_id = self.fake_db.InsertBuild(
        'pineapple', constants.WATERFALL_TRYBOT, 87, 'pineapple-pre-cq',
        'pineapple hostname')
    guava_build_id = self.fake_db.InsertBuild(
        'guava', constants.WATERFALL_TRYBOT, 7, 'guava-pre-cq',
        'guava hostname')

    # c1 has 3 pending verifications, but only 1 inflight and 1
    # launching, so it is not busy/inflight.
    self._Act(launcher_build_id, c1,
              constants.CL_ACTION_VALIDATION_PENDING_PRE_CQ,
              'pineapple-pre-cq')
    self._Act(launcher_build_id, c1,
              constants.CL_ACTION_VALIDATION_PENDING_PRE_CQ,
              'banana-pre-cq')
    self._Act(launcher_build_id, c1,
              constants.CL_ACTION_VALIDATION_PENDING_PRE_CQ,
              'guava-pre-cq')
    self._Act(launcher_build_id, c1,
              constants.CL_ACTION_TRYBOT_LAUNCHING,
              'banana-pre-cq')
    self._Act(pineapple_build_id, c1, constants.CL_ACTION_PICKED_UP)

    # c2 has 3 pending verifications, 1 inflight and 1 launching, and 1 passed,
    # so it is busy.
    self._Act(launcher_build_id, c2,
              constants.CL_ACTION_VALIDATION_PENDING_PRE_CQ,
              'pineapple-pre-cq')
    self._Act(launcher_build_id, c2,
              constants.CL_ACTION_VALIDATION_PENDING_PRE_CQ,
              'banana-pre-cq')
    self._Act(launcher_build_id, c2,
              constants.CL_ACTION_VALIDATION_PENDING_PRE_CQ,
              'guava-pre-cq')
    self._Act(launcher_build_id, c2, constants.CL_ACTION_TRYBOT_LAUNCHING,
              'banana-pre-cq')
    self._Act(pineapple_build_id, c2, constants.CL_ACTION_PICKED_UP)
    self._Act(guava_build_id, c2, constants.CL_ACTION_VERIFIED)

    # c3 has 2 pending verifications, both passed, so it is passed.
    self._Act(launcher_build_id, c3,
              constants.CL_ACTION_VALIDATION_PENDING_PRE_CQ,
              'pineapple-pre-cq')
    self._Act(launcher_build_id, c3,
              constants.CL_ACTION_VALIDATION_PENDING_PRE_CQ,
              'guava-pre-cq')
    self._Act(pineapple_build_id, c3, constants.CL_ACTION_VERIFIED)
    self._Act(guava_build_id, c3, constants.CL_ACTION_VERIFIED)

    # c4 has 2 pending verifications: one is inflight and the other
    # passed. It is considered inflight and busy.
    self._Act(launcher_build_id, c4,
              constants.CL_ACTION_VALIDATION_PENDING_PRE_CQ,
              'pineapple-pre-cq')
    self._Act(launcher_build_id, c4,
              constants.CL_ACTION_VALIDATION_PENDING_PRE_CQ,
              'guava-pre-cq')
    self._Act(pineapple_build_id, c4, constants.CL_ACTION_PICKED_UP)
    self._Act(guava_build_id, c4, constants.CL_ACTION_VERIFIED)

    # c5 has not even been screened.

    changes = [c1, c2, c3, c4, c5]
    action_history = self.fake_db.GetActionsForChanges(changes)
    progress_map = clactions.GetPreCQProgressMap(changes, action_history)

    self.assertEqual(({c2, c4}, {c4}, {c3}),
                     clactions.GetPreCQCategories(progress_map))

    # Among changes c1, c2, c3, only the guava-pre-cq config is pending. The
    # other configs are either inflight, launching, or passed everywhere.
    screened_changes = set(changes).intersection(progress_map)
    self.assertEqual({'guava-pre-cq'},
                     clactions.GetPreCQConfigsToTest(screened_changes,
                                                     progress_map))


class TestCLStatusCounter(cros_test_lib.TestCase):
  """Tests that GetCLActionCount behaves as expected."""

  def setUp(self):
    self.fake_db = fake_cidb.FakeCIDBConnection()

  def testGetCLActionCount(self):
    c1p1 = metadata_lib.GerritPatchTuple(1, 1, False)
    c1p2 = metadata_lib.GerritPatchTuple(1, 2, False)
    precq_build_id = self.fake_db.InsertBuild(
        constants.PRE_CQ_LAUNCHER_NAME, constants.WATERFALL_INTERNAL, 1,
        constants.PRE_CQ_LAUNCHER_CONFIG, 'bot-hostname')
    melon_build_id = self.fake_db.InsertBuild(
        'melon builder name', constants.WATERFALL_INTERNAL, 1,
        'melon-config-name', 'grape-bot-hostname')

    # Count should be zero before any actions are recorded.

    action_history = self.fake_db.GetActionsForChanges([c1p1])
    self.assertEqual(
        0,
        clactions.GetCLActionCount(
            c1p1, validation_pool.CQ_PIPELINE_CONFIGS,
            constants.CL_ACTION_KICKED_OUT, action_history))

    # Record 3 failures for c1p1, and some other actions. Only count the
    # actions from builders in validation_pool.CQ_PIPELINE_CONFIGS.
    self.fake_db.InsertCLActions(
        precq_build_id,
        [clactions.CLAction.FromGerritPatchAndAction(
            c1p1, constants.CL_ACTION_KICKED_OUT)])
    self.fake_db.InsertCLActions(
        precq_build_id,
        [clactions.CLAction.FromGerritPatchAndAction(
            c1p1, constants.CL_ACTION_PICKED_UP)])
    self.fake_db.InsertCLActions(
        precq_build_id,
        [clactions.CLAction.FromGerritPatchAndAction(
            c1p1, constants.CL_ACTION_KICKED_OUT)])
    self.fake_db.InsertCLActions(
        melon_build_id,
        [clactions.CLAction.FromGerritPatchAndAction(
            c1p1, constants.CL_ACTION_KICKED_OUT)])

    action_history = self.fake_db.GetActionsForChanges([c1p1])
    self.assertEqual(
        2,
        clactions.GetCLActionCount(
            c1p1, validation_pool.CQ_PIPELINE_CONFIGS,
            constants.CL_ACTION_KICKED_OUT, action_history))

    # Record a failure for c1p2. Now the latest patches failure count should be
    # 1 (true weather we pass c1p1 or c1p2), whereas the total failure count
    # should be 3.
    self.fake_db.InsertCLActions(
        precq_build_id,
        [clactions.CLAction.FromGerritPatchAndAction(
            c1p2, constants.CL_ACTION_KICKED_OUT)])

    action_history = self.fake_db.GetActionsForChanges([c1p1])
    self.assertEqual(
        1,
        clactions.GetCLActionCount(
            c1p1, validation_pool.CQ_PIPELINE_CONFIGS,
            constants.CL_ACTION_KICKED_OUT, action_history))
    self.assertEqual(
        1,
        clactions.GetCLActionCount(
            c1p2, validation_pool.CQ_PIPELINE_CONFIGS,
            constants.CL_ACTION_KICKED_OUT, action_history))
    self.assertEqual(
        3,
        clactions.GetCLActionCount(
            c1p2, validation_pool.CQ_PIPELINE_CONFIGS,
            constants.CL_ACTION_KICKED_OUT, action_history,
            latest_patchset_only=False))
