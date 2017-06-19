# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for builder_status_lib."""

from __future__ import print_function

import mock

from chromite.cbuildbot import build_status_unittest
from chromite.lib import buildbucket_lib
from chromite.lib import builder_status_lib
from chromite.lib import build_failure_message
from chromite.lib import config_lib
from chromite.lib import constants
from chromite.lib import cros_test_lib
from chromite.lib import fake_cidb
from chromite.lib import failure_message_lib
from chromite.lib import failure_message_lib_unittest
from chromite.lib import metadata_lib


bb_infos = build_status_unittest.BuildbucketInfos
cidb_infos = build_status_unittest.CIDBStatusInfos
failure_msg_helper = failure_message_lib_unittest.FailureMessageHelper
stage_failure_helper = failure_message_lib_unittest.StageFailureHelper


# pylint: disable=protected-access
class BuilderStatusManagerTest(cros_test_lib.MockTestCase):
  """Tests for BuilderStatusManager."""

  def testUnpickleBuildStatus(self):
    """Tests that _UnpickleBuildStatus returns the correct values."""
    failed_msg = build_failure_message.BuildFailureMessage(
        'you failed', ['traceback'], True, 'taco', 'bot')
    failed_input_status = builder_status_lib.BuilderStatus(
        constants.BUILDER_STATUS_FAILED, failed_msg)
    passed_input_status = builder_status_lib.BuilderStatus(
        constants.BUILDER_STATUS_PASSED, None)

    failed_output_status = (
        builder_status_lib.BuilderStatusManager._UnpickleBuildStatus(
            failed_input_status.AsPickledDict()))
    passed_output_status = (
        builder_status_lib.BuilderStatusManager._UnpickleBuildStatus(
            passed_input_status.AsPickledDict()))
    empty_string_status = (
        builder_status_lib.BuilderStatusManager._UnpickleBuildStatus(''))

    self.assertEqual(failed_input_status.AsFlatDict(),
                     failed_output_status.AsFlatDict())
    self.assertEqual(passed_input_status.AsFlatDict(),
                     passed_output_status.AsFlatDict())
    self.assertTrue(empty_string_status.Failed())

class SlaveBuilderStatusTest(cros_test_lib.MockTestCase):
  """Tests for SlaveBuilderStatus."""

  def setUp(self):
    self.db = fake_cidb.FakeCIDBConnection()
    self.master_build_id = 0
    self.site_config = config_lib.GetConfig()
    self.config = self.site_config['master-paladin']
    self.metadata = metadata_lib.CBuildbotMetadata()
    self.db = fake_cidb.FakeCIDBConnection()
    self.buildbucket_client = mock.Mock()
    self.slave_1 = 'cyan-paladin'
    self.slave_2 = 'auron-paladin'
    self.builders_array = [self.slave_1, self.slave_2]

  def testGetAllSlaveBuildbucketInfo(self):
    """Test GetAllSlaveBuildbucketInfo."""

    # Test completed builds.
    buildbucket_info_dict = {
        'build1': buildbucket_lib.BuildbucketInfo(
            'id_1', 1, 0, None, None, None),
        'build2': buildbucket_lib.BuildbucketInfo(
            'id_2', 1, 0, None, None, None)
    }
    self.PatchObject(buildbucket_lib, 'GetScheduledBuildDict',
                     return_value=buildbucket_info_dict)

    expected_status = 'COMPLETED'
    expected_result = 'SUCCESS'
    expected_url = 'fake_url'
    content = {
        'build': {
            'status': expected_status,
            'result': expected_result,
            'url': expected_url
        }
    }

    self.buildbucket_client.GetBuildRequest.return_value = content
    updated_buildbucket_info_dict = (
        builder_status_lib.SlaveBuilderStatus.GetAllSlaveBuildbucketInfo(
            self.buildbucket_client, buildbucket_info_dict))

    self.assertEqual(updated_buildbucket_info_dict['build1'].status,
                     expected_status)
    self.assertEqual(updated_buildbucket_info_dict['build1'].result,
                     expected_result)
    self.assertEqual(updated_buildbucket_info_dict['build1'].url,
                     expected_url)
    self.assertEqual(updated_buildbucket_info_dict['build2'].status,
                     expected_status)
    self.assertEqual(updated_buildbucket_info_dict['build2'].result,
                     expected_result)
    self.assertEqual(updated_buildbucket_info_dict['build1'].url,
                     expected_url)

    # Test started builds.
    expected_status = 'STARTED'
    expected_result = None
    content = {
        'build': {
            'status': 'STARTED'
        }
    }
    self.buildbucket_client.GetBuildRequest.return_value = content
    updated_buildbucket_info_dict = (
        builder_status_lib.SlaveBuilderStatus.GetAllSlaveBuildbucketInfo(
            self.buildbucket_client, buildbucket_info_dict))

    self.assertEqual(updated_buildbucket_info_dict['build1'].status,
                     expected_status)
    self.assertEqual(updated_buildbucket_info_dict['build1'].result,
                     expected_result)
    self.assertEqual(updated_buildbucket_info_dict['build2'].status,
                     expected_status)
    self.assertEqual(updated_buildbucket_info_dict['build2'].result,
                     expected_result)

    # Test BuildbucketResponseException failures.
    self.buildbucket_client.GetBuildRequest.side_effect = (
        buildbucket_lib.BuildbucketResponseException)
    updated_buildbucket_info_dict = (
        builder_status_lib.SlaveBuilderStatus.GetAllSlaveBuildbucketInfo(
            self.buildbucket_client, buildbucket_info_dict))
    self.assertIsNone(updated_buildbucket_info_dict['build1'].status)
    self.assertIsNone(updated_buildbucket_info_dict['build2'].status)

  def _InsertMasterSlaveBuildsToCIDB(self):
    """Insert master and slave builds into fake_cidb."""
    master = self.db.InsertBuild('master', constants.WATERFALL_INTERNAL, 1,
                                 'master', 'host1')
    slave1 = self.db.InsertBuild('slave1', constants.WATERFALL_INTERNAL, 2,
                                 'slave1', 'host1', master_build_id=0,
                                 buildbucket_id='id_1', status='fail')
    slave2 = self.db.InsertBuild('slave2', constants.WATERFALL_INTERNAL, 3,
                                 'slave2', 'host1', master_build_id=0,
                                 buildbucket_id='id_2', status='fail')
    return master, slave1, slave2

  def testGetAllSlaveCIDBStatusInfo(self):
    """GetAllSlaveCIDBStatusInfo without Buildbucket info."""
    _, slave1_id, slave2_id = self._InsertMasterSlaveBuildsToCIDB()

    expected_status = {
        'slave1': builder_status_lib.CIDBStatusInfo(slave1_id, 'fail', 2),
        'slave2': builder_status_lib.CIDBStatusInfo(slave2_id, 'fail', 3)
    }

    cidb_status = (
        builder_status_lib.SlaveBuilderStatus.GetAllSlaveCIDBStatusInfo(
            self.db, self.master_build_id, None))
    self.assertDictEqual(cidb_status, expected_status)

    cidb_status = (
        builder_status_lib.SlaveBuilderStatus.GetAllSlaveCIDBStatusInfo(
            self.db, self.master_build_id, None))
    self.assertDictEqual(cidb_status, expected_status)

  def testGetAllSlaveCIDBStatusInfoWithBuildbucket(self):
    """GetAllSlaveCIDBStatusInfo with Buildbucket info."""
    _, slave1_id, slave2_id = self._InsertMasterSlaveBuildsToCIDB()

    buildbucket_info_dict = {
        'slave1': build_status_unittest.BuildbucketInfos.GetStartedBuild(
            bb_id='id_1'),
        'slave2': build_status_unittest.BuildbucketInfos.GetStartedBuild(
            bb_id='id_2')
    }

    expected_status = {
        'slave1': builder_status_lib.CIDBStatusInfo(slave1_id, 'fail', 2),
        'slave2': builder_status_lib.CIDBStatusInfo(slave2_id, 'fail', 3)
    }

    cidb_status = (
        builder_status_lib.SlaveBuilderStatus.GetAllSlaveCIDBStatusInfo(
            self.db, self.master_build_id, buildbucket_info_dict))
    self.assertDictEqual(cidb_status, expected_status)

    cidb_status = (
        builder_status_lib.SlaveBuilderStatus.GetAllSlaveCIDBStatusInfo(
            self.db, self.master_build_id, buildbucket_info_dict))
    self.assertDictEqual(cidb_status, expected_status)

  def testGetAllSlaveCIDBStatusInfoWithRetriedBuilds(self):
    """GetAllSlaveCIDBStatusInfo doesn't return retried builds."""
    self._InsertMasterSlaveBuildsToCIDB()
    self.db.InsertBuild('slave1', constants.WATERFALL_INTERNAL, 3,
                        'slave1', 'host1', master_build_id=0,
                        buildbucket_id='id_3', status='inflight')

    buildbucket_info_dict = {
        'slave1': build_status_unittest.BuildbucketInfos.GetStartedBuild(
            bb_id='id_3'),
        'slave2': build_status_unittest.BuildbucketInfos.GetStartedBuild(
            bb_id='id_4')
    }

    cidb_status = (
        builder_status_lib.SlaveBuilderStatus.GetAllSlaveCIDBStatusInfo(
            self.db, self.master_build_id, buildbucket_info_dict))
    self.assertEqual(set(cidb_status.keys()), set(['slave1']))
    self.assertEqual(cidb_status['slave1'].status, 'inflight')


  def ConstructBuilderStatusManager(self,
                                    master_build_id=None,
                                    db=None,
                                    config=None,
                                    metadata=None,
                                    buildbucket_client=None,
                                    builders_array=None,
                                    dry_run=True):
    if master_build_id is None:
      master_build_id = self.master_build_id
    if db is None:
      db = self.db
    if config is None:
      config = self.config
    if metadata is None:
      metadata = self.metadata
    if buildbucket_client is None:
      buildbucket_client = self.buildbucket_client
    if builders_array is None:
      builders_array = self.builders_array

    return builder_status_lib.SlaveBuilderStatus(
        master_build_id, db, config, metadata, buildbucket_client,
        builders_array, dry_run)

  def testGetSlaveFailures(self):
    """Test _GetSlaveFailures."""
    self.PatchObject(builder_status_lib.SlaveBuilderStatus, '_InitSlaveInfo')
    entry_1 = stage_failure_helper.GetStageFailure(
        build_config=self.slave_1, failure_id=1)
    entry_2 = stage_failure_helper.GetStageFailure(
        build_config=self.slave_1, failure_id=2, outer_failure_id=1)
    entry_3 = stage_failure_helper.GetStageFailure(
        build_config=self.slave_2, failure_id=3)
    failure_entries = [entry_1, entry_2, entry_3]
    mock_db = mock.Mock()
    mock_db.GetSlaveFailures.return_value = failure_entries
    manager = self.ConstructBuilderStatusManager(db=mock_db)
    slave_failures_dict = manager._GetSlaveFailures(None)

    self.assertItemsEqual(slave_failures_dict.keys(),
                          [self.slave_1, self.slave_2])
    self.assertEqual(len(slave_failures_dict[self.slave_1]), 1)
    self.assertEqual(len(slave_failures_dict[self.slave_2]), 1)
    self.assertTrue(isinstance(slave_failures_dict[self.slave_1][0],
                               failure_message_lib.CompoundFailureMessage))
    self.assertTrue(isinstance(slave_failures_dict[self.slave_2][0],
                               failure_message_lib.StageFailureMessage))

  def testInitSlaveInfoWithBuildbucket(self):
    """Test _InitSlaveInfo with Buildbucket info."""
    self.PatchObject(config_lib, 'UseBuildbucketScheduler', return_value=True)
    self.PatchObject(builder_status_lib.SlaveBuilderStatus,
                     '_GetSlaveFailures')
    self.PatchObject(builder_status_lib.SlaveBuilderStatus,
                     'GetAllSlaveBuildbucketInfo',
                     return_value={self.slave_1: bb_infos.GetSuccessBuild()})
    manager = self.ConstructBuilderStatusManager()

    self.assertItemsEqual(manager.builders_array, [self.slave_1])
    self.assertIsNotNone(manager.buildbucket_info_dict)

  def testInitSlaveInfoOnBuildWithoutBuildbucket(self):
    """Test _InitSlaveInfo without Buildbucket info."""
    self.PatchObject(config_lib, 'UseBuildbucketScheduler', return_value=False)
    self.PatchObject(builder_status_lib.SlaveBuilderStatus,
                     '_GetSlaveFailures')
    self.PatchObject(builder_status_lib.SlaveBuilderStatus,
                     'GetAllSlaveBuildbucketInfo',
                     return_value={self.slave_1: bb_infos.GetSuccessBuild()})
    manager = self.ConstructBuilderStatusManager()

    self.assertItemsEqual(manager.builders_array, [self.slave_1, self.slave_2])
    self.assertIsNone(manager.buildbucket_info_dict)

  def testGetStatusWithBuildbucket(self):
    """Test _GetStatus with Buildbucket info."""
    self.PatchObject(builder_status_lib.SlaveBuilderStatus, '_InitSlaveInfo')
    cidb_info_dict = cidb_infos.GetFullCIDBStatusInfo()
    buildbucket_info_dict = bb_infos.GetFullBuildbucketInfoDict()
    manager = self.ConstructBuilderStatusManager()

    status_1 = manager._GetStatus(
        'scheduled', cidb_info_dict, buildbucket_info_dict)
    self.assertEqual(status_1, constants.BUILDER_STATUS_MISSING)
    status_2 = manager._GetStatus(
        'started', cidb_info_dict, buildbucket_info_dict)
    self.assertEqual(status_2, constants.BUILDER_STATUS_INFLIGHT)
    status_3 = manager._GetStatus(
        'completed_success', cidb_info_dict, buildbucket_info_dict)
    self.assertEqual(status_3, constants.BUILDER_STATUS_PASSED)
    status_4 = manager._GetStatus(
        'completed_failure', cidb_info_dict, buildbucket_info_dict)
    self.assertEqual(status_4, constants.BUILDER_STATUS_FAILED)
    status_5 = manager._GetStatus(
        'completed_canceled', cidb_info_dict, buildbucket_info_dict)
    self.assertEqual(status_5, constants.BUILDER_STATUS_FAILED)

  def testGetStatusWithoutBuildbucket(self):
    """Test _GetStatus without Buildbucket info."""
    self.PatchObject(builder_status_lib.SlaveBuilderStatus, '_InitSlaveInfo')
    cidb_info_dict = cidb_infos.GetFullCIDBStatusInfo()
    manager = self.ConstructBuilderStatusManager()

    status_1 = manager._GetStatus('started', cidb_info_dict, None)
    self.assertEqual(status_1, constants.BUILDER_STATUS_INFLIGHT)
    status_2 = manager._GetStatus('completed_canceled', cidb_info_dict, None)
    self.assertEqual(status_2, constants.BUILDER_STATUS_INFLIGHT)

  def testCreateBuildFailureMessageWithMessages(self):
    """Test CreateBuildFailureMessage with stage failure messages."""
    overlays = constants.PRIVATE_OVERLAYS
    dashboard_url = 'http://fake_dashboard_url'
    failure_messages = self._ConstructFailureMessages(self.slave_1)

    build_msg = builder_status_lib.SlaveBuilderStatus.CreateBuildFailureMessage(
        self.slave_1, overlays, dashboard_url, failure_messages)

    self.assertTrue('stage failed' in build_msg.message_summary)
    self.assertTrue(build_msg.internal)
    self.assertEqual(build_msg.builder, self.slave_1)

  def testCreateBuildFailureMessageWithoutMessages(self):
    """Test CreateBuildFailureMessage without stage failure messages."""
    overlays = constants.PUBLIC_OVERLAYS
    dashboard_url = 'http://fake_dashboard_url'

    build_msg = builder_status_lib.SlaveBuilderStatus.CreateBuildFailureMessage(
        self.slave_1, overlays, dashboard_url, None)

    self.assertTrue('cbuildbot failed' in build_msg.message_summary)
    self.assertFalse(build_msg.internal)
    self.assertEqual(build_msg.builder, self.slave_1)

  def test_GetDashboardUrlWithBuildbucket(self):
    """Test _GetDashboardUrl with Buildbucket info."""
    self.PatchObject(builder_status_lib.SlaveBuilderStatus, '_InitSlaveInfo')
    manager = self.ConstructBuilderStatusManager()
    cidb_info_dict = {
        self.slave_1: cidb_infos.GetPassedBuild(build_id=1, build_number=100)}
    buildbucket_info_dict = {
        self.slave_1: bb_infos.GetSuccessBuild(url='http://buildbucket_url'),
        self.slave_2:  bb_infos.GetSuccessBuild(url='http://buildbucket_url'),
    }

    dashboard_url = manager._GetDashboardUrl(
        self.slave_1, cidb_info_dict, buildbucket_info_dict)
    self.assertEqual(
        dashboard_url,
        'https://luci-milo.appspot.com/buildbot/chromeos/cyan-paladin/100')

    dashboard_url = manager._GetDashboardUrl(
        self.slave_2, cidb_info_dict, buildbucket_info_dict)
    self.assertEqual(dashboard_url, 'http://buildbucket_url')

  def testGetDashboardUrlWithoutBuildbucket(self):
    """Test _GetDashboardUrl without Buildbucket info."""
    self.PatchObject(builder_status_lib.SlaveBuilderStatus, '_InitSlaveInfo')
    manager = self.ConstructBuilderStatusManager()

    dashboard_url = manager._GetDashboardUrl(self.slave_1, {}, None)
    self.assertIsNone(dashboard_url)

  def _ConstructFailureMessages(self, build_config):
    entry_1 = stage_failure_helper.GetStageFailure(
        build_config=build_config, failure_id=1)
    entry_2 = stage_failure_helper.GetStageFailure(
        build_config=build_config, failure_id=2, outer_failure_id=1)
    entry_3 = stage_failure_helper.GetStageFailure(
        build_config=build_config, failure_id=3, outer_failure_id=1)
    failure_entries = [entry_1, entry_2, entry_3]
    failure_messages = (
        failure_message_lib.FailureMessageManager.ConstructStageFailureMessages(
            failure_entries))

    return failure_messages

  def testGetMessageOnFailedBuilds(self):
    """Test _GetMessage on failed builds."""
    self.PatchObject(builder_status_lib.SlaveBuilderStatus, '_InitSlaveInfo')
    manager = self.ConstructBuilderStatusManager()
    failure_messages = self._ConstructFailureMessages(self.slave_1)
    slave_failures_dict = {self.slave_1: failure_messages}
    message = manager._GetMessage(
        self.slave_1, constants.BUILDER_STATUS_FAILED, 'dashboard_url',
        slave_failures_dict)

    self.assertIsNotNone(message)

  def testGetMessageOnNotFailedBuilds(self):
    """Test _GetMessage on not failed builds."""
    self.PatchObject(builder_status_lib.SlaveBuilderStatus, '_InitSlaveInfo')
    manager = self.ConstructBuilderStatusManager()
    message = manager._GetMessage(
        self.slave_1, constants.BUILDER_STATUS_PASSED, 'dashboard_url', {})

    self.assertIsNone(message)

  def testGetBuilderStatusForBuild(self):
    """Test GetBuilderStatusForBuild."""
    self.PatchObject(builder_status_lib.SlaveBuilderStatus, '_InitSlaveInfo')
    self.PatchObject(builder_status_lib.SlaveBuilderStatus, '_GetStatus')
    self.PatchObject(builder_status_lib.SlaveBuilderStatus, '_GetDashboardUrl')
    self.PatchObject(builder_status_lib.SlaveBuilderStatus, '_GetMessage')
    manager = self.ConstructBuilderStatusManager()
    self.assertIsNotNone(manager.GetBuilderStatusForBuild(self.slave_1))
