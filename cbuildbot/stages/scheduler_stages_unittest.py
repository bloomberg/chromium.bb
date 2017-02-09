# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for scheduler stages."""

from __future__ import print_function

import mock

from chromite.cbuildbot import buildbucket_lib
from chromite.cbuildbot.stages import generic_stages
from chromite.cbuildbot.stages import generic_stages_unittest
from chromite.cbuildbot.stages import scheduler_stages
from chromite.lib import auth
from chromite.lib import cidb
from chromite.lib import config_lib
from chromite.lib import constants
from chromite.lib import fake_cidb


class ScheduleSalvesStageTest(generic_stages_unittest.AbstractStageTestCase):
  """Unit tests for ScheduleSalvesStage."""

  BOT_ID = 'master-paladin'

  def setUp(self):
    self.PatchObject(buildbucket_lib, 'GetServiceAccount',
                     return_value='server_account')
    self.PatchObject(auth.AuthorizedHttp, '__init__',
                     return_value=None)
    self.PatchObject(buildbucket_lib.BuildbucketClient,
                     '_GetHost',
                     return_value=buildbucket_lib.BUILDBUCKET_TEST_HOST)
    self.PatchObject(buildbucket_lib.BuildbucketClient,
                     'SendBuildbucketRequest',
                     return_value=None)
    # Create and set up a fake cidb instance.
    self.fake_db = fake_cidb.FakeCIDBConnection()
    cidb.CIDBConnectionFactory.SetupMockCidb(self.fake_db)

    self.sync_stage = mock.Mock()
    self._Prepare()

  def ConstructStage(self):
    return scheduler_stages.ScheduleSlavesStage(self._run, self.sync_stage)

  def testPerformStage(self):
    """Test PerformStage."""
    stage = self.ConstructStage()
    self.PatchObject(buildbucket_lib.BuildbucketClient,
                     '_GetHost',
                     return_value=buildbucket_lib.BUILDBUCKET_TEST_HOST)

    stage.PerformStage()

  def testScheduleImportantSlaveBuildsFailure(self):
    """Test ScheduleSlaveBuilds with important slave failures."""
    stage = self.ConstructStage()
    self.PatchObject(scheduler_stages.ScheduleSlavesStage,
                     'PostSlaveBuildToBuildbucket',
                     side_effect=buildbucket_lib.BuildbucketResponseException)

    slave_config_map_1 = {
        'slave_external': config_lib.BuildConfig(
            important=True, active_waterfall=constants.WATERFALL_EXTERNAL)}
    self.PatchObject(generic_stages.BuilderStage, '_GetSlaveConfigMap',
                     return_value=slave_config_map_1)
    self.assertRaises(
        buildbucket_lib.BuildbucketResponseException,
        stage.ScheduleSlaveBuildsViaBuildbucket,
        important_only=False, dryrun=True)

    slave_config_map_2 = {
        'slave_internal': config_lib.BuildConfig(
            important=True, active_waterfall=constants.WATERFALL_INTERNAL)}
    self.PatchObject(generic_stages.BuilderStage, '_GetSlaveConfigMap',
                     return_value=slave_config_map_2)
    self.assertRaises(
        buildbucket_lib.BuildbucketResponseException,
        stage.ScheduleSlaveBuildsViaBuildbucket,
        important_only=False, dryrun=True)

  def testScheduleUnimportantSlaveBuildsFailure(self):
    """Test ScheduleSlaveBuilds with unimportant slave failures."""
    stage = self.ConstructStage()
    self.PatchObject(scheduler_stages.ScheduleSlavesStage,
                     'PostSlaveBuildToBuildbucket',
                     side_effect=buildbucket_lib.BuildbucketResponseException)

    slave_config_map = {
        'slave_external': config_lib.BuildConfig(
            important=False, active_waterfall=constants.WATERFALL_EXTERNAL),
        'slave_internal': config_lib.BuildConfig(
            important=False, active_waterfall=constants.WATERFALL_INTERNAL),}
    self.PatchObject(generic_stages.BuilderStage, '_GetSlaveConfigMap',
                     return_value=slave_config_map)
    stage.ScheduleSlaveBuildsViaBuildbucket(important_only=False, dryrun=True)

    scheduled_slaves = self._run.attrs.metadata.GetValue('scheduled_slaves')
    self.assertEqual(len(scheduled_slaves), 0)
    unscheduled_slaves = self._run.attrs.metadata.GetValue('unscheduled_slaves')
    self.assertEqual(len(unscheduled_slaves), 2)

  def testScheduleSlaveBuildsFailure(self):
    """Test ScheduleSlaveBuilds with mixed slave failures."""
    stage = self.ConstructStage()
    self.PatchObject(scheduler_stages.ScheduleSlavesStage,
                     'PostSlaveBuildToBuildbucket',
                     side_effect=buildbucket_lib.BuildbucketResponseException)

    slave_config_map = {
        'slave_external': config_lib.BuildConfig(
            important=False, active_waterfall=constants.WATERFALL_EXTERNAL),
        'slave_internal': config_lib.BuildConfig(
            important=True, active_waterfall=constants.WATERFALL_INTERNAL)}
    self.PatchObject(generic_stages.BuilderStage, '_GetSlaveConfigMap',
                     return_value=slave_config_map)
    self.assertRaises(
        buildbucket_lib.BuildbucketResponseException,
        stage.ScheduleSlaveBuildsViaBuildbucket,
        important_only=False, dryrun=True)

  def testScheduleSlaveBuildsSuccess(self):
    """Test ScheduleSlaveBuilds with success."""
    stage = self.ConstructStage()

    self.PatchObject(scheduler_stages.ScheduleSlavesStage,
                     'PostSlaveBuildToBuildbucket',
                     return_value=('buildbucket_id', None))

    slave_config_map = {
        'slave_external': config_lib.BuildConfig(
            important=False, active_waterfall=constants.WATERFALL_EXTERNAL),
        'slave_internal': config_lib.BuildConfig(
            important=True, active_waterfall=constants.WATERFALL_INTERNAL)}
    self.PatchObject(generic_stages.BuilderStage, '_GetSlaveConfigMap',
                     return_value=slave_config_map)

    stage.ScheduleSlaveBuildsViaBuildbucket(important_only=False, dryrun=True)

    scheduled_slaves = self._run.attrs.metadata.GetValue('scheduled_slaves')
    self.assertEqual(len(scheduled_slaves), 1)
    unscheduled_slaves = self._run.attrs.metadata.GetValue('unscheduled_slaves')
    self.assertEqual(len(unscheduled_slaves), 0)

  def testNoScheduledSlaveBuilds(self):
    """Test no slave builds are scheduled."""
    stage = self.ConstructStage()
    schedule_mock = self.PatchObject(
        scheduler_stages.ScheduleSlavesStage,
        'ScheduleSlaveBuildsViaBuildbucket')
    self.sync_stage.pool.applied = []
    self.sync_stage.pool.has_chump_cls = False

    stage.PerformStage()
    self.assertFalse(schedule_mock.called)

  def testScheduleSlaveBuildsWithChumpCLs(self):
    """Test no slave builds are scheduled."""
    stage = self.ConstructStage()
    schedule_mock = self.PatchObject(
        scheduler_stages.ScheduleSlavesStage,
        'ScheduleSlaveBuildsViaBuildbucket')
    self.sync_stage.pool.applied = []
    self.sync_stage.pool.has_chump_cls = True

    stage.PerformStage()
    self.assertTrue(schedule_mock.called)
