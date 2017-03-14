# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing unit tests for build_status."""

from __future__ import print_function

import datetime
import mock
import time

from chromite.cbuildbot import buildbucket_lib
from chromite.cbuildbot import build_status
from chromite.cbuildbot import relevant_changes
from chromite.cbuildbot import validation_pool_unittest
from chromite.lib import constants
from chromite.lib import config_lib
from chromite.lib import fake_cidb
from chromite.lib import metadata_lib
from chromite.lib import patch_unittest

# pylint: disable=protected-access

class BuildbucketInfos(object):
  """Helper methods to build BuildbucketInfo."""

  @staticmethod
  def GetScheduledBuild(bb_id='scheduled_id_1', retry=0):
    return buildbucket_lib.BuildbucketInfo(
        buildbucket_id=bb_id,
        retry=retry,
        created_ts=1,
        status=constants.BUILDBUCKET_BUILDER_STATUS_SCHEDULED,
        result=None
    )

  @staticmethod
  def GetStartedBuild(bb_id='started_id_1', retry=0):
    return buildbucket_lib.BuildbucketInfo(
        buildbucket_id=bb_id,
        retry=retry,
        created_ts=1,
        status=constants.BUILDBUCKET_BUILDER_STATUS_STARTED,
        result=None
    )

  @staticmethod
  def GetSuccessBuild(bb_id='success_id_1', retry=0):
    return buildbucket_lib.BuildbucketInfo(
        buildbucket_id=bb_id,
        retry=retry,
        created_ts=1,
        status=constants.BUILDBUCKET_BUILDER_STATUS_COMPLETED,
        result=constants.BUILDBUCKET_BUILDER_RESULT_SUCCESS
    )

  @staticmethod
  def GetFailureBuild(bb_id='failure_id_1', retry=0):
    return buildbucket_lib.BuildbucketInfo(
        buildbucket_id=bb_id,
        retry=retry,
        created_ts=1,
        status=constants.BUILDBUCKET_BUILDER_STATUS_COMPLETED,
        result=constants.BUILDBUCKET_BUILDER_RESULT_FAILURE
    )

  @staticmethod
  def GetCanceledBuild(bb_id='canceled_id_1', retry=0):
    return buildbucket_lib.BuildbucketInfo(
        buildbucket_id=bb_id,
        retry=retry,
        created_ts=1,
        status=constants.BUILDBUCKET_BUILDER_STATUS_COMPLETED,
        result=constants.BUILDBUCKET_BUILDER_RESULT_CANCELED
    )

  @staticmethod
  def GetMissingBuild(bb_id='missing_id_1', retry=0):
    return buildbucket_lib.BuildbucketInfo(
        buildbucket_id=bb_id,
        retry=retry,
        created_ts=1,
        status=None,
        result=None
    )

class SlaveStatusTest(patch_unittest.MockPatchBase):
  """Test methods testing methods in SalveStatus class."""

  def setUp(self):
    self.time_now = datetime.datetime.now()
    self.master_build_id = 0
    self.master_test_config = config_lib.BuildConfig(
        name='master-test', master=True,
        active_waterfall=constants.WATERFALL_INTERNAL)
    self.master_cq_config = config_lib.BuildConfig(
        name='master-paladin', master=True,
        active_waterfall=constants.WATERFALL_INTERNAL)
    self.master_canary_config = config_lib.BuildConfig(
        name='master-release', master=True,
        active_waterfall=constants.WATERFALL_INTERNAL)
    self.metadata = metadata_lib.CBuildbotMetadata()
    self.db = fake_cidb.FakeCIDBConnection()
    self.buildbucket_client = mock.Mock()

  def _GetSlaveStatus(self, start_time=None, builders_array=None,
                      master_build_id=None, db=None, config=None,
                      metadata=None, buildbucket_client=None, version=None,
                      pool=None, dry_run=True):
    if start_time is None:
      start_time = self.time_now
    if builders_array is None:
      builders_array = []
    if master_build_id is None:
      master_build_id = self.master_build_id
    if db is None:
      db = self.db
    if metadata is None:
      metadata = self.metadata
    if buildbucket_client is None:
      buildbucket_client = self.buildbucket_client

    return build_status.SlaveStatus(
        start_time, builders_array, master_build_id, db,
        config=config,
        metadata=metadata,
        buildbucket_client=buildbucket_client,
        version=version,
        pool=pool,
        dry_run=dry_run)

  def _Mock_GetSlaveStatusesFromCIDB(self, cidb_status=None):
    return self.PatchObject(build_status.SlaveStatus,
                            '_GetNewSlaveCIDBStatusInfo',
                            return_value=cidb_status)

  def _Mock_GetSlaveStatusesFromBuildbucket(self, buildbucket_info_dict=None):
    self.PatchObject(build_status.SlaveStatus,
                     'GetAllSlaveBuildbucketInfo')
    self.PatchObject(build_status.SlaveStatus,
                     '_GetNewlyCompletedSlaveBuildbucketInfo',
                     return_value=buildbucket_info_dict)
    self.PatchObject(buildbucket_lib, 'GetBuildInfoDict',
                     return_value=buildbucket_info_dict)

  def _Mock_GetRetriableBuilds(self, builds=None):
    return self.PatchObject(build_status.SlaveStatus,
                            '_GetRetriableBuilds',
                            return_value=builds)

  def _Mock_RetryBuilds(self, builds=None):
    if builds is None:
      builds = set()
    self.PatchObject(build_status.SlaveStatus, '_RetryBuilds',
                     return_value=builds)

  def _GetFullBuildConfigs(self, exclude_builds=None):
    build_config_list = ['scheduled', 'started', 'completed_success',
                         'completed_failure', 'completed_canceled']

    if exclude_builds:
      for exclude_build in exclude_builds:
        build_config_list.remove(exclude_build)

    return build_config_list

  def _GetFullBuildInfoDict(self, exclude_builds=None):
    buildbucket_info_dict = {
        'scheduled': BuildbucketInfos.GetScheduledBuild(),
        'started': BuildbucketInfos.GetStartedBuild(),
        'completed_success': BuildbucketInfos.GetSuccessBuild(),
        'completed_failure': BuildbucketInfos.GetFailureBuild(),
        'completed_canceled': BuildbucketInfos.GetCanceledBuild()
    }

    if exclude_builds:
      for exclude_build in exclude_builds:
        buildbucket_info_dict.pop(exclude_build, None)

    return buildbucket_info_dict

  def _GetCompletedBuildInfoDict(self):
    return {
        'completed_success': BuildbucketInfos.GetSuccessBuild(),
        'completed_failure': BuildbucketInfos.GetFailureBuild(),
        'completed_canceled': BuildbucketInfos.GetCanceledBuild()
    }

  def _GetCompletedAllSet(self):
    return set(['completed_success',
                'completed_failure',
                'completed_canceled'])

  def _GetFullCIDBStatusInfo(self, exclude_builds=None):
    cidb_status = {
        'started': build_status.CIDBStatusInfo(
            1, constants.BUILDER_STATUS_INFLIGHT),
        'completed_success': build_status.CIDBStatusInfo(
            2, constants.BUILDER_STATUS_PASSED),
        'completed_failure': build_status.CIDBStatusInfo(
            3, constants.BUILDER_STATUS_FAILED),
        'completed_canceled': build_status.CIDBStatusInfo(
            4, constants.BUILDER_STATUS_INFLIGHT)
    }

    if exclude_builds:
      for exclude_build in exclude_builds:
        cidb_status.pop(exclude_build, None)

    return cidb_status

  def testGetSlaveStatusWithValidationPool(self):
    """Test build SlaveStatus with ValidationPool."""
    self.patch_mock = self.StartPatcher(
        validation_pool_unittest.MockPatchSeries())
    p = self.GetPatches(how_many=3)
    pool = validation_pool_unittest.MakePool(applied=p)

    self.patch_mock.SetGerritDependencies(p[0], [])
    self.patch_mock.SetGerritDependencies(p[1], [])
    self.patch_mock.SetGerritDependencies(p[2], [])

    self.patch_mock.SetCQDependencies(p[1], [p[0]])
    self.patch_mock.SetCQDependencies(p[2], [p[0]])

    slave_status = self._GetSlaveStatus(
        builders_array=self._GetFullBuildConfigs(),
        config=self.master_cq_config,
        pool=pool)

    expected_map = {
        p[0]: {p[1], p[2]},
    }

    self.assertDictEqual(expected_map, slave_status.dependency_map)

  def testGetMissingBuilds(self):
    """Tests GetMissingBuilds returns the missing builders."""
    cidb_status = {
        'build1': build_status.CIDBStatusInfo(
            1, constants.BUILDER_STATUS_FAILED),
        'build2': build_status.CIDBStatusInfo(
            2, constants.BUILDER_STATUS_INFLIGHT)
    }
    self._Mock_GetSlaveStatusesFromCIDB(cidb_status)

    slave_status = self._GetSlaveStatus(
        builders_array=['build1', 'build2', 'missing_builder'])

    self.assertEqual(slave_status._GetMissingBuilds(), set(['missing_builder']))

  def testGetMissingBuildsWithBuildbucket(self):
    """Tests GetMissingBuilds returns the missing builders with Buildbucket."""
    cidb_status = {
        'started':  build_status.CIDBStatusInfo(
            1, constants.BUILDER_STATUS_INFLIGHT)
    }
    self._Mock_GetSlaveStatusesFromCIDB(cidb_status)

    buildbucket_info_dict = {
        'scheduled': BuildbucketInfos.GetScheduledBuild(),
        'started': BuildbucketInfos.GetStartedBuild(),
        'missing': BuildbucketInfos.GetMissingBuild()}
    self._Mock_GetSlaveStatusesFromBuildbucket(buildbucket_info_dict)

    slave_status = self._GetSlaveStatus(
        builders_array=['scheduled', 'started', 'missing'],
        config=self.master_cq_config)

    self.assertEqual(slave_status._GetMissingBuilds(), set(['missing']))

  def testGetMissingBuildsNone(self):
    """Tests GetMissingBuilds returns None."""
    cidb_status = {
        'build1': build_status.CIDBStatusInfo(
            1, constants.BUILDER_STATUS_FAILED),
        'build2': build_status.CIDBStatusInfo(
            2, constants.BUILDER_STATUS_INFLIGHT)
    }
    self._Mock_GetSlaveStatusesFromCIDB(cidb_status)

    slave_status = self._GetSlaveStatus(
        builders_array=['build1', 'build2'])

    self.assertEqual(slave_status._GetMissingBuilds(), set())

  def testGetMissingBuildsNoneWithBuildbucket(self):
    """Tests GetMissing returns None with Buildbucket."""
    cidb_status = {
        'started': build_status.CIDBStatusInfo(
            1, constants.BUILDER_STATUS_INFLIGHT)}
    self._Mock_GetSlaveStatusesFromCIDB(cidb_status)

    buildbucket_info_dict = {
        'scheduled': BuildbucketInfos.GetScheduledBuild(),
        'started': BuildbucketInfos.GetStartedBuild()
    }
    self._Mock_GetSlaveStatusesFromBuildbucket(buildbucket_info_dict)

    slave_status = self._GetSlaveStatus(
        builders_array=['build1', 'build2'],
        config=self.master_cq_config)

    self.assertEqual(slave_status._GetMissingBuilds(), set())

  def testGetCompletedBuilds(self):
    """Tests GetCompletedBuilds returns the completed builds."""
    cidb_status = {
        'passed': build_status.CIDBStatusInfo(
            1, constants.BUILDER_STATUS_PASSED),
        'failed': build_status.CIDBStatusInfo(
            2, constants.BUILDER_STATUS_FAILED),
        'aborted': build_status.CIDBStatusInfo(
            3, constants.BUILDER_STATUS_ABORTED),
        'skipped': build_status.CIDBStatusInfo(
            4, constants.BUILDER_STATUS_SKIPPED),
        'forgiven': build_status.CIDBStatusInfo(
            5, constants.BUILDER_STATUS_FORGIVEN),
        'inflight': build_status.CIDBStatusInfo(
            6, constants.BUILDER_STATUS_INFLIGHT),
        'missing': build_status.CIDBStatusInfo(
            7, constants.BUILDER_STATUS_MISSING),
        'planned': build_status.CIDBStatusInfo(
            8, constants.BUILDER_STATUS_PLANNED)
    }
    self._Mock_GetSlaveStatusesFromCIDB(cidb_status)

    slave_status = self._GetSlaveStatus(
        builders_array=['passed', 'failed', 'aborted', 'skipped', 'forgiven',
                        'inflight', 'missing', 'planning'])

    self.assertEqual(slave_status._GetCompletedBuilds(),
                     set(['passed', 'failed', 'aborted', 'skipped',
                          'forgiven']))

  def testGetRetriableBuildsReturnsNone(self):
    """GetRetriableBuilds returns no build to retry."""
    self._Mock_GetSlaveStatusesFromCIDB(
        self._GetFullCIDBStatusInfo())

    self._Mock_GetSlaveStatusesFromBuildbucket(
        self._GetFullBuildInfoDict())

    slave_status = self._GetSlaveStatus(
        builders_array=self._GetFullBuildConfigs(),
        config=self.master_canary_config)
    self.assertEqual(
        slave_status._GetRetriableBuilds(self._GetCompletedAllSet()),
        set())

    self.db.InsertBuildStage(3, 'CommitQueueSync',
                             status=constants.BUILDER_STATUS_PASSED)
    self.db.InsertBuildStage(4, 'MasterSlaveLKGMSync',
                             status=constants.BUILDER_STATUS_PASSED)
    slave_status = self._GetSlaveStatus(
        builders_array=self._GetFullBuildConfigs(),
        config=self.master_cq_config)
    self.assertEqual(
        slave_status._GetRetriableBuilds(self._GetCompletedAllSet()),
        set())

  def _MockForGetRetriableBuildsTests(self, exclude_cidb_builds=None):
    """Helper method for GetRetriableBuilds tests."""
    self._Mock_GetSlaveStatusesFromCIDB(
        self._GetFullCIDBStatusInfo(exclude_builds=exclude_cidb_builds))

    self._Mock_GetSlaveStatusesFromBuildbucket(
        self._GetFullBuildInfoDict())

  def testGetRetriableBuildsNotRetryOnStartedBuilds(self):
    """test _GetRetriableBuilds for master not retrying started builds."""
    self._MockForGetRetriableBuildsTests(
        exclude_cidb_builds=['completed_canceled'])

    slave_status = self._GetSlaveStatus(
        builders_array=self._GetFullBuildConfigs(),
        config=self.master_canary_config)
    self.assertEqual(
        slave_status._GetRetriableBuilds(self._GetCompletedAllSet()),
        set(['completed_canceled']))

  def testGetRetriableBuildsRetryOnStartedBuilds(self):
    """Retry the slave if it fails to pass the critical stage."""
    self._MockForGetRetriableBuildsTests()

    self.db.InsertBuildStage(3, 'CommitQueueSync',
                             status=constants.BUILDER_STATUS_FAILED)
    self.db.InsertBuildStage(4, 'MasterSlaveLKGMSync',
                             status=constants.BUILDER_STATUS_FAILED)
    slave_status = self._GetSlaveStatus(
        builders_array=self._GetFullBuildConfigs(),
        config=self.master_cq_config)
    self.assertEqual(
        slave_status._GetRetriableBuilds(self._GetCompletedAllSet()),
        set(['completed_failure', 'completed_canceled']))

  def testGetRetriableBuildsRetryOnStartedBuilds_2(self):
    """Retry the slave if it fails to pass the critical stage."""
    self._MockForGetRetriableBuildsTests()

    slave_status = self._GetSlaveStatus(
        builders_array=self._GetFullBuildConfigs(),
        config=self.master_cq_config)
    self.assertEqual(
        slave_status._GetRetriableBuilds(self._GetCompletedAllSet()),
        set(['completed_failure', 'completed_canceled']))

  def testGetRetriableBuildsRetryOnStartedBuilds_3(self):
    """Retry the slave if it fails to pass the critical stage."""
    self._MockForGetRetriableBuildsTests()

    self.db.InsertBuildStage(3, 'CommitQueueSync',
                             status=constants.BUILDER_STATUS_PLANNED)
    self.db.InsertBuildStage(4, 'MasterSlaveLKGMSync',
                             status=constants.BUILDER_STATUS_PLANNED)
    slave_status = self._GetSlaveStatus(
        builders_array=self._GetFullBuildConfigs(),
        config=self.master_cq_config)
    self.assertEqual(
        slave_status._GetRetriableBuilds(self._GetCompletedAllSet()),
        set(['completed_failure', 'completed_canceled']))

  def testGetRetriableBuildsExceedsLimit(self):
    """Do not return builds which have exceeded the retry_limit."""
    cidb_status = {
        'started': build_status.CIDBStatusInfo(
            1, constants.BUILDER_STATUS_INFLIGHT),
        'completed_success': build_status.CIDBStatusInfo(
            2, constants.BUILDER_STATUS_PASSED)
    }
    self._Mock_GetSlaveStatusesFromCIDB(cidb_status)

    buildbucket_info_dict = {
        'scheduled': BuildbucketInfos.GetScheduledBuild(),
        'started': BuildbucketInfos.GetStartedBuild(),
        'completed_success': BuildbucketInfos.GetSuccessBuild(),
        'completed_failure': BuildbucketInfos.GetFailureBuild(
            retry=constants.BUILDBUCKET_BUILD_RETRY_LIMIT),
        'completed_canceled': BuildbucketInfos.GetCanceledBuild()}
    self._Mock_GetSlaveStatusesFromBuildbucket(buildbucket_info_dict)

    slave_status = self._GetSlaveStatus(
        builders_array=self._GetFullBuildConfigs(),
        config=self.master_cq_config)

    self.assertEqual(
        slave_status._GetRetriableBuilds(self._GetCompletedAllSet()),
        set(['completed_canceled']))

  def testGetCompletedBuildsWithBuildbucket(self):
    """Tests GetCompletedBuilds with Buildbucket"""
    self._Mock_GetSlaveStatusesFromCIDB(
        self._GetFullCIDBStatusInfo())

    self._Mock_GetSlaveStatusesFromBuildbucket(
        self._GetFullBuildInfoDict())

    slave_status = self._GetSlaveStatus(
        builders_array=self._GetFullBuildConfigs(),
        config=self.master_canary_config)

    self.assertEqual(slave_status._GetCompletedBuilds(),
                     set(['completed_success', 'completed_failure',
                          'completed_canceled']))

    self._Mock_GetRetriableBuilds(builds=set(['completed_failure']))
    slave_status = self._GetSlaveStatus(
        builders_array=self._GetFullBuildConfigs(),
        config=self.master_cq_config)

    self.assertEqual(slave_status._GetCompletedBuilds(),
                     set(['completed_success', 'completed_canceled']))

  def testGetBuildsToRetry(self):
    """Test GetBuildsToRetry."""
    cidb_status = {
        'started': build_status.CIDBStatusInfo(
            1, constants.BUILDER_STATUS_INFLIGHT),
        'completed_success': build_status.CIDBStatusInfo(
            2, constants.BUILDER_STATUS_PASSED),
        'completed_canceled': build_status.CIDBStatusInfo(
            3, constants.BUILDER_STATUS_INFLIGHT)
    }
    self._Mock_GetSlaveStatusesFromCIDB(cidb_status)

    slave_status = self._GetSlaveStatus(
        builders_array=self._GetFullBuildConfigs())
    self.assertEqual(slave_status._GetBuildsToRetry(), None)

  def testGetBuildsToRetryWithBuildbucket(self):
    """Test GetBuildsToRetry with Buildbucket."""
    self._Mock_GetSlaveStatusesFromCIDB(
        self._GetFullCIDBStatusInfo(exclude_builds=['completed_failure']))

    self._Mock_GetSlaveStatusesFromBuildbucket(
        self._GetFullBuildInfoDict())

    slave_status = self._GetSlaveStatus(
        builders_array=self._GetFullBuildConfigs(),
        config=self.master_canary_config)

    self.assertEqual(slave_status._GetBuildsToRetry(),
                     set(['completed_failure']))

  def testCompleted(self):
    """Tests Completed returns proper bool."""
    builders_array = ['build1', 'build2']
    statusNotCompleted = {
        'build1': build_status.CIDBStatusInfo(
            1, constants.BUILDER_STATUS_FAILED),
        'build2': build_status.CIDBStatusInfo(
            2, constants.BUILDER_STATUS_INFLIGHT)
    }
    self._Mock_GetSlaveStatusesFromCIDB(statusNotCompleted)
    slaveStatusNotCompleted = self._GetSlaveStatus(
        builders_array=builders_array)
    self.assertFalse(slaveStatusNotCompleted._Completed())

    statusCompleted = {
        'build1': build_status.CIDBStatusInfo(
            1, constants.BUILDER_STATUS_FAILED),
        'build2': build_status.CIDBStatusInfo(
            2, constants.BUILDER_STATUS_PASSED)
    }
    self._Mock_GetSlaveStatusesFromCIDB(statusCompleted)
    slaveStatusCompleted = self._GetSlaveStatus(
        builders_array=builders_array)
    self.assertTrue(slaveStatusCompleted._Completed())

  def testCompletedWithBuildbucket(self):
    """Tests Completed returns proper bool with Buildbucket."""
    builders_array = ['started', 'failure', 'missing']
    status_not_completed = {
        'started': build_status.CIDBStatusInfo(
            1, constants.BUILDER_STATUS_INFLIGHT),
        'failure': build_status.CIDBStatusInfo(
            2, constants.BUILDER_STATUS_FAILED)
    }
    self._Mock_GetSlaveStatusesFromCIDB(status_not_completed)

    buildbucket_info_dict_not_completed = {
        'started': BuildbucketInfos.GetStartedBuild(),
        'failure': BuildbucketInfos.GetFailureBuild(),
        'missing': BuildbucketInfos.GetMissingBuild()}
    self._Mock_GetSlaveStatusesFromBuildbucket(
        buildbucket_info_dict_not_completed)

    slaveStatusNotCompleted = self._GetSlaveStatus(
        builders_array=builders_array,
        config=self.master_cq_config)

    self.assertFalse(slaveStatusNotCompleted._Completed())

    status_completed = {
        'success': build_status.CIDBStatusInfo(
            1, constants.BUILDER_STATUS_PASSED),
        'failure': build_status.CIDBStatusInfo(
            2, constants.BUILDER_STATUS_FAILED)
    }
    self._Mock_GetSlaveStatusesFromCIDB(status_completed)

    buildbucket_info_dict_complted = {
        'success': BuildbucketInfos.GetSuccessBuild(),
        'failure': BuildbucketInfos.GetFailureBuild()}
    self._Mock_GetSlaveStatusesFromBuildbucket(
        buildbucket_info_dict_complted)

    builders_array = ['success', 'failure']
    slaveStatusCompleted = self._GetSlaveStatus(
        builders_array=builders_array,
        config=self.master_canary_config)
    self.assertTrue(slaveStatusCompleted._Completed())

    self._Mock_GetRetriableBuilds(builds=set())
    slaveStatusCompleted = self._GetSlaveStatus(
        builders_array=builders_array,
        config=self.master_cq_config)
    self.assertTrue(slaveStatusCompleted._Completed())

  def testCompletedWithNoSlaveScheduledByBuildbucket(self):
    """Tests Completed returns True when no slaves are scheduled."""
    self._Mock_GetSlaveStatusesFromCIDB({})
    self._Mock_GetSlaveStatusesFromBuildbucket({})
    slaveStatusCompleted = self._GetSlaveStatus(
        builders_array=['build1', 'build2'],
        config=self.master_cq_config)
    self.assertTrue(slaveStatusCompleted._Completed())

  def testShouldFailForBuilderStartTimeoutTrue(self):
    """Tests that ShouldFailForBuilderStartTimeout says fail when it should."""
    cidb_status = {
        'build1': build_status.CIDBStatusInfo(
            1, constants.BUILDER_STATUS_FAILED)
    }
    self._Mock_GetSlaveStatusesFromCIDB(cidb_status)

    start_time = datetime.datetime.now()
    slave_status = self._GetSlaveStatus(
        builders_array=['build1', 'build2'])
    check_time = start_time + datetime.timedelta(
        minutes=slave_status.BUILD_START_TIMEOUT_MIN + 1)

    self.assertTrue(slave_status._ShouldFailForBuilderStartTimeout(check_time))

  def testShouldFailForBuilderStartTimeoutTrueWithBuildbucket(self):
    """Tests that ShouldFailForBuilderStartTimeout says fail when it should."""
    cidb_status = {
        'success': build_status.CIDBStatusInfo(
            1, constants.BUILDER_STATUS_PASSED)
    }
    self._Mock_GetSlaveStatusesFromCIDB(cidb_status)

    buildbucket_info_dict = {
        'success': BuildbucketInfos.GetSuccessBuild(),
        'scheduled': BuildbucketInfos.GetScheduledBuild()
    }
    self._Mock_GetSlaveStatusesFromBuildbucket(buildbucket_info_dict)

    start_time = datetime.datetime.now()
    slave_status = self._GetSlaveStatus(
        start_time=start_time,
        builders_array=['success', 'scheduled'],
        config=self.master_cq_config)
    check_time = start_time + datetime.timedelta(
        minutes=slave_status.BUILD_START_TIMEOUT_MIN + 1)

    self.assertTrue(slave_status._ShouldFailForBuilderStartTimeout(check_time))

  def testShouldFailForBuilderStartTimeoutFalseTooEarly(self):
    """Tests that ShouldFailForBuilderStartTimeout doesn't fail.

    Make sure that we don't fail if there are missing builders but we're
    checking before the timeout and the other builders have completed.
    """
    cidb_status = {
        'build1': build_status.CIDBStatusInfo(
            1, constants.BUILDER_STATUS_FAILED)
    }
    self._Mock_GetSlaveStatusesFromCIDB(cidb_status)

    start_time = datetime.datetime.now()
    slave_status = self._GetSlaveStatus(
        start_time=start_time,
        builders_array=['build1', 'build2'])

    self.assertFalse(slave_status._ShouldFailForBuilderStartTimeout(start_time))

  def testShouldFailForBuilderStartTimeoutFalseTooEarlyWithBuildbucket(self):
    """Tests that ShouldFailForBuilderStartTimeout doesn't fail.

    With Buildbucket, make sure that we don't fail if there are missing
    builders but we're checking before the timeout and the other builders
    have completed.
    """
    cidb_status = {
        'success': build_status.CIDBStatusInfo(
            1, constants.BUILDER_STATUS_PASSED)
    }
    self._Mock_GetSlaveStatusesFromCIDB(cidb_status)

    buildbucket_info_dict = {
        'success': BuildbucketInfos.GetSuccessBuild(),
        'missing': BuildbucketInfos.GetMissingBuild()}
    self._Mock_GetSlaveStatusesFromBuildbucket(buildbucket_info_dict)

    start_time = datetime.datetime.now()
    slave_status = self._GetSlaveStatus(
        builders_array=['success', 'missing'],
        config=self.master_cq_config)

    self.assertFalse(slave_status._ShouldFailForBuilderStartTimeout(start_time))

  def testShouldFailForBuilderStartTimeoutFalseNotCompleted(self):
    """Tests that ShouldFailForBuilderStartTimeout doesn't fail.

    Make sure that we don't fail if there are missing builders and we're
    checking after the timeout but the other builders haven't completed.
    """
    cidb_status = {
        'build1': build_status.CIDBStatusInfo(
            1, constants.BUILDER_STATUS_INFLIGHT)
    }
    self._Mock_GetSlaveStatusesFromCIDB(cidb_status)

    start_time = datetime.datetime.now()
    slave_status = self._GetSlaveStatus(
        start_time=start_time,
        builders_array=['build1', 'build2'],
        config=self.master_cq_config,
        metadata=self.metadata,
        buildbucket_client=self.buildbucket_client)
    check_time = start_time + datetime.timedelta(
        minutes=slave_status.BUILD_START_TIMEOUT_MIN + 1)

    self.assertFalse(slave_status._ShouldFailForBuilderStartTimeout(check_time))

  def testShouldFailForStartTimeoutFalseNotCompletedWithBuildbucket(self):
    """Tests that ShouldWait doesn't fail with Buildbucket.

    With Buildbucket, make sure that we don't fail if there are missing builders
    and we're checking after the timeout but the other builders haven't
    completed.
    """
    cidb_status = {
        'started': build_status.CIDBStatusInfo(
            1, constants.BUILDER_STATUS_INFLIGHT)
    }
    self._Mock_GetSlaveStatusesFromCIDB(cidb_status)

    buildbucket_info_dict = {
        'started': BuildbucketInfos.GetStartedBuild(),
        'missing': BuildbucketInfos.GetMissingBuild()}
    self._Mock_GetSlaveStatusesFromBuildbucket(buildbucket_info_dict)

    start_time = datetime.datetime.now()
    slave_status = self._GetSlaveStatus(
        start_time=start_time,
        builders_array=['started', 'missing'],
        config=self.master_cq_config)
    check_time = start_time + datetime.timedelta(
        minutes=slave_status.BUILD_START_TIMEOUT_MIN + 1)

    self.assertFalse(slave_status._ShouldFailForBuilderStartTimeout(check_time))

  def testShouldWaitAllBuildersCompleted(self):
    """Tests that ShouldWait says no waiting because all builders finished."""
    cidb_status = {
        'build1': build_status.CIDBStatusInfo(
            1, constants.BUILDER_STATUS_FAILED),
        'build2': build_status.CIDBStatusInfo(
            2, constants.BUILDER_STATUS_PASSED)
    }
    self._Mock_GetSlaveStatusesFromCIDB(cidb_status)

    slave_status = self._GetSlaveStatus(
        builders_array=['build1', 'build2'])

    self.assertFalse(slave_status.ShouldWait())

  def testShouldWaitAllBuildersCompletedWithBuildbucket(self):
    """ShouldWait says no because all builders finished with Buildbucket."""
    cidb_status = {
        'failure': build_status.CIDBStatusInfo(
            1, constants.BUILDER_STATUS_FAILED),
        'success': build_status.CIDBStatusInfo(
            2, constants.BUILDER_STATUS_PASSED)
    }
    self._Mock_GetSlaveStatusesFromCIDB(cidb_status)

    buildbucket_info_dict = {
        'failure': BuildbucketInfos.GetFailureBuild(),
        'success': BuildbucketInfos.GetSuccessBuild()}
    self._Mock_GetSlaveStatusesFromBuildbucket(buildbucket_info_dict)

    slave_status = self._GetSlaveStatus(
        builders_array=['failure', 'success'],
        config=self.master_canary_config)

    self.assertFalse(slave_status.ShouldWait())

  def testShouldWaitMissingBuilder(self):
    """Tests that ShouldWait says no waiting because a builder is missing."""
    cidb_status = {
        'build1': build_status.CIDBStatusInfo(
            1, constants.BUILDER_STATUS_FAILED)
    }
    self._Mock_GetSlaveStatusesFromCIDB(cidb_status)

    slave_status = self._GetSlaveStatus(
        start_time=datetime.datetime.now() - datetime.timedelta(hours=1),
        builders_array=['build1', 'build2'])

    self.assertFalse(slave_status.ShouldWait())

  def testShouldWaitScheduledBuilderWithBuildbucket(self):
    """Test ShouldWait on canary-master with scheduled builds."""
    cidb_status = {
        'failure': build_status.CIDBStatusInfo(
            1, constants.BUILDER_STATUS_FAILED)
    }
    self._Mock_GetSlaveStatusesFromCIDB(cidb_status)

    buildbucket_info_dict = {
        'failure': BuildbucketInfos.GetFailureBuild(),
        'scheduled': BuildbucketInfos.GetScheduledBuild()}
    self._Mock_GetSlaveStatusesFromBuildbucket(buildbucket_info_dict)

    slave_status = self._GetSlaveStatus(
        start_time=datetime.datetime.now() - datetime.timedelta(hours=1),
        builders_array=['failure', 'scheduled'],
        config=self.master_canary_config)
    self.assertFalse(slave_status.ShouldWait())

  def testShouldWaitScheduledBuilderWithBuildbucket_2(self):
    """Test ShouldWait on CQ-master with scheduled builds."""
    cidb_status = {
        'failure': build_status.CIDBStatusInfo(
            1, constants.BUILDER_STATUS_FAILED)
    }
    self._Mock_GetSlaveStatusesFromCIDB(cidb_status)

    buildbucket_info_dict = {
        'failure': BuildbucketInfos.GetFailureBuild(),
        'scheduled': BuildbucketInfos.GetScheduledBuild()}
    self._Mock_GetSlaveStatusesFromBuildbucket(buildbucket_info_dict)

    self._Mock_GetRetriableBuilds(set())
    slave_status = self._GetSlaveStatus(
        start_time=datetime.datetime.now() - datetime.timedelta(hours=1),
        builders_array=['failure', 'scheduled'],
        config=self.master_cq_config)
    self.assertFalse(slave_status.ShouldWait())

    self._Mock_GetRetriableBuilds(set(['failure']))
    self._Mock_RetryBuilds()
    slave_status = self._GetSlaveStatus(
        start_time=datetime.datetime.now() - datetime.timedelta(hours=1),
        builders_array=['failure', 'scheduled'],
        config=self.master_cq_config)
    self.assertTrue(slave_status.ShouldWait())

  def testShouldWaitNoScheduledBuilderWithBuildbucket(self):
    """Test ShouldWait on canary-master without scheduled builds."""
    cidb_status = {
        'failure': build_status.CIDBStatusInfo(
            1, constants.BUILDER_STATUS_FAILED),
        'success': build_status.CIDBStatusInfo(
            2, constants.BUILDER_STATUS_PASSED)
    }
    self._Mock_GetSlaveStatusesFromCIDB(cidb_status)

    buildbucket_info_dict = {
        'failure': BuildbucketInfos.GetFailureBuild(),
        'success': BuildbucketInfos.GetSuccessBuild()}
    self._Mock_GetSlaveStatusesFromBuildbucket(buildbucket_info_dict)

    slave_status = self._GetSlaveStatus(
        start_time=datetime.datetime.now() - datetime.timedelta(hours=1),
        builders_array=['failure', 'success'],
        config=self.master_canary_config)

    self.assertFalse(slave_status.ShouldWait())

  def testShouldWaitNoScheduledBuilderWithBuildbucket_2(self):
    """Test ShouldWait on cq-master without scheduled builds."""
    cidb_status = {
        'failure': build_status.CIDBStatusInfo(
            1, constants.BUILDER_STATUS_FAILED),
        'success':  build_status.CIDBStatusInfo(
            2, constants.BUILDER_STATUS_PASSED)
    }
    self._Mock_GetSlaveStatusesFromCIDB(cidb_status)

    buildbucket_info_dict = {
        'failure': BuildbucketInfos.GetFailureBuild(),
        'success': BuildbucketInfos.GetSuccessBuild()}
    self._Mock_GetSlaveStatusesFromBuildbucket(buildbucket_info_dict)

    self._Mock_GetRetriableBuilds(set())
    slave_status = self._GetSlaveStatus(
        start_time=datetime.datetime.now() - datetime.timedelta(hours=1),
        builders_array=['failure', 'success'],
        config=self.master_cq_config)
    self.assertFalse(slave_status.ShouldWait())

    self._Mock_GetRetriableBuilds(set(['failure']))
    self._Mock_RetryBuilds()
    slave_status = self._GetSlaveStatus(
        start_time=datetime.datetime.now() - datetime.timedelta(hours=1),
        builders_array=['failure', 'success'],
        config=self.master_cq_config)
    self.assertTrue(slave_status.ShouldWait())

  def testShouldWaitMissingBuilderWithBuildbucket(self):
    """Test ShouldWait says yes waiting because one build status is missing."""
    cidb_status = {
        'failure': build_status.CIDBStatusInfo(
            1, constants.BUILDER_STATUS_FAILED)
    }
    self._Mock_GetSlaveStatusesFromCIDB(cidb_status)

    buildbucket_info_dict = {
        'failure': BuildbucketInfos.GetFailureBuild(),
        'missing': BuildbucketInfos.GetMissingBuild()}
    self._Mock_GetSlaveStatusesFromBuildbucket(buildbucket_info_dict)

    slave_status = self._GetSlaveStatus(
        start_time=datetime.datetime.now() - datetime.timedelta(hours=1),
        builders_array=['failure', 'missing'],
        config=self.master_canary_config)

    self.assertTrue(slave_status.ShouldWait())

  def testShouldWaitBuildersStillBuilding(self):
    """Tests that ShouldWait says to wait because builders still building."""
    cidb_status = {
        'build1': build_status.CIDBStatusInfo(
            1, constants.BUILDER_STATUS_INFLIGHT),
        'build2': build_status.CIDBStatusInfo(
            2, constants.BUILDER_STATUS_FAILED)
    }
    self._Mock_GetSlaveStatusesFromCIDB(cidb_status)

    slave_status = self._GetSlaveStatus(
        builders_array=['build1', 'build2'])

    self.assertTrue(slave_status.ShouldWait())

  def testShouldWaitBuildersStillBuildingWithBuildbucket(self):
    """ShouldWait says yes because builders still in started status."""
    cidb_status = {
        'started': build_status.CIDBStatusInfo(
            1, constants.BUILDER_STATUS_INFLIGHT),
        'failure': build_status.CIDBStatusInfo(
            2, constants.BUILDER_STATUS_FAILED)
    }
    self._Mock_GetSlaveStatusesFromCIDB(cidb_status)

    buildbucket_info_dict = {
        'started': BuildbucketInfos.GetStartedBuild(),
        'failure': BuildbucketInfos.GetFailureBuild()
    }
    self._Mock_GetSlaveStatusesFromBuildbucket(buildbucket_info_dict)

    slave_status = self._GetSlaveStatus(
        builders_array=['started', 'failure'],
        config=self.master_canary_config)

    self.assertTrue(slave_status.ShouldWait())
    self.assertEqual(slave_status.builds_to_retry, set())

  def testShouldWaitBuildersStillBuildingWithBuildbucket_2(self):
    """ShouldWait says yes because builders still in started status."""
    cidb_status = {
        'started': build_status.CIDBStatusInfo(
            1, constants.BUILDER_STATUS_INFLIGHT),
        'failure':  build_status.CIDBStatusInfo(
            2, constants.BUILDER_STATUS_FAILED)
    }
    self._Mock_GetSlaveStatusesFromCIDB(cidb_status)

    buildbucket_info_dict = {
        'started': BuildbucketInfos.GetStartedBuild(),
        'failure': BuildbucketInfos.GetFailureBuild()
    }
    self._Mock_GetSlaveStatusesFromBuildbucket(buildbucket_info_dict)

    self._Mock_GetRetriableBuilds(set(['failure']))
    self._Mock_RetryBuilds()
    slave_status = self._GetSlaveStatus(
        builders_array=['started', 'failure'],
        config=self.master_canary_config)

    self.assertTrue(slave_status.ShouldWait())
    self.assertEqual(slave_status.builds_to_retry, set(['failure']))

  def testShouldWaitSetsBuildsToRetry(self):
    """ShouldWait should update slave_status.builds_to_retry."""
    self.PatchObject(build_status.SlaveStatus,
                     '_Completed',
                     return_value=False)
    self.PatchObject(build_status.SlaveStatus,
                     '_ShouldFailForBuilderStartTimeout',
                     return_value=False)
    slave_status = self._GetSlaveStatus(
        builders_array=['slave1', 'slave2'],
        config=self.master_cq_config)
    slave_status.builds_to_retry = set(['slave1', 'slave2'])

    self.PatchObject(build_status.SlaveStatus, '_RetryBuilds',
                     return_value=set(['slave1']))
    slave_status.ShouldWait()
    self.assertEqual(slave_status.builds_to_retry, set(['slave2']))

    self.PatchObject(build_status.SlaveStatus, '_RetryBuilds',
                     return_value=set(['slave2']))
    slave_status.ShouldWait()
    self.assertEqual(slave_status.builds_to_retry, set([]))

  def testShouldWaitWithTriageRelevantChangesShouldWaitFalse(self):
    """Test ShouldWait with TriageRelevantChanges.ShouldWait is False."""
    relevant_changes.TriageRelevantChanges.__init__ = mock.Mock(
        return_value=None)
    self.PatchObject(relevant_changes.TriageRelevantChanges,
                     'ShouldWait', return_value=False)
    self.PatchObject(build_status.SlaveStatus,
                     '_Completed',
                     return_value=False)
    self.PatchObject(build_status.SlaveStatus,
                     '_ShouldFailForBuilderStartTimeout',
                     return_value=False)
    pool = validation_pool_unittest.MakePool(applied=[])
    slave_status = self._GetSlaveStatus(
        builders_array=['slave1', 'slave2'],
        config=self.master_cq_config,
        version='9289.0.0-rc2',
        pool=pool)

    self.assertFalse(slave_status.ShouldWait())
    self.assertTrue(slave_status.metadata.GetValueWithDefault(
        constants.SELF_DESTRUCTED_BUILD, False))

  def testShouldWaitWithTriageRelevantChangesShouldWaitTrue(self):
    """Test ShouldWait with TriageRelevantChanges.ShouldWait is True."""
    relevant_changes.TriageRelevantChanges.__init__ = mock.Mock(
        return_value=None)
    self.PatchObject(relevant_changes.TriageRelevantChanges,
                     'ShouldWait', return_value=True)
    self.PatchObject(build_status.SlaveStatus,
                     '_Completed',
                     return_value=False)
    self.PatchObject(build_status.SlaveStatus,
                     '_ShouldFailForBuilderStartTimeout',
                     return_value=False)
    pool = validation_pool_unittest.MakePool(applied=[])
    slave_status = self._GetSlaveStatus(
        builders_array=['slave1', 'slave2'],
        config=self.master_cq_config,
        version='9289.0.0-rc2',
        pool=pool)

    self.assertTrue(slave_status.ShouldWait())
    self.assertFalse(slave_status.metadata.GetValueWithDefault(
        constants.SELF_DESTRUCTED_BUILD, False))

  def testRetryBuilds(self):
    """Test RetryBuilds."""
    self._Mock_GetSlaveStatusesFromCIDB()
    self.PatchObject(build_status.SlaveStatus, 'UpdateSlaveStatus')
    metadata = metadata_lib.CBuildbotMetadata()
    slaves = [('failure', 'id_1', time.time()),
              ('canceled', 'id_2', time.time())]
    metadata.ExtendKeyListWithList(
        constants.METADATA_SCHEDULED_SLAVES, slaves)

    slave_status = self._GetSlaveStatus(
        builders_array=['failure', 'canceled'],
        config=self.master_cq_config,
        metadata=metadata,
        buildbucket_client=self.buildbucket_client)

    builds_to_retry = set(['failure', 'canceled'])
    updated_buildbucket_info_dict = {
        'failure': BuildbucketInfos.GetFailureBuild(),
        'canceled': BuildbucketInfos.GetCanceledBuild(),
    }
    slave_status.buildbucket_info_dict = updated_buildbucket_info_dict
    content = {
        'build':{
            'status': 'SCHEDULED',
            'id': 'retry_id',
            'retry_of': 'id',
            'created_ts': time.time()
        }
    }
    slave_status.buildbucket_client.RetryBuildRequest.return_value = content

    retried_builds = slave_status._RetryBuilds(builds_to_retry)
    self.assertEqual(retried_builds, builds_to_retry)
    buildbucket_info_dict = buildbucket_lib.GetBuildInfoDict(
        slave_status.metadata)
    self.assertEqual(buildbucket_info_dict['failure'].buildbucket_id,
                     'retry_id')
    self.assertEqual(buildbucket_info_dict['failure'].retry, 1)

    retried_builds = slave_status._RetryBuilds(builds_to_retry)
    self.assertEqual(retried_builds, builds_to_retry)
    buildbucket_info_dict = buildbucket_lib.GetBuildInfoDict(
        slave_status.metadata)
    self.assertEqual(buildbucket_info_dict['canceled'].buildbucket_id,
                     'retry_id')
    self.assertEqual(buildbucket_info_dict['canceled'].retry, 2)

  def testGetAllSlaveBuildbucketInfo(self):
    """Test GetAllSlaveBuildbucketInfo."""
    self._Mock_GetSlaveStatusesFromCIDB()
    self.PatchObject(build_status.SlaveStatus, 'UpdateSlaveStatus')

    # Test completed builds.
    buildbucket_info_dict = {
        'build1': buildbucket_lib.BuildbucketInfo('id_1', 1, 0, None, None),
        'build2': buildbucket_lib.BuildbucketInfo('id_2', 1, 0, None, None)
    }
    self.PatchObject(buildbucket_lib, 'GetScheduledBuildDict',
                     return_value=buildbucket_info_dict)

    expected_status = 'COMPLETED'
    expected_result = 'SUCCESS'
    content = {
        'build': {
            'status': expected_status,
            'result': expected_result
        }
    }

    slave_status = self._GetSlaveStatus(
        builders_array=['build1', 'build2'],
        config=self.master_cq_config)
    slave_status.buildbucket_client.GetBuildRequest.return_value = content
    updated_buildbucket_info_dict = (
        slave_status.GetAllSlaveBuildbucketInfo(
            self.buildbucket_client, buildbucket_info_dict))

    self.assertEqual(updated_buildbucket_info_dict['build1'].status,
                     expected_status)
    self.assertEqual(updated_buildbucket_info_dict['build1'].result,
                     expected_result)
    self.assertEqual(updated_buildbucket_info_dict['build2'].status,
                     expected_status)
    self.assertEqual(updated_buildbucket_info_dict['build2'].result,
                     expected_result)

    # Test started builds.
    expected_status = 'STARTED'
    expected_result = None
    content = {
        'build': {
            'status': 'STARTED'
        }
    }
    slave_status.buildbucket_client.GetBuildRequest.return_value = content
    updated_buildbucket_info_dict = (
        slave_status.GetAllSlaveBuildbucketInfo(
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
    slave_status.buildbucket_client.GetBuildRequest.side_effect = (
        buildbucket_lib.BuildbucketResponseException)
    updated_buildbucket_info_dict = (
        slave_status.GetAllSlaveBuildbucketInfo(
            self.buildbucket_client, buildbucket_info_dict))
    self.assertIsNone(updated_buildbucket_info_dict['build1'].status)
    self.assertIsNone(updated_buildbucket_info_dict['build2'].status)

  def test_GetNewlyCompletedSlaveBuildbucketInfo(self):
    """Test GetNewlyCompletedSlaveBuildbucketInfo."""
    all_buildbucket_info_dict = self._GetCompletedBuildInfoDict()
    completed = {'completed_success'}

    slave_status = self._GetSlaveStatus(
        builders_array=[self._GetCompletedAllSet()],
        config=self.master_cq_config)

    new_buildbucket_info = slave_status._GetNewlyCompletedSlaveBuildbucketInfo(
        all_buildbucket_info_dict, completed)

    self.assertItemsEqual(new_buildbucket_info.keys(),
                          ['completed_canceled', 'completed_failure'])

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
    self.PatchObject(build_status.SlaveStatus, 'UpdateSlaveStatus')
    _, slave1_id, slave2_id = self._InsertMasterSlaveBuildsToCIDB()
    slave_status = self._GetSlaveStatus(builders_array=['slave1', 'slave2'])

    expected_status = {
        'slave1': build_status.CIDBStatusInfo(slave1_id, 'fail'),
        'slave2': build_status.CIDBStatusInfo(slave2_id, 'fail')
    }

    cidb_status = slave_status.GetAllSlaveCIDBStatusInfo(
        self.db, self.master_build_id, None)
    self.assertDictEqual(cidb_status, expected_status)

    slave_status.completed_builds.update(['slave1'])
    cidb_status = slave_status.GetAllSlaveCIDBStatusInfo(
        self.db, self.master_build_id, None)
    self.assertDictEqual(cidb_status, expected_status)

  def testGetAllSlaveCIDBStatusInfoWithBuildbucket(self):
    """GetAllSlaveCIDBStatusInfo with Buildbucket info."""
    self.PatchObject(build_status.SlaveStatus, 'UpdateSlaveStatus')
    _, slave1_id, slave2_id = self._InsertMasterSlaveBuildsToCIDB()
    slave_status = self._GetSlaveStatus(
        builders_array=['slave1', 'slave2'],
        config=self.master_cq_config)

    buildbucket_info_dict = {
        'slave1': BuildbucketInfos.GetStartedBuild(bb_id='id_1'),
        'slave2': BuildbucketInfos.GetStartedBuild(bb_id='id_2')
    }

    expected_status = {
        'slave1': build_status.CIDBStatusInfo(slave1_id, 'fail'),
        'slave2': build_status.CIDBStatusInfo(slave2_id, 'fail')
    }

    cidb_status = slave_status.GetAllSlaveCIDBStatusInfo(
        self.db, self.master_build_id, buildbucket_info_dict)
    self.assertDictEqual(cidb_status, expected_status)

    slave_status.completed_builds.update(['slave1'])
    cidb_status = slave_status.GetAllSlaveCIDBStatusInfo(
        self.db, self.master_build_id, buildbucket_info_dict)
    self.assertDictEqual(cidb_status, expected_status)

  def testGetAllSlaveCIDBStatusInfoWithRetriedBuilds(self):
    """GetAllSlaveCIDBStatusInfo doesn't return retried builds."""
    self.PatchObject(build_status.SlaveStatus, 'UpdateSlaveStatus')
    self._InsertMasterSlaveBuildsToCIDB()
    self.db.InsertBuild('slave1', constants.WATERFALL_INTERNAL, 3,
                        'slave1', 'host1', master_build_id=0,
                        buildbucket_id='id_3', status='inflight')

    slave_status = self._GetSlaveStatus(
        builders_array=['slave1', 'slave2'],
        config=self.master_cq_config)

    buildbucket_info_dict = {
        'slave1': BuildbucketInfos.GetStartedBuild(bb_id='id_3'),
        'slave2': BuildbucketInfos.GetStartedBuild(bb_id='id_4')
    }

    cidb_status = slave_status.GetAllSlaveCIDBStatusInfo(
        self.db, self.master_build_id, buildbucket_info_dict)
    self.assertEqual(set(cidb_status.keys()), set(['slave1']))
    self.assertEqual(cidb_status['slave1'].status, 'inflight')

  def test_GetNewSlaveCIDBStatusInfoWithCompletedBuilds(self):
    """test _GetNewSlaveCIDBStatusInfo with completed_builds."""
    all_cidb_status_dict = self._GetFullCIDBStatusInfo()
    slave_status = self._GetSlaveStatus(
        builders_array=self._GetFullBuildConfigs(),
        config=self.master_cq_config)
    cidb_status_dict = slave_status._GetNewSlaveCIDBStatusInfo(
        all_cidb_status_dict, set(['completed_success']))

    self.assertItemsEqual(
        cidb_status_dict.keys(),
        ['started', 'completed_failure', 'completed_canceled'])

  def test_GetNewSlaveCIDBStatusInfoWithEmptyCompletedBuilds(self):
    """Test _GetNewSlaveCIDBStatusInfo with empty completed_builds."""
    all_cidb_status_dict = self._GetFullCIDBStatusInfo()
    slave_status = self._GetSlaveStatus(
        builders_array=self._GetFullBuildConfigs(),
        config=self.master_cq_config)
    cidb_status_dict = slave_status._GetNewSlaveCIDBStatusInfo(
        all_cidb_status_dict, set())

    self.assertDictEqual(cidb_status_dict, all_cidb_status_dict)
