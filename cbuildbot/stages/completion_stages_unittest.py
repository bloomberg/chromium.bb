# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for completion stages."""

from __future__ import print_function

import mock

from chromite.cbuildbot import commands
from chromite.cbuildbot import manifest_version
from chromite.cbuildbot import prebuilts
from chromite.cbuildbot.stages import completion_stages
from chromite.cbuildbot.stages import generic_stages
from chromite.cbuildbot.stages import generic_stages_unittest
from chromite.cbuildbot.stages import sync_stages_unittest
from chromite.cbuildbot.stages import sync_stages
from chromite.lib import builder_status_lib
from chromite.lib import cidb
from chromite.lib import config_lib
from chromite.lib import constants
from chromite.lib import portage_util
from chromite.lib.buildstore import FakeBuildStore

# pylint: disable=protected-access


class ManifestVersionedSyncCompletionStageTest(
    sync_stages_unittest.ManifestVersionedSyncStageTest):
  """Tests the ManifestVersionedSyncCompletion stage."""

  # pylint: disable=abstract-method

  BOT_ID = 'eve-release'

  def setUp(self):
    self.buildstore = FakeBuildStore()

  def testManifestVersionedSyncCompletedSuccess(self):
    """Tests basic ManifestVersionedSyncStageCompleted on success"""
    board_runattrs = self._run.GetBoardRunAttrs('eve')
    board_runattrs.SetParallel('success', True)
    update_status_mock = self.PatchObject(manifest_version.BuildSpecsManager,
                                          'UpdateStatus')

    stage = completion_stages.ManifestVersionedSyncCompletionStage(
        self._run, self.buildstore, self.sync_stage, success=True)

    stage.Run()
    update_status_mock.assert_called_once_with(
        message=None, success_map={self.BOT_ID: True})

  def testManifestVersionedSyncCompletedFailure(self):
    """Tests basic ManifestVersionedSyncStageCompleted on failure"""
    stage = completion_stages.ManifestVersionedSyncCompletionStage(
        self._run, self.buildstore, self.sync_stage, success=False)
    message = 'foo'
    get_msg_mock = self.PatchObject(
        generic_stages.BuilderStage,
        'GetBuildFailureMessage',
        return_value=message)
    update_status_mock = self.PatchObject(manifest_version.BuildSpecsManager,
                                          'UpdateStatus')

    stage.Run()
    update_status_mock.assert_called_once_with(
        message='foo', success_map={self.BOT_ID: False})
    get_msg_mock.assert_called_once_with()

  def testManifestVersionedSyncCompletedIncomplete(self):
    """Tests basic ManifestVersionedSyncStageCompleted on incomplete build."""
    stage = completion_stages.ManifestVersionedSyncCompletionStage(
        self._run, self.buildstore, self.sync_stage, success=False)
    stage.Run()

  def testGetBuilderSuccessMap(self):
    """Tests that the builder success map is properly created."""
    board_runattrs = self._run.GetBoardRunAttrs('eve')
    board_runattrs.SetParallel('success', True)
    builder_success_map = completion_stages.GetBuilderSuccessMap(
        self._run, True)
    expected_map = {self.BOT_ID: True}
    self.assertEqual(expected_map, builder_success_map)


class MasterSlaveSyncCompletionStageMockConfigTest(
    generic_stages_unittest.AbstractStageTestCase):
  """Tests MasterSlaveSyncCompletionStage with ManifestVersionedSyncStage."""
  BOT_ID = 'master'

  def setUp(self):
    self.source_repo = 'ssh://source/repo'
    self.manifest_version_url = 'fake manifest url'
    self.branch = 'master'
    self.build_type = constants.PFQ_TYPE

    # Use our mocked out SiteConfig for all tests.
    self.test_config = self._GetTestConfig()
    self._Prepare(site_config=self.test_config)
    self.buildstore = FakeBuildStore()

  def ConstructStage(self):
    sync_stage = sync_stages.ManifestVersionedSyncStage(self._run,
                                                        self.buildstore)
    return completion_stages.MasterSlaveSyncCompletionStage(
        self._run, self.buildstore, sync_stage, success=True)

  def _GetTestConfig(self):
    test_config = config_lib.SiteConfig()
    test_config.Add(
        'master',
        config_lib.BuildConfig(),
        boards=[],
        build_type=self.build_type,
        master=True,
        slave_configs=['test3', 'test5'],
        manifest_version=True,
    )
    test_config.Add(
        'test1',
        config_lib.BuildConfig(),
        boards=['amd64-generic'],
        manifest_version=True,
        build_type=constants.PFQ_TYPE,
        overlays='public',
        important=False,
        chrome_rev=None,
        branch=False,
        internal=False,
        master=False,
    )
    test_config.Add(
        'test2',
        config_lib.BuildConfig(),
        boards=['amd64-generic'],
        manifest_version=False,
        build_type=constants.PFQ_TYPE,
        overlays='public',
        important=True,
        chrome_rev=None,
        branch=False,
        internal=False,
        master=False,
    )
    test_config.Add(
        'test3',
        config_lib.BuildConfig(),
        boards=['amd64-generic'],
        manifest_version=True,
        build_type=constants.PFQ_TYPE,
        overlays='both',
        important=True,
        chrome_rev=None,
        branch=False,
        internal=True,
        master=False,
    )
    test_config.Add(
        'test4',
        config_lib.BuildConfig(),
        boards=['amd64-generic'],
        manifest_version=True,
        build_type=constants.PFQ_TYPE,
        overlays='both',
        important=True,
        chrome_rev=None,
        branch=True,
        internal=True,
        master=False,
    )
    test_config.Add(
        'test5',
        config_lib.BuildConfig(),
        boards=['amd64-generic'],
        manifest_version=True,
        build_type=constants.PFQ_TYPE,
        overlays='public',
        important=True,
        chrome_rev=None,
        branch=False,
        internal=False,
        master=False,
    )
    return test_config

  def testGetSlavesForMaster(self):
    """Tests that we get the slaves for a fake unified master configuration."""
    stage = self.ConstructStage()
    p = stage._GetSlaveConfigs()
    self.assertEqual([self.test_config['test3'], self.test_config['test5']], p)


class CanaryCompletionStageTest(generic_stages_unittest.AbstractStageTestCase):
  """Tests how canary master handles failures in CanaryCompletionStage."""
  BOT_ID = 'master-release'

  # We duplicate __init__ to specify a default for bot_id.
  # pylint: disable=arguments-differ,useless-super-delegation
  def _Prepare(self, bot_id=BOT_ID, **kwargs):
    super(CanaryCompletionStageTest, self)._Prepare(bot_id, **kwargs)

  def setUp(self):
    self.build_type = constants.CANARY_TYPE
    self._Prepare()
    self.buildstore = FakeBuildStore()

  def ConstructStage(self):
    """Returns a CanaryCompletionStage object."""
    sync_stage = sync_stages.ManifestVersionedSyncStage(self._run,
                                                        self.buildstore)
    return completion_stages.CanaryCompletionStage(
        self._run, self.buildstore, sync_stage, success=True)

  def testGetBuilderStatusesFetcher(self):
    """Test GetBuilderStatusesFetcher."""
    mock_fetcher = mock.Mock()
    self.PatchObject(
        builder_status_lib, 'BuilderStatusesFetcher', return_value=mock_fetcher)
    mock_wait = self.PatchObject(
        completion_stages.MasterSlaveSyncCompletionStage,
        '_WaitForSlavesToComplete')
    stage = self.ConstructStage()
    stage._run.attrs.manifest_manager = mock.Mock()

    self.assertEqual(stage._GetBuilderStatusesFetcher(), mock_fetcher)
    self.assertEqual(mock_wait.call_count, 1)


class PublishUprevChangesStageTest(
    generic_stages_unittest.AbstractStageTestCase):
  """Tests for the PublishUprevChanges stage."""
  BOT_ID = 'master-mst-android-pfq'

  def setUp(self):
    self.PatchObject(completion_stages.PublishUprevChangesStage,
                     '_GetPortageEnvVar')

    overlays_map = {
        constants.BOTH_OVERLAYS: ['ext', 'int'],
        constants.PUBLIC_OVERLAYS: ['ext'],
        constants.PRIVATE_OVERLAYS: ['int'],
    }

    self.PatchObject(portage_util, 'FindOverlays',
                     side_effect=lambda o, buildroot: overlays_map[o])
    self.PatchObject(prebuilts.BinhostConfWriter, 'Perform')
    self.push_mock = self.PatchObject(commands, 'UprevPush')
    self.PatchObject(generic_stages.BuilderStage, 'GetRepoRepository')
    self.PatchObject(commands, 'UprevPackages')

    self._Prepare()
    self.buildstore = FakeBuildStore()

  def ConstructStage(self):
    sync_stage = sync_stages.ManifestVersionedSyncStage(self._run,
                                                        self.buildstore)
    sync_stage.pool = mock.MagicMock()
    return completion_stages.PublishUprevChangesStage(
        self._run, self.buildstore, sync_stage, success=True)

  def testCheckSlaveUploadPrebuiltsTest(self):
    """Tests for CheckSlaveUploadPrebuiltsTest."""
    stage = self.ConstructStage()
    stage._build_stage_id = 'test_build_stage_id'

    mock_cidb = mock.MagicMock()
    cidb.CIDBConnectionFactory.SetupMockCidb(mock_cidb)

    stage_name = 'UploadPrebuilts'

    slave_a = 'slave_a'
    slave_b = 'slave_b'
    slave_c = 'slave_c'

    slave_configs_a = [{'name': slave_a}, {'name': slave_b}]
    slave_stages_a = [{'name': stage_name,
                       'build_config': slave_a,
                       'status': constants.BUILDER_STATUS_PASSED},
                      {'name': stage_name,
                       'build_config': slave_b,
                       'status': constants.BUILDER_STATUS_PASSED}]

    self.PatchObject(
        completion_stages.PublishUprevChangesStage,
        '_GetSlaveConfigs',
        return_value=slave_configs_a)
    self.PatchObject(FakeBuildStore, 'GetBuildStatuses', return_value=[])
    self.PatchObject(FakeBuildStore, 'GetBuildsStages',
                     return_value=slave_stages_a)

    # All important slaves are covered
    self.assertTrue(stage.CheckSlaveUploadPrebuiltsTest())

    slave_stages_b = [{'name': stage_name,
                       'build_config': slave_a,
                       'status': constants.BUILDER_STATUS_FAILED},
                      {'name': stage_name,
                       'build_config': slave_b,
                       'status': constants.BUILDER_STATUS_PASSED}]
    self.PatchObject(
        completion_stages.PublishUprevChangesStage,
        '_GetSlaveConfigs',
        return_value=slave_configs_a)
    self.PatchObject(FakeBuildStore, 'GetBuildsStages',
                     return_value=slave_stages_b)

    # Slave_a didn't pass the stage
    self.assertFalse(stage.CheckSlaveUploadPrebuiltsTest())

    slave_configs_b = [{'name': slave_a}, {'name': slave_b}, {'name': slave_c}]
    self.PatchObject(
        completion_stages.PublishUprevChangesStage,
        '_GetSlaveConfigs',
        return_value=slave_configs_b)
    self.PatchObject(FakeBuildStore, 'GetBuildsStages',
                     return_value=slave_stages_a)

    # No stage information for slave_c
    self.assertFalse(stage.CheckSlaveUploadPrebuiltsTest())

  def testAndroidPush(self):
    """Test values for PublishUprevChanges with Android PFQ."""
    self._Prepare(
        bot_id=constants.NYC_ANDROID_PFQ_MASTER,
        extra_config={'push_overlays': constants.PUBLIC_OVERLAYS},
        extra_cmd_args=['--android_rev', constants.ANDROID_REV_LATEST])
    self._run.options.prebuilts = True
    self.RunStage()
    self.push_mock.assert_called_once_with(
        self.build_root, overlay_type='public', dryrun=False,
        staging_branch=None)
    self.assertTrue(self._run.attrs.metadata.GetValue('UprevvedAndroid'))
    metadata_dict = self._run.attrs.metadata.GetDict()
    self.assertNotIn('UprevvedChrome', metadata_dict)

  def testPerformStageOnChromePFQ(self):
    """Test PerformStage on ChromePFQ."""
    stage = self.ConstructStage()
    stage.sync_stage.pool.HasPickedUpCLs.return_value = True
    stage.PerformStage()
    self.push_mock.assert_called_once_with(
        self.build_root, overlay_type='both', dryrun=False, staging_branch=None)
