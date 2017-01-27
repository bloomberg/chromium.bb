# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for RelevantChanges."""

from __future__ import print_function

import mock

from chromite.cbuildbot import chromeos_config
from chromite.cbuildbot import relevant_changes
from chromite.lib import clactions
from chromite.lib import constants
from chromite.lib import fake_cidb
from chromite.lib import patch_unittest


# pylint: disable=protected-access
class RelevantChangesTest(patch_unittest.MockPatchBase):
  """Tests for RelevantChanges."""

  def setUp(self):
    self._bot_id = 'master-paladin'
    self.site_config = chromeos_config.GetConfig()
    self.build_config = self.site_config[self._bot_id]

    self.fake_cidb = fake_cidb.FakeCIDBConnection()
    self.master_build_id = self.fake_cidb.InsertBuild(
        self._bot_id, 'chromeos', '1', self._bot_id, 'bot_hostname')

  def test_GetSlaveMappingAndCLActions(self):
    """Tests _GetSlaveMappingAndCLActions."""
    changes = set(self.GetPatches(how_many=4))
    slave_config = 'test-paladin'
    test_build_id = self.fake_cidb.InsertBuild(
        slave_config, 'chromeos', '2', slave_config, 'bot_hostname',
        master_build_id=self.master_build_id, buildbucket_id='bb_id_1')

    for change in changes:
      self.fake_cidb.InsertCLActions(
          test_build_id, [clactions.CLAction.FromGerritPatchAndAction(
              change, constants.CL_ACTION_PICKED_UP)])

    config_map, action_history = (
        relevant_changes.RelevantChanges._GetSlaveMappingAndCLActions(
            self.master_build_id, self.fake_cidb, self.build_config,
            changes, ['bb_id_1']))

    expected_config_map = {
        self.master_build_id: self._bot_id,
        test_build_id: slave_config
    }

    self.assertDictEqual(config_map, expected_config_map)
    self.assertEqual(len(action_history), 4)

  def testGetRelevantChangesForSlaves(self):
    """Tests the logic of GetRelevantChangesForSlaves()."""
    change_set1 = set(self.GetPatches(how_many=2))
    change_set2 = set(self.GetPatches(how_many=3))
    changes = set.union(change_set1, change_set2)
    no_stat = ['no_stat-paladin']
    config_map = {'123': 'foo-paladin',
                  '124': 'bar-paladin',
                  '125': 'no_stat-paladin'}
    changes_by_build_id = {'123': change_set1,
                           '124': change_set2}
    # If a slave did not report status (no_stat), assume all changes
    # are relevant.
    expected = {'foo-paladin': change_set1,
                'bar-paladin': change_set2,
                'no_stat-paladin': changes}

    self.PatchObject(relevant_changes.RelevantChanges,
                     '_GetSlaveMappingAndCLActions',
                     return_value=(config_map, []))
    self.PatchObject(clactions, 'GetRelevantChangesForBuilds',
                     return_value=changes_by_build_id)

    results = relevant_changes.RelevantChanges.GetRelevantChangesForSlaves(
        self.master_build_id, self.fake_cidb, self.build_config, changes,
        no_stat, None)
    self.assertEqual(results, expected)

  def testGetSubsysResultForSlaves(self):
    """Tests for the GetSubsysResultForSlaves."""
    def get_dict(build_config, message_type, message_subtype, message_value):
      return {'build_config': build_config,
              'message_type': message_type,
              'message_subtype': message_subtype,
              'message_value': message_value}

    slave_msgs = [get_dict('config_1', constants.SUBSYSTEMS,
                           constants.SUBSYSTEM_PASS, 'a'),
                  get_dict('config_1', constants.SUBSYSTEMS,
                           constants.SUBSYSTEM_PASS, 'b'),
                  get_dict('config_1', constants.SUBSYSTEMS,
                           constants.SUBSYSTEM_FAIL, 'c'),
                  get_dict('config_2', constants.SUBSYSTEMS,
                           constants.SUBSYSTEM_UNUSED, None),
                  get_dict('config_3', constants.SUBSYSTEMS,
                           constants.SUBSYSTEM_PASS, 'a'),
                  get_dict('config_3', constants.SUBSYSTEMS,
                           constants.SUBSYSTEM_PASS, 'e'),]
    # Setup DB and provide list of slave build messages.
    mock_cidb = mock.MagicMock()
    self.PatchObject(mock_cidb, 'GetSlaveBuildMessages',
                     return_value=slave_msgs)

    expect_result = {
        'config_1': {'pass_subsystems':set(['a', 'b']),
                     'fail_subsystems':set(['c'])},
        'config_2': {},
        'config_3': {'pass_subsystems':set(['a', 'e'])}}
    self.assertEqual(
        relevant_changes.RelevantChanges.GetSubsysResultForSlaves(
            1, mock_cidb),
        expect_result)
