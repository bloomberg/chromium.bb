# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for completion stages."""

from __future__ import print_function

import mock
import sys

from chromite.cbuildbot import cbuildbot_run
from chromite.cbuildbot import commands
from chromite.cbuildbot import constants
from chromite.cbuildbot import failures_lib
from chromite.cbuildbot import manifest_version
from chromite.cbuildbot import results_lib
from chromite.cbuildbot.stages import completion_stages
from chromite.cbuildbot.stages import generic_stages_unittest
from chromite.cbuildbot.stages import sync_stages_unittest
from chromite.cbuildbot.stages import sync_stages
from chromite.lib import alerts
from chromite.lib import clactions
from chromite.lib import patch_unittest


# pylint: disable=protected-access


class ManifestVersionedSyncCompletionStageTest(
    sync_stages_unittest.ManifestVersionedSyncStageTest):
  """Tests the ManifestVersionedSyncCompletion stage."""

  # pylint: disable=abstract-method

  BOT_ID = 'x86-mario-release'

  def testManifestVersionedSyncCompletedSuccess(self):
    """Tests basic ManifestVersionedSyncStageCompleted on success"""
    board_runattrs = self._run.GetBoardRunAttrs('x86-mario')
    board_runattrs.SetParallel('success', True)
    update_status_mock = self.PatchObject(
        manifest_version.BuildSpecsManager, 'UpdateStatus')

    stage = completion_stages.ManifestVersionedSyncCompletionStage(
        self._run, self.sync_stage, success=True)

    stage.Run()
    update_status_mock.assert_called_once_with(
        message=None, success_map={self.BOT_ID: True}, dashboard_url=mock.ANY)

  def testManifestVersionedSyncCompletedFailure(self):
    """Tests basic ManifestVersionedSyncStageCompleted on failure"""
    stage = completion_stages.ManifestVersionedSyncCompletionStage(
        self._run, self.sync_stage, success=False)
    message = 'foo'
    self.PatchObject(stage, 'GetBuildFailureMessage', return_value=message)
    update_status_mock = self.PatchObject(
        manifest_version.BuildSpecsManager, 'UpdateStatus')

    stage.Run()
    update_status_mock.assert_called_once_with(
        message='foo', success_map={self.BOT_ID: False},
        dashboard_url=mock.ANY)

  def testManifestVersionedSyncCompletedIncomplete(self):
    """Tests basic ManifestVersionedSyncStageCompleted on incomplete build."""
    stage = completion_stages.ManifestVersionedSyncCompletionStage(
        self._run, self.sync_stage, success=False)
    stage.Run()

  def testMeaningfulMessage(self):
    """Tests that all essential components are in the message."""
    stage = completion_stages.ManifestVersionedSyncCompletionStage(
        self._run, self.sync_stage, success=False)

    exception = Exception('failed!')
    traceback = results_lib.RecordedTraceback(
        'TacoStage', 'Taco', exception, 'traceback_str')
    self.PatchObject(
        results_lib.Results, 'GetTracebacks', return_value=[traceback])

    msg = stage.GetBuildFailureMessage()
    self.assertTrue(stage._run.config.name in msg.message)
    self.assertTrue(stage._run.ConstructDashboardURL() in msg.message)
    self.assertTrue('TacoStage' in msg.message)
    self.assertTrue(str(exception) in msg.message)

    self.assertTrue('TacoStage' in msg.reason)
    self.assertTrue(str(exception) in msg.reason)

  def testGetBuilderSuccessMap(self):
    """Tests that the builder success map is properly created."""
    board_runattrs = self._run.GetBoardRunAttrs('x86-mario')
    board_runattrs.SetParallel('success', True)
    builder_success_map = completion_stages.GetBuilderSuccessMap(
        self._run, True)
    expected_map = {self.BOT_ID: True}
    self.assertEqual(expected_map, builder_success_map)


class MasterSlaveSyncCompletionStageTest(
    generic_stages_unittest.AbstractStageTest):
  """Tests MasterSlaveSyncCompletionStage with ManifestVersionedSyncStage."""
  BOT_ID = 'x86-generic-paladin'

  def setUp(self):
    self.source_repo = 'ssh://source/repo'
    self.manifest_version_url = 'fake manifest url'
    self.branch = 'master'
    self.build_type = constants.PFQ_TYPE

    self._Prepare()

  def _Prepare(self, bot_id=None, **kwargs):
    super(MasterSlaveSyncCompletionStageTest, self)._Prepare(bot_id, **kwargs)

    self._run.config['manifest_version'] = True
    self._run.config['build_type'] = self.build_type
    self._run.config['master'] = True

  def ConstructStage(self):
    sync_stage = sync_stages.ManifestVersionedSyncStage(self._run)
    return completion_stages.MasterSlaveSyncCompletionStage(
        self._run, sync_stage, success=True)

  def _GetTestConfig(self):
    test_config = {}
    test_config['test1'] = {
        'manifest_version': True,
        'build_type': constants.PFQ_TYPE,
        'overlays': 'public',
        'important': False,
        'chrome_rev': None,
        'branch': False,
        'internal': False,
        'master': False,
    }
    test_config['test2'] = {
        'manifest_version': False,
        'build_type': constants.PFQ_TYPE,
        'overlays': 'public',
        'important': True,
        'chrome_rev': None,
        'branch': False,
        'internal': False,
        'master': False,
    }
    test_config['test3'] = {
        'manifest_version': True,
        'build_type': constants.PFQ_TYPE,
        'overlays': 'both',
        'important': True,
        'chrome_rev': None,
        'branch': False,
        'internal': True,
        'master': False,
    }
    test_config['test4'] = {
        'manifest_version': True,
        'build_type': constants.PFQ_TYPE,
        'overlays': 'both',
        'important': True,
        'chrome_rev': None,
        'branch': True,
        'internal': True,
        'master': False,
    }
    test_config['test5'] = {
        'manifest_version': True,
        'build_type': constants.PFQ_TYPE,
        'overlays': 'public',
        'important': True,
        'chrome_rev': None,
        'branch': False,
        'internal': False,
        'master': False,
    }
    return test_config

  def testGetSlavesForMaster(self):
    """Tests that we get the slaves for a fake unified master configuration."""
    orig_config = completion_stages.cbuildbot_config.config
    try:
      test_config = self._GetTestConfig()
      completion_stages.cbuildbot_config.config = test_config
      stage = self.ConstructStage()

      p = stage._GetSlaveConfigs()
      self.assertTrue(test_config['test3'] in p)
      self.assertTrue(test_config['test5'] in p)
      self.assertFalse(test_config['test1'] in p)
      self.assertFalse(test_config['test2'] in p)
      self.assertFalse(test_config['test4'] in p)

    finally:
      completion_stages.cbuildbot_config.config = orig_config

  def testIsFailureFatal(self):
    """Tests the correctness of the _IsFailureFatal method"""
    stage = self.ConstructStage()

    # Test behavior when there are no sanity check builders
    self.assertFalse(stage._IsFailureFatal(set(), set(), set()))
    self.assertTrue(stage._IsFailureFatal(set(['test3']), set(), set()))
    self.assertTrue(stage._IsFailureFatal(set(), set(['test5']), set()))
    self.assertTrue(stage._IsFailureFatal(set(), set(), set(['test1'])))

    # Test behavior where there is a sanity check builder
    stage._run.config.sanity_check_slaves = ['sanity']
    self.assertTrue(stage._IsFailureFatal(set(['test5']), set(['sanity']),
                                          set()))
    self.assertFalse(stage._IsFailureFatal(set(), set(['sanity']), set()))
    self.assertTrue(stage._IsFailureFatal(set(), set(['sanity']),
                                          set(['test1'])))
    self.assertFalse(stage._IsFailureFatal(set(), set(),
                                           set(['sanity'])))

  def testAnnotateFailingBuilders(self):
    """Tests that _AnnotateFailingBuilders is free of syntax errors."""
    stage = self.ConstructStage()

    failing = {'a'}
    inflight = {}
    failed_msg = failures_lib.BuildFailureMessage(
        'message', [], True, 'reason', 'bot')
    status = manifest_version.BuilderStatus('failed', failed_msg, 'url')

    statuses = {'a' : status}
    no_stat = set()
    stage._AnnotateFailingBuilders(failing, inflight, no_stat, statuses)

  def testExceptionHandler(self):
    """Verify _HandleStageException is sane."""
    stage = self.ConstructStage()
    e = ValueError('foo')
    try:
      raise e
    except ValueError:
      ret = stage._HandleStageException(sys.exc_info())
      self.assertTrue(isinstance(ret, tuple))
      self.assertEqual(len(ret), 3)
      self.assertEqual(ret[0], e)


class MasterSlaveSyncCompletionStageTestWithLKGMSync(
    MasterSlaveSyncCompletionStageTest):
  """Tests the MasterSlaveSyncCompletionStage with MasterSlaveLKGMSyncStage."""
  BOT_ID = 'x86-generic-paladin'

  def ConstructStage(self):
    sync_stage = sync_stages.MasterSlaveLKGMSyncStage(self._run)
    return completion_stages.MasterSlaveSyncCompletionStage(
        self._run, sync_stage, success=True)


class CanaryCompletionStageTest(
    generic_stages_unittest.AbstractStageTest):
  """Tests how canary master handles failures in CanaryCompletionStage."""
  BOT_ID = 'master-release'

  def _Prepare(self, bot_id=BOT_ID, **kwargs):
    super(CanaryCompletionStageTest, self)._Prepare(bot_id, **kwargs)

  def setUp(self):
    self.build_type = constants.CANARY_TYPE
    self._Prepare()

  def ConstructStage(self):
    """Returns a CanaryCompletionStage object."""
    sync_stage = sync_stages.ManifestVersionedSyncStage(self._run)
    return completion_stages.CanaryCompletionStage(
        self._run, sync_stage, success=True)

  def testComposeTreeStatusMessage(self):
    """Tests that the status message is constructed as expected."""
    failing = ['foo1', 'foo2', 'foo3', 'foo4', 'foo5']
    inflight = ['bar']
    no_stat = []
    stage = self.ConstructStage()
    self.assertEqual(
        stage._ComposeTreeStatusMessage(failing, inflight, no_stat),
        'bar timed out; foo1,foo2 and 3 others failed')


class CommitQueueCompletionStageTest(
    generic_stages_unittest.AbstractStageTest, patch_unittest.MockPatchBase):
  """Tests how CQ master handles changes in CommitQueueCompletionStage."""
  BOT_ID = 'master-paladin'

  def _Prepare(self, bot_id=BOT_ID, **kwargs):
    super(CommitQueueCompletionStageTest, self)._Prepare(bot_id, **kwargs)
    self.assertTrue(self._run.config['master'])

  def setUp(self):
    self.build_type = constants.PFQ_TYPE
    self._Prepare()

    self.partial_submit_changes = ['C', 'D']
    self.other_changes = ['A', 'B']
    self.changes = self.other_changes + self.partial_submit_changes
    self.tot_sanity_mock = self.PatchObject(
        completion_stages.CommitQueueCompletionStage,
        '_ToTSanity',
        return_value=True)

    self.alert_email_mock = self.PatchObject(alerts, '_SendEmailHelper')
    self.PatchObject(cbuildbot_run._BuilderRunBase,
                     'InProduction', return_value=True)
    self.PatchObject(completion_stages.MasterSlaveSyncCompletionStage,
                     'HandleFailure')
    self.PatchObject(completion_stages.CommitQueueCompletionStage,
                     '_GetFailedMessages')
    self.PatchObject(completion_stages.CommitQueueCompletionStage,
                     '_GetSlaveMappingAndCLActions',
                     return_value=(dict(), []))
    self.PatchObject(clactions, 'GetRelevantChangesForBuilds')

  # pylint: disable=W0221
  def ConstructStage(self, tree_was_open=True):
    """Returns a CommitQueueCompletionStage object.

    Args:
      tree_was_open: If not true, tree was not open when we acquired changes.
    """
    sync_stage = sync_stages.CommitQueueSyncStage(self._run)
    sync_stage.pool = mock.MagicMock()
    sync_stage.pool.changes = self.changes
    sync_stage.pool.tree_was_open = tree_was_open

    sync_stage.pool.handle_failure_mock = self.PatchObject(
        sync_stage.pool, 'HandleValidationFailure')
    sync_stage.pool.handle_timeout_mock = self.PatchObject(
        sync_stage.pool, 'HandleValidationTimeout')
    return completion_stages.CommitQueueCompletionStage(
        self._run, sync_stage, success=True)

  def VerifyStage(self, failing, inflight, handle_failure=True,
                  handle_timeout=False, sane_tot=True, submit_partial=False,
                  alert=False, stage=None):
    """Runs and Verifies CQMasterHandleFailure.

    Args:
      failing: The names of the builders that failed.
      inflight: The names of the buiders that timed out.
      handle_failure: If True, calls HandleValidationFailure.
      handle_timeout: If True, calls HandleValidationTimeout.
      sane_tot: If not true, assumes TOT is not sane.
      submit_partial: If True, submit partial pool.
      alert: If True, sends out an alert email for infra failures.
      stage: If set, use this constructed stage, otherwise create own.
    """
    if not stage:
      stage = self.ConstructStage()

    if submit_partial:
      self.PatchObject(stage.sync_stage.pool, 'SubmitPartialPool',
                       return_value=self.other_changes)
    else:
      self.PatchObject(stage.sync_stage.pool, 'SubmitPartialPool',
                       return_value=self.changes)

    stage.CQMasterHandleFailure(failing, inflight, [])

    # Verify the calls.
    self.tot_sanity_mock.assert_called_once_with(mock.ANY, mock.ANY)
    if alert:
      self.alert_email_mock.called_once_with(
          mock.ANY, mock.ANY, mock.ANY, mock.ANY)

    if handle_failure:
      stage.sync_stage.pool.handle_failure_mock.assert_called_once_with(
          mock.ANY, no_stat=[], sanity=sane_tot, changes=self.other_changes)

    if handle_timeout:
      stage.sync_stage.pool.handle_timeout_mock.assert_called_once_with(
          sanity=mock.ANY, changes=self.changes)

  def testNoInflightBuildersNoInfraFail(self):
    """Test case where there are no inflight builders and no infra failures."""
    failing = ['foo']
    inflight = []

    self.PatchObject(completion_stages.CommitQueueCompletionStage,
                     '_GetInfraFailMessages', return_value=[])
    self.PatchObject(completion_stages.CommitQueueCompletionStage,
                     '_GetBuildersWithNoneMessages', return_value=[])
    self.VerifyStage(failing, inflight, submit_partial=True)

  def testNoInflightBuildersWithInfraFail(self):
    """Test case where there are no inflight builders but are infra failures."""
    failing = ['foo']
    inflight = []

    self.PatchObject(completion_stages.CommitQueueCompletionStage,
                     '_GetInfraFailMessages', return_value=['msg'])
    self.PatchObject(completion_stages.CommitQueueCompletionStage,
                     '_GetBuildersWithNoneMessages', return_value=[])
    # An alert is sent, since there are infra failures.
    self.VerifyStage(failing, inflight, submit_partial=True, alert=True)

  def testNoInflightBuildersWithNoneFailureMessages(self):
    """Test case where failed builders reported NoneType messages."""
    failing = ['foo']
    inflight = []

    self.PatchObject(completion_stages.CommitQueueCompletionStage,
                     '_GetInfraFailMessages', return_value=[])
    self.PatchObject(completion_stages.CommitQueueCompletionStage,
                     '_GetBuildersWithNoneMessages', return_value=['foo'])
    # An alert is sent, since NonType messages are considered infra failures.
    self.VerifyStage(failing, inflight, submit_partial=True, alert=True)

  def testWithInflightBuildersNoInfraFail(self):
    """Tests that we don't submit partial pool on non-empty inflight."""
    failing = ['foo', 'bar']
    inflight = ['inflight']

    self.PatchObject(completion_stages.CommitQueueCompletionStage,
                     '_GetInfraFailMessages', return_value=[])
    self.PatchObject(completion_stages.CommitQueueCompletionStage,
                     '_GetBuildersWithNoneMessages', return_value=[])

    # An alert is sent, since we have an inflight build still.
    self.VerifyStage(failing, inflight, handle_failure=False,
                     handle_timeout=True, alert=True)

  def testGetRelevantChangesForSlave(self):
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
    self.PatchObject(completion_stages.CommitQueueCompletionStage,
                     '_GetSlaveMappingAndCLActions',
                     return_value=(config_map, []))
    self.PatchObject(clactions, 'GetRelevantChangesForBuilds',
                     return_value=changes_by_build_id)

    stage = self.ConstructStage()
    results = stage.GetRelevantChangesForSlaves(changes, no_stat)
    self.assertEqual(results, expected)

  def testWithExponentialFallbackApplied(self):
    """Tests that we don't treat TOT as sane when it isn't."""
    failing = ['foo', 'bar']
    inflight = ['inflight']
    stage = self.ConstructStage(tree_was_open=False)
    self.PatchObject(completion_stages.CommitQueueCompletionStage,
                     '_GetInfraFailMessages', return_value=[])
    self.PatchObject(completion_stages.CommitQueueCompletionStage,
                     '_GetBuildersWithNoneMessages', return_value=['foo'])

    # An alert is sent, since we have an inflight build still.
    self.VerifyStage(failing, inflight, handle_failure=False,
                     handle_timeout=False, sane_tot=False, alert=True,
                     stage=stage)


class PublishUprevChangesStageTest(generic_stages_unittest.AbstractStageTest):
  """Tests for the PublishUprevChanges stage."""

  def setUp(self):
    self.PatchObject(completion_stages.PublishUprevChangesStage,
                     '_GetPortageEnvVar')
    self.PatchObject(completion_stages.PublishUprevChangesStage,
                     '_ExtractOverlays', return_value=[['foo'], ['bar']])
    self.push_mock = self.PatchObject(commands, 'UprevPush')

  def ConstructStage(self):
    return completion_stages.PublishUprevChangesStage(self._run, success=True)

  def testPush(self):
    """Test values for PublishUprevChanges."""
    self._Prepare(extra_config={'build_type': constants.BUILD_FROM_SOURCE_TYPE,
                                'push_overlays': constants.PUBLIC_OVERLAYS,
                                'master': True},
                  extra_cmd_args=['--chrome_rev', constants.CHROME_REV_TOT])
    self._run.options.prebuilts = True
    self.RunStage()
    self.push_mock.assert_called_once_with(self.build_root, ['bar'], False)
