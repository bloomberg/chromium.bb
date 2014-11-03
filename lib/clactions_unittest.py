#!/usr/bin/python
# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for clactions methods."""

from __future__ import print_function

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))

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


class TestCLPreCQStatus(cros_test_lib.TestCase):
  """Tests methods related to CL pre-CQ status."""

  def setUp(self):
    self.fake_db = fake_cidb.FakeCIDBConnection()


  def _GetCLStatus(self, change):
    """Helper method to get a CL's pre-CQ status from fake_db."""
    action_history = self.fake_db.GetActionsForChanges([change])
    return clactions.GetCLPreCQStatus(change, action_history)

  def testGetCLPreCQStatus(self):
    change = metadata_lib.GerritPatchTuple(1, 1, False)
    # Initial pre-CQ status of a change is None.
    self.assertEqual(self._GetCLStatus(change), None)

    # Builders can update the CL's pre-CQ status.
    build_id = self.fake_db.InsertBuild(constants.PRE_CQ_LAUNCHER_NAME,
        constants.WATERFALL_INTERNAL, 1, constants.PRE_CQ_LAUNCHER_CONFIG,
        'bot-hostname')

    def act(action):
      self.fake_db.InsertCLActions(
          build_id,
          [clactions.CLAction.FromGerritPatchAndAction(change, action)])

    act(constants.CL_ACTION_PRE_CQ_WAITING)
    self.assertEqual(self._GetCLStatus(change), constants.CL_STATUS_WAITING)

    act(constants.CL_ACTION_PRE_CQ_INFLIGHT)
    self.assertEqual(self._GetCLStatus(change), constants.CL_STATUS_INFLIGHT)

    # Recording a cl action that is not a valid pre-cq status should leave
    # pre-cq status unaffected.
    act('polenta')
    self.assertEqual(self._GetCLStatus(change), constants.CL_STATUS_INFLIGHT)

    # Marking the CL as KICKED_OUT should mark it as FAILED, as a workaround
    # for Bug 429777. TODO(davidjames): Remove this.
    act(constants.CL_ACTION_KICKED_OUT)
    self.assertEqual(self._GetCLStatus(change), constants.CL_STATUS_FAILED)


class TestCLStatusCounter(cros_test_lib.TestCase):
  """Tests that GetCLActionCount behaves as expected."""

  def setUp(self):
    self.fake_db = fake_cidb.FakeCIDBConnection()

  def testGetCLActionCount(self):
    c1p1 = metadata_lib.GerritPatchTuple(1, 1, False)
    c1p2 = metadata_lib.GerritPatchTuple(1, 2, False)
    precq_build_id = self.fake_db.InsertBuild(constants.PRE_CQ_LAUNCHER_NAME,
        constants.WATERFALL_INTERNAL, 1, constants.PRE_CQ_LAUNCHER_CONFIG,
        'bot-hostname')
    melon_build_id = self.fake_db.InsertBuild('melon builder name',
        constants.WATERFALL_INTERNAL, 1, 'melon-config-name',
        'grape-bot-hostname')

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


if __name__ == '__main__':
  cros_test_lib.main()
