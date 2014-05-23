#!/usr/bin/python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for completion stages."""

import mox
import os
import sys

sys.path.insert(0, os.path.abspath('%s/../../..' % os.path.dirname(__file__)))
from chromite.cbuildbot import cbuildbot_commands as commands
from chromite.cbuildbot import constants
from chromite.cbuildbot import manifest_version
from chromite.cbuildbot import validation_pool
from chromite.cbuildbot.stages import completion_stages
from chromite.cbuildbot.stages import generic_stages_unittest
from chromite.cbuildbot.stages import sync_stages_unittest
from chromite.cbuildbot.stages import sync_stages
from chromite.lib import alerts
from chromite.lib import cros_test_lib


# pylint: disable=R0901,W0212
class ManifestVersionedSyncCompletionStageTest(
    sync_stages_unittest.ManifestVersionedSyncStageTest):
  """Tests the ManifestVersionedSyncCompletion stage."""
    # pylint: disable=W0223

  def testManifestVersionedSyncCompletedSuccess(self):
    """Tests basic ManifestVersionedSyncStageCompleted on success"""
    self.mox.StubOutWithMock(manifest_version.BuildSpecsManager, 'UpdateStatus')

    self.manager.UpdateStatus(message=None, success=True,
                              dashboard_url=mox.IgnoreArg())

    self.mox.ReplayAll()
    stage = completion_stages.ManifestVersionedSyncCompletionStage(
        self._run, self.sync_stage, success=True)
    stage.Run()
    self.mox.VerifyAll()

  def testManifestVersionedSyncCompletedFailure(self):
    """Tests basic ManifestVersionedSyncStageCompleted on failure"""
    self.mox.StubOutWithMock(manifest_version.BuildSpecsManager, 'UpdateStatus')

    self.manager.UpdateStatus(message=None, success=False,
                              dashboard_url=mox.IgnoreArg())


    self.mox.ReplayAll()
    stage = completion_stages.ManifestVersionedSyncCompletionStage(
        self._run, self.sync_stage, success=False)
    stage.Run()
    self.mox.VerifyAll()

  def testManifestVersionedSyncCompletedIncomplete(self):
    """Tests basic ManifestVersionedSyncStageCompleted on incomplete build."""
    self.mox.ReplayAll()
    stage = completion_stages.ManifestVersionedSyncCompletionStage(
        self._run, self.sync_stage, success=False)
    stage.Run()
    self.mox.VerifyAll()


class MasterSlaveSyncCompletionStageTest(
    generic_stages_unittest.AbstractStageTest):
  """Tests the MasterSlaveSyncCompletionStage."""
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
    sync_stage = sync_stages.MasterSlaveSyncStage(self._run)
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

      self.mox.ReplayAll()

      stage = self.ConstructStage()
      p = stage._GetSlaveConfigs()
      self.mox.VerifyAll()

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
    failed_msg = validation_pool.ValidationFailedMessage(
        'message', [], True, 'reason')
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


class CommitQueueCompletionStageTest(
    generic_stages_unittest.AbstractStageTest):
  """Tests how CQ master handles changes in CommitQueueCompletionStage."""
  BOT_ID = 'master-paladin'

  def _Prepare(self, bot_id=BOT_ID, **kwargs):
    super(CommitQueueCompletionStageTest, self)._Prepare(bot_id, **kwargs)
    self._run.config['master'] = True

  def setUp(self):
    # pylint: disable=E1120
    self.build_type = constants.PFQ_TYPE
    self._Prepare()

    self.partial_submit_changes = ['C', 'D']
    self.other_changes = ['A', 'B']
    self.changes = self.other_changes + self.partial_submit_changes

    self.mox.StubOutWithMock(completion_stages.MasterSlaveSyncCompletionStage,
                             'HandleFailure')
    self.mox.StubOutWithMock(completion_stages.CommitQueueCompletionStage,
                             'SubmitPartialPool')
    self.mox.StubOutWithMock(completion_stages.CommitQueueCompletionStage,
                             '_ToTSanity')
    self.mox.StubOutWithMock(alerts, '_SendEmailHelper')

    self.PatchObject(completion_stages.CommitQueueCompletionStage,
                     '_GetFailedMessages')
    self.PatchObject(completion_stages.CommitQueueCompletionStage,
                     'ShouldDisableAlerts', return_value=False)

  def ConstructStage(self):
    """Returns a CommitQueueCompletionStage object."""
    sync_stage = sync_stages.CommitQueueSyncStage(self._run)
    sync_stage.pool = mox.MockObject(validation_pool.ValidationPool)
    sync_stage.pool.changes = self.changes
    self.mox.StubOutWithMock(sync_stage.pool,
                             'HandleValidationFailure')
    self.mox.StubOutWithMock(sync_stage.pool,
                             'HandleValidationTimeout')

    return completion_stages.CommitQueueCompletionStage(
        self._run, sync_stage, success=True)

  def testNoInflightBuildersNoInfraFail(self):
    """Test case where there are no inflight builders and no infra failures."""
    # pylint: disable=E1120
    failing = ['foo']
    inflight = []

    stage = self.ConstructStage()
    self.PatchObject(completion_stages.CommitQueueCompletionStage,
                     '_GetInfraFailMessages', return_value=[])
    completion_stages.CommitQueueCompletionStage.SubmitPartialPool(
        mox.IgnoreArg()).AndReturn(self.other_changes)
    completion_stages.CommitQueueCompletionStage._ToTSanity(
        mox.IgnoreArg(), mox.IgnoreArg())
    stage.sync_stage.pool.HandleValidationFailure(
        mox.IgnoreArg(), no_stat=[], sanity=mox.IgnoreArg(),
        changes=self.other_changes)

    self.mox.ReplayAll()
    stage.CQMasterHandleFailure(failing, inflight, [])
    self.mox.VerifyAll()

  def testNoInflightBuildersWithInfraFail(self):
    """Test case where there are no inflight builders but are infra failures."""
    # pylint: disable=E1120
    failing = ['foo']
    inflight = []

    stage = self.ConstructStage()
    self.PatchObject(completion_stages.CommitQueueCompletionStage,
                     '_GetInfraFailMessages', return_value=['msg'])
    # An alert is sent, since there are infra failures.
    alerts._SendEmailHelper(mox.IgnoreArg(), mox.IgnoreArg(), mox.IgnoreArg(),
                            mox.IgnoreArg())
    completion_stages.CommitQueueCompletionStage._GetFailedMessages(failing)
    completion_stages.CommitQueueCompletionStage.SubmitPartialPool(
        mox.IgnoreArg()).AndReturn(self.other_changes)
    completion_stages.CommitQueueCompletionStage._ToTSanity(
        mox.IgnoreArg(), mox.IgnoreArg())
    stage.sync_stage.pool.HandleValidationFailure(
        mox.IgnoreArg(), no_stat=[], sanity=mox.IgnoreArg(),
        changes=self.other_changes)

    self.mox.ReplayAll()
    stage.CQMasterHandleFailure(failing, inflight, [])
    self.mox.VerifyAll()

  def testWithInflightBuildersNoInfraFail(self):
    """Tests that we don't submit partial pool on non-empty inflight."""
    # pylint: disable=E1120
    failing = ['foo', 'bar']
    inflight = ['inflight']

    stage = self.ConstructStage()
    self.PatchObject(completion_stages.CommitQueueCompletionStage,
                     '_GetInfraFailMessages', return_value=[])
    # An alert is sent, since we have an inflight build still.
    alerts._SendEmailHelper(mox.IgnoreArg(), mox.IgnoreArg(), mox.IgnoreArg(),
                            mox.IgnoreArg())
    completion_stages.CommitQueueCompletionStage._GetFailedMessages(failing)
    completion_stages.CommitQueueCompletionStage._ToTSanity(
        mox.IgnoreArg(), mox.IgnoreArg())
    stage.sync_stage.pool.HandleValidationTimeout(
        sanity=mox.IgnoreArg(), changes=self.changes)

    self.mox.ReplayAll()
    stage.CQMasterHandleFailure(failing, inflight, [])
    self.mox.VerifyAll()


class PublishUprevChangesStageTest(generic_stages_unittest.AbstractStageTest):
  """Tests for the PublishUprevChanges stage."""

  def setUp(self):
    # pylint: disable=E1120
    self.mox.StubOutWithMock(completion_stages.PublishUprevChangesStage,
                             '_GetPortageEnvVar')
    self.mox.StubOutWithMock(commands, 'UploadPrebuilts')
    self.mox.StubOutWithMock(commands, 'UprevPush')
    self.mox.StubOutWithMock(completion_stages.PublishUprevChangesStage,
                             '_ExtractOverlays')
    completion_stages.PublishUprevChangesStage._ExtractOverlays().AndReturn(
        [['foo'], ['bar']])

  def ConstructStage(self):
    return completion_stages.PublishUprevChangesStage(self._run, success=True)

  def testPush(self):
    """Test values for PublishUprevChanges."""
    self._Prepare(extra_config={'build_type': constants.BUILD_FROM_SOURCE_TYPE,
                                'push_overlays': constants.PUBLIC_OVERLAYS,
                                'master': True},
                  extra_cmd_args=['--chrome_rev', constants.CHROME_REV_TOT])
    self._run.options.prebuilts = True
    completion_stages.commands.UprevPush(self.build_root, ['bar'], False)

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()


if __name__ == '__main__':
  cros_test_lib.main()
