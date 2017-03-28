# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for builder_status_lib."""

from __future__ import print_function

import mock

from chromite.cbuildbot import build_status_unittest
from chromite.lib import buildbucket_lib
from chromite.lib import builder_status_lib
from chromite.lib import constants
from chromite.lib import cros_test_lib
from chromite.lib import failures_lib
from chromite.lib import fake_cidb


# pylint: disable=protected-access
class BuilderStatusManagerTest(cros_test_lib.MockTestCase):
  """Tests for BuilderStatusManager."""

  def testUnpickleBuildStatus(self):
    """Tests that _UnpickleBuildStatus returns the correct values."""
    failed_msg = failures_lib.BuildFailureMessage(
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
    self.buildbucket_client = mock.Mock()

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
