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
from chromite.lib import constants
from chromite.lib import config_lib
from chromite.lib import cros_test_lib
from chromite.lib import fake_cidb
from chromite.lib import metadata_lib

# pylint: disable=protected-access

class BuildbucketInfos(object):
  """Helper methods to build BuildbucketInfo."""

  @staticmethod
  def GetScheduledBuild(bb_id='scheduled_id_1', retry=0):
    return build_status.BuildbucketInfo(
        buildbucket_id=bb_id,
        retry=retry,
        status=constants.BUILDBUCKET_BUILDER_STATUS_SCHEDULED,
        result=None
    )

  @staticmethod
  def GetStartedBuild(bb_id='started_id_1', retry=0):
    return build_status.BuildbucketInfo(
        buildbucket_id=bb_id,
        retry=retry,
        status=constants.BUILDBUCKET_BUILDER_STATUS_STARTED,
        result=None
    )

  @staticmethod
  def GetSuccessBuild(bb_id='success_id_1', retry=0):
    return build_status.BuildbucketInfo(
        buildbucket_id=bb_id,
        retry=retry,
        status=constants.BUILDBUCKET_BUILDER_STATUS_COMPLETED,
        result=constants.BUILDBUCKET_BUILDER_RESULT_SUCCESS
    )

  @staticmethod
  def GetFailureBuild(bb_id='failure_id_1', retry=0):
    return build_status.BuildbucketInfo(
        buildbucket_id=bb_id,
        retry=retry,
        status=constants.BUILDBUCKET_BUILDER_STATUS_COMPLETED,
        result=constants.BUILDBUCKET_BUILDER_RESULT_FAILURE
    )

  @staticmethod
  def GetCanceledBuild(bb_id='canceled_id_1', retry=0):
    return build_status.BuildbucketInfo(
        buildbucket_id=bb_id,
        retry=retry,
        status=constants.BUILDBUCKET_BUILDER_STATUS_COMPLETED,
        result=constants.BUILDBUCKET_BUILDER_RESULT_CANCELED
    )

  @staticmethod
  def GetMissingBuild(bb_id='missing_id_1', retry=0):
    return build_status.BuildbucketInfo(
        buildbucket_id=bb_id,
        retry=retry,
        status=None,
        result=None
    )

class SlaveStatusTest(cros_test_lib.MockTestCase):
  """Test methods testing methods in SalveStatus class."""

  def setUp(self):
    self.time_now = datetime.datetime.now()
    self.master_build_id = 0
    self.master_test_config = config_lib.BuildConfig(
        name='master-test', master=True)
    self.master_cq_config = config_lib.BuildConfig(
        name='master-paladin', master=True)
    self.metadata = metadata_lib.CBuildbotMetadata()
    self.db = fake_cidb.FakeCIDBConnection()
    self.buildbucket_client = mock.Mock()

  def _GetSlaveStatus(self, start_time=None, builders_array=None,
                      master_build_id=None, db=None, config=None,
                      metadata=None, buildbucket_client=None,
                      dry_run=True):
    if start_time is None:
      start_time = self.time_now
    if builders_array is None:
      builders_array = []
    if master_build_id is None:
      master_build_id = self.master_build_id
    if db is None:
      db = self.db

    return build_status.SlaveStatus(
        start_time, builders_array, master_build_id, db,
        config=config,
        metadata=metadata,
        buildbucket_client=buildbucket_client,
        dry_run=dry_run)

  def _Mock_GetSlaveStatusesFromCIDB(self, cidb_status=None):
    return self.PatchObject(build_status.SlaveStatus,
                            '_GetSlaveStatusesFromCIDB',
                            return_value=cidb_status)

  def _Mock_GetSlaveStatusesFromBuildbucket(self, build_info_dict):
    return self.PatchObject(build_status.SlaveStatus,
                            '_GetSlaveStatusesFromBuildbucket',
                            return_value=build_info_dict)

  def testGetMissingBuilds(self):
    """Tests GetMissingBuilds returns the missing builders."""
    cidb_status = {'build1': constants.BUILDER_STATUS_FAILED,
                   'build2': constants.BUILDER_STATUS_INFLIGHT}
    self._Mock_GetSlaveStatusesFromCIDB(cidb_status)

    slave_status = self._GetSlaveStatus(
        builders_array=['build1', 'build2', 'missing_builder'])

    self.assertEqual(slave_status.GetMissingBuilds(), set(['missing_builder']))

  def testGetMissingBuildsWithBuildbucket(self):
    """Tests GetMissingBuilds returns the missing builders with Buildbucket."""
    cidb_status = {'started':  constants.BUILDER_STATUS_INFLIGHT}
    self._Mock_GetSlaveStatusesFromCIDB(cidb_status)

    build_info_dict = {
        'scheduled': BuildbucketInfos.GetScheduledBuild(),
        'started': BuildbucketInfos.GetStartedBuild(),
        'missing': BuildbucketInfos.GetMissingBuild()}
    self._Mock_GetSlaveStatusesFromBuildbucket(build_info_dict)

    slave_status = self._GetSlaveStatus(
        builders_array=['scheduled', 'started', 'missing'],
        config=self.master_cq_config,
        metadata=self.metadata,
        buildbucket_client=self.buildbucket_client)

    self.assertEqual(slave_status.GetMissingBuilds(), set(['missing']))

  def testGetMissingBuildsNone(self):
    """Tests GetMissingBuilds returns None."""
    cidb_status = {'build1': constants.BUILDER_STATUS_FAILED,
                   'build2': constants.BUILDER_STATUS_INFLIGHT}
    self._Mock_GetSlaveStatusesFromCIDB(cidb_status)

    slave_status = self._GetSlaveStatus(
        builders_array=['build1', 'build2'])

    self.assertEqual(slave_status.GetMissingBuilds(), set())

  def testGetMissingBuildsNoneWithBuildbucket(self):
    """Tests GetMissing returns None with Buildbucket."""
    cidb_status = {'started': constants.BUILDER_STATUS_INFLIGHT}
    self._Mock_GetSlaveStatusesFromCIDB(cidb_status)

    build_info_dict = {
        'scheduled': BuildbucketInfos.GetScheduledBuild(),
        'started': BuildbucketInfos.GetStartedBuild()
    }
    self._Mock_GetSlaveStatusesFromBuildbucket(build_info_dict)

    slave_status = self._GetSlaveStatus(
        builders_array=['build1', 'build2'],
        config=self.master_cq_config,
        metadata=self.metadata,
        buildbucket_client=self.buildbucket_client)

    self.assertEqual(slave_status.GetMissingBuilds(), set())

  def testGetCompletedBuilds(self):
    """Tests GetCompletedBuilds returns the completed builds."""
    cidb_status = {'passed': constants.BUILDER_STATUS_PASSED,
                   'failed': constants.BUILDER_STATUS_FAILED,
                   'aborted': constants.BUILDER_STATUS_ABORTED,
                   'skipped': constants.BUILDER_STATUS_SKIPPED,
                   'forgiven': constants.BUILDER_STATUS_FORGIVEN,
                   'inflight': constants.BUILDER_STATUS_INFLIGHT,
                   'missing': constants.BUILDER_STATUS_MISSING,
                   'planned': constants.BUILDER_STATUS_PLANNED}
    self._Mock_GetSlaveStatusesFromCIDB(cidb_status)

    slave_status = self._GetSlaveStatus(
        builders_array=['passed', 'failed', 'aborted', 'skipped', 'forgiven',
                        'inflight', 'missing', 'planning'])

    self.assertEqual(slave_status.GetCompletedBuilds(),
                     set(['passed', 'failed', 'aborted', 'skipped',
                          'forgiven']))

  def testGetRetriableBuildsReturnsNone(self):
    """GetRetriableBuilds returns no build to retry."""
    cidb_status = {'started': constants.BUILDER_STATUS_INFLIGHT,
                   'completed_success': constants.BUILDER_STATUS_PASSED,
                   'completed_failure': constants.BUILDER_STATUS_FAILED,
                   'completed_canceled': constants.BUILDER_STATUS_INFLIGHT}
    self._Mock_GetSlaveStatusesFromCIDB(cidb_status)

    build_info_dict = {
        'scheduled': BuildbucketInfos.GetScheduledBuild(),
        'started': BuildbucketInfos.GetStartedBuild(),
        'completed_success': BuildbucketInfos.GetSuccessBuild(),
        'completed_failure': BuildbucketInfos.GetFailureBuild(),
        'completed_canceled': BuildbucketInfos.GetCanceledBuild()}
    self._Mock_GetSlaveStatusesFromBuildbucket(build_info_dict)

    completed_all = set(['completed_success',
                         'completed_failure',
                         'completed_canceled'])

    slave_status = self._GetSlaveStatus(
        builders_array=['scheduled', 'started', 'completed_success',
                        'completed_failure', 'completed_canceled'],
        config=self.master_cq_config,
        metadata=self.metadata,
        buildbucket_client=self.buildbucket_client,
        dry_run=True)

    self.assertEqual(slave_status.GetRetriableBuilds(completed_all), set())

  def testGetRetriableBuilds(self):
    """Retry failed and canceled builds."""
    cidb_status = {'started': constants.BUILDER_STATUS_INFLIGHT,
                   'completed_success': constants.BUILDER_STATUS_PASSED}
    self._Mock_GetSlaveStatusesFromCIDB(cidb_status)

    build_info_dict = {
        'scheduled': BuildbucketInfos.GetScheduledBuild(),
        'started': BuildbucketInfos.GetStartedBuild(),
        'completed_success': BuildbucketInfos.GetSuccessBuild(),
        'completed_failure': BuildbucketInfos.GetFailureBuild(),
        'completed_canceled': BuildbucketInfos.GetCanceledBuild()}
    self._Mock_GetSlaveStatusesFromBuildbucket(build_info_dict)

    completed_all = set(['completed_success',
                         'completed_failure',
                         'completed_canceled'])

    slave_status = self._GetSlaveStatus(
        builders_array=['scheduled', 'started', 'completed_success',
                        'completed_failure', 'completed_canceled'],
        config=self.master_cq_config,
        metadata=self.metadata,
        buildbucket_client=self.buildbucket_client,
        dry_run=True)
    retry_builds = slave_status.GetRetriableBuilds(completed_all)

    self.assertEqual(retry_builds,
                     set(['completed_failure', 'completed_canceled']))

  def testGetRetriableBuildsExceedsLimit(self):
    """Do not return builds which have exceeded the retry_limit."""
    cidb_status = {'started': constants.BUILDER_STATUS_INFLIGHT,
                   'completed_success': constants.BUILDER_STATUS_PASSED}
    self._Mock_GetSlaveStatusesFromCIDB(cidb_status)

    build_info_dict = {
        'scheduled': BuildbucketInfos.GetScheduledBuild(),
        'started': BuildbucketInfos.GetStartedBuild(),
        'completed_success': BuildbucketInfos.GetSuccessBuild(),
        'completed_failure': BuildbucketInfos.GetFailureBuild(
            retry=constants.BUILDBUCKET_BUILD_RETRY_LIMIT),
        'completed_canceled': BuildbucketInfos.GetCanceledBuild()}
    self._Mock_GetSlaveStatusesFromBuildbucket(build_info_dict)

    completed_all = set(['completed_success',
                         'completed_failure',
                         'completed_canceled'])

    slave_status = self._GetSlaveStatus(
        builders_array=['scheduled', 'started', 'completed_success',
                        'completed_failure', 'completed_canceled'],
        config=self.master_cq_config,
        metadata=self.metadata,
        buildbucket_client=self.buildbucket_client)
    retry_builds = slave_status.GetRetriableBuilds(completed_all)

    self.assertEqual(retry_builds, set(['completed_canceled']))

  def testGetCompletedBuildsWithBuildbucket(self):
    """Tests GetCompletedBuilds with Buildbucket"""
    cidb_status = {'started': constants.BUILDER_STATUS_INFLIGHT,
                   'completed_success': constants.BUILDER_STATUS_PASSED,
                   'completed_failure': constants.BUILDER_STATUS_FAILED,
                   'completed_canceled': constants.BUILDER_STATUS_INFLIGHT}
    self._Mock_GetSlaveStatusesFromCIDB(cidb_status)

    build_info_dict = {
        'scheduled': BuildbucketInfos.GetScheduledBuild(),
        'started': BuildbucketInfos.GetStartedBuild(),
        'completed_success': BuildbucketInfos.GetSuccessBuild(),
        'completed_failure': BuildbucketInfos.GetFailureBuild(),
        'completed_canceled': BuildbucketInfos.GetCanceledBuild()}
    self._Mock_GetSlaveStatusesFromBuildbucket(build_info_dict)

    slave_status = self._GetSlaveStatus(
        builders_array=['scheduled', 'started', 'completed_success',
                        'completed_failure', 'completed_canceled'],
        config=self.master_cq_config,
        metadata=self.metadata,
        buildbucket_client=self.buildbucket_client)

    self.assertEqual(slave_status.GetCompletedBuilds(),
                     set(['completed_success', 'completed_failure',
                          'completed_canceled']))

  def testGetBuildsToRetry(self):
    """Test GetBuildsToRetry."""
    cidb_status = {'started': constants.BUILDER_STATUS_INFLIGHT,
                   'completed_success': constants.BUILDER_STATUS_PASSED,
                   'completed_canceled': constants.BUILDER_STATUS_INFLIGHT}
    self._Mock_GetSlaveStatusesFromCIDB(cidb_status)

    slave_status = self._GetSlaveStatus(
        builders_array=['scheduled', 'started', 'completed_success',
                        'completed_failure', 'completed_canceled'])
    self.assertEqual(slave_status.GetBuildsToRetry(), None)

  def testGetBuildsToRetryWithBuildbucket(self):
    """Test GetBuildsToRetry with Buildbucket."""
    cidb_status = {'started': constants.BUILDER_STATUS_INFLIGHT,
                   'completed_success': constants.BUILDER_STATUS_PASSED,
                   'completed_canceled': constants.BUILDER_STATUS_INFLIGHT}
    self._Mock_GetSlaveStatusesFromCIDB(cidb_status)

    build_info_dict = {
        'scheduled': BuildbucketInfos.GetScheduledBuild(),
        'started': BuildbucketInfos.GetStartedBuild(),
        'completed_success': BuildbucketInfos.GetSuccessBuild(),
        'completed_failure': BuildbucketInfos.GetFailureBuild(),
        'completed_canceled': BuildbucketInfos.GetCanceledBuild()}
    self._Mock_GetSlaveStatusesFromBuildbucket(build_info_dict)

    slave_status = self._GetSlaveStatus(
        builders_array=['scheduled', 'started', 'completed_success',
                        'completed_failure', 'completed_canceled'],
        config=self.master_cq_config,
        metadata=self.metadata,
        buildbucket_client=self.buildbucket_client,
        dry_run=True)

    self.assertEqual(slave_status.GetBuildsToRetry(),
                     set(['completed_failure']))

  def testCompleted(self):
    """Tests Completed returns proper bool."""
    builders_array = ['build1', 'build2']
    statusNotCompleted = {'build1': constants.BUILDER_STATUS_FAILED,
                          'build2': constants.BUILDER_STATUS_INFLIGHT}
    self._Mock_GetSlaveStatusesFromCIDB(statusNotCompleted)
    slaveStatusNotCompleted = self._GetSlaveStatus(
        builders_array=builders_array)
    self.assertFalse(slaveStatusNotCompleted.Completed())

    statusCompleted = {'build1': constants.BUILDER_STATUS_FAILED,
                       'build2': constants.BUILDER_STATUS_PASSED}
    self._Mock_GetSlaveStatusesFromCIDB(statusCompleted)
    slaveStatusCompleted = self._GetSlaveStatus(
        builders_array=builders_array)
    self.assertTrue(slaveStatusCompleted.Completed())

  def testCompletedWithBuildbucket(self):
    """Tests Completed returns proper bool with Buildbucket."""
    builders_array = ['started', 'failure', 'missing']
    status_not_completed = {
        'started': constants.BUILDER_STATUS_INFLIGHT,
        'failure': constants.BUILDER_STATUS_FAILED}
    self._Mock_GetSlaveStatusesFromCIDB(status_not_completed)

    build_info_dict_not_completed = {
        'started': BuildbucketInfos.GetStartedBuild(),
        'failure': BuildbucketInfos.GetFailureBuild(),
        'missing': BuildbucketInfos.GetMissingBuild()}
    self._Mock_GetSlaveStatusesFromBuildbucket(build_info_dict_not_completed)

    slaveStatusNotCompleted = self._GetSlaveStatus(
        builders_array=builders_array,
        config=self.master_cq_config,
        metadata=self.metadata,
        buildbucket_client=self.buildbucket_client)

    self.assertFalse(slaveStatusNotCompleted.Completed())

    status_completed = {
        'success': constants.BUILDER_STATUS_PASSED,
        'failure': constants.BUILDER_STATUS_FAILED}
    self._Mock_GetSlaveStatusesFromCIDB(status_completed)

    build_info_dict_complted = {
        'success': BuildbucketInfos.GetSuccessBuild(),
        'failure': BuildbucketInfos.GetFailureBuild()}
    self._Mock_GetSlaveStatusesFromBuildbucket(build_info_dict_complted)

    builders_array = ['success', 'failure']
    slaveStatusCompleted = self._GetSlaveStatus(
        builders_array=builders_array,
        config=self.master_cq_config,
        metadata=self.metadata,
        buildbucket_client=self.buildbucket_client)

    self.assertTrue(slaveStatusCompleted.Completed())

  def testShouldFailForBuilderStartTimeoutTrue(self):
    """Tests that ShouldFailForBuilderStartTimeout says fail when it should."""
    cidb_status = {'build1': constants.BUILDER_STATUS_FAILED}
    self._Mock_GetSlaveStatusesFromCIDB(cidb_status)

    start_time = datetime.datetime.now()
    slave_status = self._GetSlaveStatus(
        builders_array=['build1', 'build2'])
    check_time = start_time + datetime.timedelta(
        minutes=slave_status.BUILD_START_TIMEOUT_MIN + 1)

    self.assertTrue(slave_status.ShouldFailForBuilderStartTimeout(check_time))

  def testShouldFailForBuilderStartTimeoutTrueWithBuildbucket(self):
    """Tests that ShouldFailForBuilderStartTimeout says fail when it should."""
    cidb_status = {'success': constants.BUILDER_STATUS_PASSED}
    self._Mock_GetSlaveStatusesFromCIDB(cidb_status)

    build_info_dict = {
        'success': BuildbucketInfos.GetSuccessBuild(),
        'scheduled': BuildbucketInfos.GetScheduledBuild()
    }
    self._Mock_GetSlaveStatusesFromBuildbucket(build_info_dict)

    start_time = datetime.datetime.now()
    slave_status = self._GetSlaveStatus(
        start_time=start_time,
        builders_array=['success', 'scheduled'],
        config=self.master_cq_config,
        metadata=self.metadata,
        buildbucket_client=self.buildbucket_client,
        dry_run=True)
    check_time = start_time + datetime.timedelta(
        minutes=slave_status.BUILD_START_TIMEOUT_MIN + 1)

    self.assertTrue(slave_status.ShouldFailForBuilderStartTimeout(check_time))

  def testShouldFailForBuilderStartTimeoutFalseTooEarly(self):
    """Tests that ShouldFailForBuilderStartTimeout doesn't fail.

    Make sure that we don't fail if there are missing builders but we're
    checking before the timeout and the other builders have completed.
    """
    cidb_status = {'build1': constants.BUILDER_STATUS_FAILED}
    self._Mock_GetSlaveStatusesFromCIDB(cidb_status)

    start_time = datetime.datetime.now()
    slave_status = self._GetSlaveStatus(
        start_time=start_time,
        builders_array=['build1', 'build2'])

    self.assertFalse(slave_status.ShouldFailForBuilderStartTimeout(start_time))

  def testShouldFailForBuilderStartTimeoutFalseTooEarlyWithBuildbucket(self):
    """Tests that ShouldFailForBuilderStartTimeout doesn't fail.

    With Buildbucket, make sure that we don't fail if there are missing
    builders but we're checking before the timeout and the other builders
    have completed.
    """
    cidb_status = {'success': constants.BUILDER_STATUS_PASSED}
    self._Mock_GetSlaveStatusesFromCIDB(cidb_status)

    build_info_dict = {
        'success': BuildbucketInfos.GetSuccessBuild(),
        'missing': BuildbucketInfos.GetMissingBuild()}
    self._Mock_GetSlaveStatusesFromBuildbucket(build_info_dict)

    start_time = datetime.datetime.now()
    slave_status = self._GetSlaveStatus(
        builders_array=['success', 'missing'],
        config=self.master_cq_config,
        metadata=self.metadata,
        buildbucket_client=self.buildbucket_client)

    self.assertFalse(slave_status.ShouldFailForBuilderStartTimeout(start_time))

  def testShouldFailForBuilderStartTimeoutFalseNotCompleted(self):
    """Tests that ShouldFailForBuilderStartTimeout doesn't fail.

    Make sure that we don't fail if there are missing builders and we're
    checking after the timeout but the other builders haven't completed.
    """
    cidb_status = {'build1': constants.BUILDER_STATUS_INFLIGHT}
    self._Mock_GetSlaveStatusesFromCIDB(cidb_status)

    start_time = datetime.datetime.now()
    slave_status = self._GetSlaveStatus(
        start_time=start_time,
        builders_array=['build1', 'build2'],
        config=self.master_cq_config,
        metadata=self.metadata,
        buildbucket_client=self.buildbucket_client,
        dry_run=True)
    check_time = start_time + datetime.timedelta(
        minutes=slave_status.BUILD_START_TIMEOUT_MIN + 1)

    self.assertFalse(slave_status.ShouldFailForBuilderStartTimeout(check_time))

  def testShouldFailForStartTimeoutFalseNotCompletedWithBuildbucket(self):
    """Tests that ShouldWait doesn't fail with Buildbucket.

    With Buildbucket, make sure that we don't fail if there are missing builders
    and we're checking after the timeout but the other builders haven't
    completed.
    """
    cidb_status = {'started': constants.BUILDER_STATUS_INFLIGHT}
    self._Mock_GetSlaveStatusesFromCIDB(cidb_status)

    build_info_dict = {
        'started': BuildbucketInfos.GetStartedBuild(),
        'missing': BuildbucketInfos.GetMissingBuild()}
    self._Mock_GetSlaveStatusesFromBuildbucket(build_info_dict)

    start_time = datetime.datetime.now()
    slave_status = self._GetSlaveStatus(
        start_time=start_time,
        builders_array=['started', 'missing'],
        config=self.master_cq_config,
        metadata=self.metadata,
        buildbucket_client=self.buildbucket_client,
        dry_run=True)
    check_time = start_time + datetime.timedelta(
        minutes=slave_status.BUILD_START_TIMEOUT_MIN + 1)

    self.assertFalse(slave_status.ShouldFailForBuilderStartTimeout(check_time))

  def testShouldWaitAllBuildersCompleted(self):
    """Tests that ShouldWait says no waiting because all builders finished."""
    cidb_status = {'build1': constants.BUILDER_STATUS_FAILED,
                   'build2': constants.BUILDER_STATUS_PASSED}
    self._Mock_GetSlaveStatusesFromCIDB(cidb_status)

    slave_status = self._GetSlaveStatus(
        builders_array=['build1', 'build2'])

    self.assertFalse(slave_status.ShouldWait())

  def testShouldWaitAllBuildersCompletedWithBuildbucket(self):
    """ShouldWait says no because all builders finished with Buildbucket."""
    cidb_status = {'failure': constants.BUILDER_STATUS_FAILED,
                   'success': constants.BUILDER_STATUS_PASSED}
    self._Mock_GetSlaveStatusesFromCIDB(cidb_status)

    build_info_dict = {
        'failure': BuildbucketInfos.GetFailureBuild(),
        'success': BuildbucketInfos.GetSuccessBuild()}
    self._Mock_GetSlaveStatusesFromBuildbucket(build_info_dict)

    slave_status = self._GetSlaveStatus(
        builders_array=['failure', 'success'],
        config=self.master_cq_config,
        metadata=self.metadata,
        buildbucket_client=self.buildbucket_client)

    self.assertFalse(slave_status.ShouldWait())

  def testShouldWaitMissingBuilder(self):
    """Tests that ShouldWait says no waiting because a builder is missing."""
    cidb_status = {'build1': constants.BUILDER_STATUS_FAILED}
    self._Mock_GetSlaveStatusesFromCIDB(cidb_status)

    slave_status = self._GetSlaveStatus(
        start_time=datetime.datetime.now() - datetime.timedelta(hours=1),
        builders_array=['build1', 'build2'])

    self.assertFalse(slave_status.ShouldWait())

  def testShouldWaitScheduledBuilderWithBuildbucket(self):
    """ShouldWait says no because a builder is in scheduled with Buildbucket."""
    cidb_status = {'failure': constants.BUILDER_STATUS_FAILED}
    self._Mock_GetSlaveStatusesFromCIDB(cidb_status)

    build_info_dict = {
        'failure': BuildbucketInfos.GetFailureBuild(),
        'scheduled': BuildbucketInfos.GetScheduledBuild()}
    self._Mock_GetSlaveStatusesFromBuildbucket(build_info_dict)

    slave_status = self._GetSlaveStatus(
        start_time=datetime.datetime.now() - datetime.timedelta(hours=1),
        builders_array=['failure', 'scheduled'],
        config=self.master_cq_config,
        metadata=self.metadata,
        buildbucket_client=self.buildbucket_client,)

    self.assertFalse(slave_status.ShouldWait())

  def testShouldWaitNoScheduledBuilderWithBuildbucket(self):
    """ShouldWait says no because all scheduled builds are completed."""
    cidb_status = {'failure': constants.BUILDER_STATUS_FAILED,
                   'success': constants.BUILDER_STATUS_PASSED}
    self._Mock_GetSlaveStatusesFromCIDB(cidb_status)

    build_info_dict = {
        'failure': BuildbucketInfos.GetFailureBuild(),
        'success': BuildbucketInfos.GetSuccessBuild()}
    self._Mock_GetSlaveStatusesFromBuildbucket(build_info_dict)

    slave_status = self._GetSlaveStatus(
        start_time=datetime.datetime.now() - datetime.timedelta(hours=1),
        builders_array=['build1', 'build2'],
        config=self.master_cq_config,
        metadata=self.metadata,
        buildbucket_client=self.buildbucket_client,
        dry_run=True)

    self.assertFalse(slave_status.ShouldWait())

  def testShouldWaitMissingBuilderWithBuildbucket(self):
    """ShouldWait says yes waiting because one build status is missing."""
    cidb_status = {'failure': constants.BUILDER_STATUS_FAILED}
    self._Mock_GetSlaveStatusesFromCIDB(cidb_status)

    build_info_dict = {
        'failure': BuildbucketInfos.GetFailureBuild(),
        'missing': BuildbucketInfos.GetMissingBuild()}
    self._Mock_GetSlaveStatusesFromBuildbucket(build_info_dict)

    slave_status = self._GetSlaveStatus(
        start_time=datetime.datetime.now() - datetime.timedelta(hours=1),
        builders_array=['failure', 'missing'],
        config=self.master_cq_config,
        metadata=self.metadata,
        buildbucket_client=self.buildbucket_client,
        dry_run=True)

    self.assertTrue(slave_status.ShouldWait())

  def testShouldWaitBuildersStillBuilding(self):
    """Tests that ShouldWait says to wait because builders still building."""
    cidb_status = {'build1': constants.BUILDER_STATUS_INFLIGHT,
                   'build2': constants.BUILDER_STATUS_FAILED}
    self._Mock_GetSlaveStatusesFromCIDB(cidb_status)

    slave_status = self._GetSlaveStatus(
        builders_array=['build1', 'build2'])

    self.assertTrue(slave_status.ShouldWait())

  def testShouldWaitBuildersStillBuildingWithBuildbucket(self):
    """ShouldWait says yes because builders still in started status."""
    cidb_status = {'started': constants.BUILDER_STATUS_INFLIGHT,
                   'failure': constants.BUILDER_STATUS_FAILED}
    self._Mock_GetSlaveStatusesFromCIDB(cidb_status)

    build_info_dict = {
        'started': BuildbucketInfos.GetStartedBuild(),
        'failure': BuildbucketInfos.GetFailureBuild()
    }
    self._Mock_GetSlaveStatusesFromBuildbucket(build_info_dict)

    slave_status = self._GetSlaveStatus(
        builders_array=['started', 'failure'],
        config=self.master_cq_config,
        metadata=self.metadata,
        buildbucket_client=self.buildbucket_client,
        dry_run=True)

    self.assertTrue(slave_status.ShouldWait())

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
        buildbucket_client=self.buildbucket_client,
        dry_run=True)

    builds_to_retry = set(['failure', 'canceled'])
    updated_build_info_dict = {
        'failure': BuildbucketInfos.GetFailureBuild(),
        'canceled': BuildbucketInfos.GetCanceledBuild(),
    }
    slave_status.build_info_dict = updated_build_info_dict
    content = {
        'build':{
            'status': 'SCHEDULED',
            'id': 'retry_id',
            'retry_of': 'id',
            'created_ts': time.time()
        }
    }
    slave_status.buildbucket_client.RetryBuildRequest.return_value = content

    slave_status.RetryBuilds(builds_to_retry)
    build_info_dict = buildbucket_lib.GetBuildInfoDict(slave_status.metadata)
    self.assertEqual(build_info_dict['failure']['buildbucket_id'], 'retry_id')
    self.assertEqual(build_info_dict['failure']['retry'], 1)

    slave_status.RetryBuilds(builds_to_retry)
    build_info_dict = buildbucket_lib.GetBuildInfoDict(slave_status.metadata)
    self.assertEqual(build_info_dict['canceled']['buildbucket_id'], 'retry_id')
    self.assertEqual(build_info_dict['canceled']['retry'], 2)

  def test_GetSlaveStatusesFromBuildbucket(self):
    """Test _GetSlaveStatusesFromBuildbucket."""
    self._Mock_GetSlaveStatusesFromCIDB()
    self.PatchObject(build_status.SlaveStatus, 'UpdateSlaveStatus')

    # Test completed builds.
    build_info_dict = {
        'build1': {'buildbucket_id': 'id_1', 'retry': 0},
        'build2': {'buildbucket_id': 'id_2', 'retry': 0}
    }
    self.PatchObject(buildbucket_lib, 'GetScheduledBuildDict',
                     return_value=build_info_dict)

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
        config=self.master_cq_config,
        metadata=self.metadata,
        buildbucket_client=self.buildbucket_client,
        dry_run=True)
    slave_status.buildbucket_client.GetBuildRequest.return_value = content
    updated_build_info_dict = slave_status._GetSlaveStatusesFromBuildbucket()

    self.assertEqual(updated_build_info_dict['build1'].status,
                     expected_status)
    self.assertEqual(updated_build_info_dict['build1'].result,
                     expected_result)
    self.assertEqual(updated_build_info_dict['build2'].status,
                     expected_status)
    self.assertEqual(updated_build_info_dict['build2'].result,
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
    updated_build_info_dict = slave_status._GetSlaveStatusesFromBuildbucket()

    self.assertEqual(updated_build_info_dict['build1'].status,
                     expected_status)
    self.assertEqual(updated_build_info_dict['build1'].result,
                     expected_result)
    self.assertEqual(updated_build_info_dict['build2'].status,
                     expected_status)
    self.assertEqual(updated_build_info_dict['build2'].result,
                     expected_result)

    # Test BuildbucketResponseException failures.
    slave_status.buildbucket_client.GetBuildRequest.side_effect = (
        buildbucket_lib.BuildbucketResponseException)
    updated_build_info_dict = slave_status._GetSlaveStatusesFromBuildbucket()
    self.assertIsNone(updated_build_info_dict['build1'].status)
    self.assertIsNone(updated_build_info_dict['build2'].status)

  def test_GetSlaveStatusesFromCIDB(self):
    """Test test_GetSlaveStatusesFromCIDB"""
    master = 'master-paladin'
    slave1 = 'slave1-paladin'
    slave2 = 'slave2-paladin'
    self.db.InsertBuild(master, constants.WATERFALL_INTERNAL, 1, master,
                        'host1')
    self.db.InsertBuild(slave1, constants.WATERFALL_INTERNAL, 2, slave1,
                        'host1', master_build_id=0)
    self.db.InsertBuild(slave2, constants.WATERFALL_INTERNAL, 3, slave2,
                        'host1', master_build_id=0)

    slave_status = self._GetSlaveStatus(
        builders_array=[slave1, slave2],
        config=self.master_cq_config,
        metadata=self.metadata,
        buildbucket_client=self.buildbucket_client,
        dry_run=True)

    cidb_status = slave_status._GetSlaveStatusesFromCIDB()
    self.assertEqual(set(cidb_status.keys()), set([slave1, slave2]))

    slave_status.completed_builds.update([slave1])
    cidb_status = slave_status._GetSlaveStatusesFromCIDB()
    self.assertEqual(set(cidb_status.keys()), set([slave2]))
