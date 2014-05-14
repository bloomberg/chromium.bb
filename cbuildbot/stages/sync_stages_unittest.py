#!/usr/bin/python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for sync stages."""

import cPickle
import itertools
import os
import sys
import time
import tempfile

sys.path.insert(0, os.path.abspath('%s/../../..' % os.path.dirname(__file__)))
from chromite.cbuildbot import constants
from chromite.cbuildbot import lkgm_manager
from chromite.cbuildbot import manifest_version
from chromite.cbuildbot import manifest_version_unittest
from chromite.cbuildbot import repository
from chromite.cbuildbot import validation_pool
from chromite.cbuildbot.stages import sync_stages
from chromite.cbuildbot.stages import generic_stages_unittest
from chromite.lib import cros_build_lib_unittest
from chromite.lib import cros_test_lib
from chromite.lib import gerrit
from chromite.lib import git_unittest
from chromite.lib import gob_util
from chromite.lib import osutils
from chromite.lib import partial_mock
from chromite.lib import timeout_util


# TODO(build): Finish test wrapper (http://crosbug.com/37517).
# Until then, this has to be after the chromite imports.
import mock


# pylint: disable=R0901,W0212
class ManifestVersionedSyncStageTest(generic_stages_unittest.AbstractStageTest):
  """Tests the ManifestVersionedSync stage."""
  # pylint: disable=W0223

  def setUp(self):
    self.source_repo = 'ssh://source/repo'
    self.manifest_version_url = 'fake manifest url'
    self.branch = 'master'
    self.build_name = 'x86-generic'
    self.incr_type = 'branch'
    self.next_version = 'next_version'
    self.sync_stage = None

    repo = repository.RepoRepository(
      self.source_repo, self.tempdir, self.branch)
    self.manager = manifest_version.BuildSpecsManager(
      repo, self.manifest_version_url, self.build_name, self.incr_type,
      force=False, branch=self.branch, dry_run=True)

    self._Prepare()

  def _Prepare(self, bot_id=None, **kwargs):
    super(ManifestVersionedSyncStageTest, self)._Prepare(bot_id, **kwargs)

    self._run.config['manifest_version'] = self.manifest_version_url
    self.sync_stage = sync_stages.ManifestVersionedSyncStage(self._run)
    self.sync_stage.manifest_manager = self.manager
    self._run.attrs.manifest_manager = self.manager

  def testManifestVersionedSyncOnePartBranch(self):
    """Tests basic ManifestVersionedSyncStage with branch ooga_booga"""
    # pylint: disable=E1120
    self.mox.StubOutWithMock(sync_stages.ManifestVersionedSyncStage,
                             'Initialize')
    self.mox.StubOutWithMock(manifest_version.BuildSpecsManager,
                             'GetNextBuildSpec')
    self.mox.StubOutWithMock(manifest_version.BuildSpecsManager,
                             'GetLatestPassingSpec')
    self.mox.StubOutWithMock(sync_stages.SyncStage, 'ManifestCheckout')

    sync_stages.ManifestVersionedSyncStage.Initialize()
    self.manager.GetNextBuildSpec(
        dashboard_url=self.sync_stage.ConstructDashboardURL()
        ).AndReturn(self.next_version)
    self.manager.GetLatestPassingSpec().AndReturn(None)

    sync_stages.SyncStage.ManifestCheckout(self.next_version)

    self.mox.ReplayAll()
    self.sync_stage.Run()
    self.mox.VerifyAll()


class MockPatch(mock.MagicMock):
  """MagicMock for a GerritPatch-like object."""

  gerrit_number = '1234'
  patch_number = '1'
  project = 'chromiumos/chromite'
  status = 'NEW'
  internal = False
  current_patch_set = {
    'number': patch_number,
    'draft': False,
  }
  patch_dict = {
    'currentPatchSet': current_patch_set,
  }

  def HasApproval(self, field, value):
    """Pretends the patch is good.

    Pretend the patch has all of the values listed in
    constants.DEFAULT_CQ_READY_FIELDS, but not any other fields.
    """
    return constants.DEFAULT_CQ_READY_FIELDS.get(field, 0) == value


class BaseCQTest(generic_stages_unittest.StageTest):
  """Helper class for testing the CommitQueueSync stage"""
  PALADIN_BOT_ID = None
  MANIFEST_CONTENTS = '<manifest/>'

  def setUp(self):
    """Setup patchers for specified bot id."""
    # Mock out methods as needed.
    self.PatchObject(lkgm_manager, 'GenerateBlameList')
    self.PatchObject(repository.RepoRepository, 'ExportManifest',
                     return_value=self.MANIFEST_CONTENTS, autospec=True)
    self.StartPatcher(git_unittest.ManifestMock())
    self.StartPatcher(git_unittest.ManifestCheckoutMock())
    version_file = os.path.join(self.build_root, constants.VERSION_FILE)
    manifest_version_unittest.VersionInfoTest.WriteFakeVersionFile(version_file)
    rc_mock = self.StartPatcher(cros_build_lib_unittest.RunCommandMock())
    rc_mock.SetDefaultCmdResult()

    # Block the CQ from contacting GoB.
    self.PatchObject(gerrit.GerritHelper, 'RemoveCommitReady')
    self.PatchObject(gerrit.GerritHelper, 'SubmitChange')
    self.PatchObject(validation_pool.PaladinMessage, 'Send')

    # If a test is still contacting GoB, something is busted.
    self.PatchObject(gob_util, 'CreateHttpConn',
                     side_effect=AssertionError('Test should not contact GoB'))

    # Create a fake repo / manifest on disk that is used by subclasses.
    for subdir in ('repo', 'manifests'):
      osutils.SafeMakedirs(os.path.join(self.build_root, '.repo', subdir))
    self.manifest_path = os.path.join(self.build_root, '.repo', 'manifest.xml')
    osutils.WriteFile(self.manifest_path, self.MANIFEST_CONTENTS)
    self.PatchObject(validation_pool.ValidationPool, 'ReloadChanges',
                     side_effect=lambda x: x)

    self.sync_stage = None
    self._Prepare()

  def _Prepare(self, bot_id=None, **kwargs):
    super(BaseCQTest, self)._Prepare(bot_id, **kwargs)

    self.sync_stage = sync_stages.CommitQueueSyncStage(self._run)

  def PerformSync(self, remote='cros', committed=False, tree_open=True,
                  tree_throttled=False, tracking_branch='master',
                  num_patches=1, runs=0):
    """Helper to perform a basic sync for master commit queue.

    Args:
      remote: Remote name to use for mock patches. Default: 'cros'.
      committed: Value to be returned by mock patches' IsChangeCommitted.
                 Default: False.
      tree_open: If True, behave as if tree is open. Default: True.
      tree_throttled: If True, behave as if tree is throttled
                      (overriding the tree_open arg). Default: False.
      tracking_branch: Tracking branch name for mock patches.
      num_patches: The number of mock patches to create. Default: 1.
      runs: The maximum number of times to allow validation_pool.AcquirePool
            to wait for additional changes. runs=0 means never wait for
            additional changes. Default: 0.
    """
    p = MockPatch(remote=remote, tracking_branch=tracking_branch)
    my_patches = [p] * num_patches
    self.PatchObject(gerrit.GerritHelper, 'IsChangeCommitted',
                     return_value=committed, autospec=True)
    self.PatchObject(gerrit.GerritHelper, 'Query',
                     return_value=my_patches, autospec=True)
    if tree_throttled:
      self.PatchObject(timeout_util, 'WaitForTreeStatus',
                       return_value=constants.TREE_THROTTLED, autospec=True)
    elif tree_open:
      self.PatchObject(timeout_util, 'WaitForTreeStatus',
                       return_value=constants.TREE_OPEN, autospec=True)
    else:
      self.PatchObject(timeout_util, 'WaitForTreeStatus',
                       side_effect=timeout_util.TimeoutError())

    exit_it = itertools.chain([False] * runs, itertools.repeat(True))
    self.PatchObject(validation_pool.ValidationPool, 'ShouldExitEarly',
                     side_effect=exit_it)
    self.sync_stage.PerformStage()

  def ReloadPool(self):
    """Save the pool to disk and reload it."""
    with tempfile.NamedTemporaryFile() as f:
      cPickle.dump(self.sync_stage.pool, f)
      f.flush()
      self._run.options.validation_pool = f.name
      self.sync_stage = sync_stages.CommitQueueSyncStage(self._run)
      self.sync_stage.HandleSkip()


class SlaveCQSyncTest(BaseCQTest):
  """Tests the CommitQueueSync stage for the paladin slaves."""
  BOT_ID = 'x86-alex-paladin'

  def testReload(self):
    """Test basic ability to sync and reload the patches from disk."""
    self.PatchObject(lkgm_manager.LKGMManager, 'GetLatestCandidate',
                     return_value=self.manifest_path, autospec=True)
    self.sync_stage.PerformStage()
    self.ReloadPool()


class MasterCQSyncTest(BaseCQTest):
  """Tests the CommitQueueSync stage for the paladin masters.

  Tests in this class should apply both to the paladin masters and to the
  Pre-CQ Launcher.
  """
  BOT_ID = 'master-paladin'

  def setUp(self):
    """Setup patchers for specified bot id."""
    self.AutoPatch([[validation_pool.ValidationPool, 'ApplyPoolIntoRepo']])
    self.PatchObject(lkgm_manager.LKGMManager, 'CreateNewCandidate',
                     return_value=self.manifest_path, autospec=True)
    self.PatchObject(lkgm_manager.LKGMManager, 'CreateFromManifest',
                     return_value=self.manifest_path, autospec=True)

  def testCommitNonManifestChange(self, **kwargs):
    """Test the commit of a non-manifest change."""
    # Setting tracking_branch=foo makes this a non-manifest change.
    kwargs.setdefault('committed', True)
    self.PerformSync(tracking_branch='foo', **kwargs)

  def testFailedCommitOfNonManifestChange(self):
    """Test that the commit of a non-manifest change fails."""
    self.testCommitNonManifestChange(committed=False)

  def testCommitManifestChange(self, **kwargs):
    """Test committing a change to a project that's part of the manifest."""
    self.PatchObject(validation_pool.ValidationPool, '_FilterNonCrosProjects',
                     side_effect=lambda x, _: (x, []))
    self.PerformSync(**kwargs)

  def testDefaultSync(self):
    """Test basic ability to sync with standard options."""
    self.PerformSync()


class ExtendedMasterCQSyncTest(MasterCQSyncTest):
  """Additional tests for the CommitQueueSync stage.

  These only apply to the paladin master and not to any other stages.
  """

  def testReload(self):
    """Test basic ability to sync and reload the patches from disk."""
    # Use zero patches because MockPatches can't be pickled.
    self.PerformSync(num_patches=0, runs=0)
    self.ReloadPool()

  def testTreeClosureBlocksCommit(self):
    """Test that tree closures block commits."""
    self.assertRaises(SystemExit, self.testCommitNonManifestChange,
                      tree_open=False)

  def testTreeThrottleUsesAlternateGerritQuery(self):
    """Test that if the tree is throttled, we use an alternate gerrit query."""
    self.PerformSync(tree_throttled=True)
    gerrit.GerritHelper.Query.assert_called_with(
        mock.ANY, constants.THROTTLED_CQ_READY_QUERY,
        sort='lastUpdated')


class CLStatusMock(partial_mock.PartialMock):
  """Partial mock for CLStatus methods in ValidationPool."""

  TARGET = 'chromite.cbuildbot.validation_pool.ValidationPool'
  ATTRS = ('GetCLStatus', 'GetCLStatusCount', 'UpdateCLStatus',)

  def __init__(self, treat_launching_as_inflight=False):
    """CLStatusMock constructor.

    Args:
      treat_launching_as_inflight: When getting a CL's status via
        GetCLStatus, treat any change with status LAUNCHING as if
        it has status INFLIGHT. This simulates pre-cq tryjobs getting
        immediately launched. Default: False.
    """
    partial_mock.PartialMock.__init__(self)
    self.calls = {}
    self.status = {}
    self.status_count = {}
    self._treat_launching_as_inflight = treat_launching_as_inflight

  def GetCLStatus(self, _bot, change):
    status = self.status.get(change)
    if (self._treat_launching_as_inflight and
        status == validation_pool.ValidationPool.STATUS_LAUNCHING):
      return validation_pool.ValidationPool.STATUS_INFLIGHT
    return status

  def GetCLStatusCount(self, _bot, change, count, latest_patchset_only=True):
    # pylint: disable=W0613
    return self.status_count.get(change, 0)

  def UpdateCLStatus(self, _bot, change, status, dry_run):
    # pylint: disable=W0613
    self.calls[status] = self.calls.get(status, 0) + 1
    self.status[change] = status
    self.status_count[change] = self.status_count.get(change, 0) + 1


class PreCQLauncherStageTest(MasterCQSyncTest):
  """Tests for the PreCQLauncherStage."""
  BOT_ID = 'pre-cq-launcher'
  STATUS_LAUNCHING = validation_pool.ValidationPool.STATUS_LAUNCHING
  STATUS_WAITING = validation_pool.ValidationPool.STATUS_WAITING
  STATUS_FAILED = validation_pool.ValidationPool.STATUS_FAILED

  def setUp(self):
    self.PatchObject(time, 'sleep', autospec=True)

  def _PrepareValidationPoolMock(self, auto_launch=False):
    # pylint: disable-msg=W0201
    self.pre_cq = CLStatusMock(treat_launching_as_inflight=auto_launch)
    self.StartPatcher(self.pre_cq)

  def _Prepare(self, bot_id=None, **kwargs):
    super(PreCQLauncherStageTest, self)._Prepare(bot_id, **kwargs)

    self.sync_stage = sync_stages.PreCQLauncherStage(self._run)

  def testTreeClosureIsOK(self):
    """Test that tree closures block commits."""
    self._PrepareValidationPoolMock()
    self.testCommitNonManifestChange(tree_open=False)

  def testLaunchTrybot(self):
    """Test launching a trybot."""
    self._PrepareValidationPoolMock()
    self.testCommitManifestChange()
    self.assertEqual(self.pre_cq.status.values(), [self.STATUS_LAUNCHING])
    self.assertEqual(self.pre_cq.calls.keys(), [self.STATUS_LAUNCHING])

  def runTrybotTest(self, launching, waiting, failed, runs):
    """Helper function for testing PreCQLauncher.

    Create a mock patch to be picked up by the PreCQ. Allow up to
    |runs|+1 calls to ProcessChanges. Assert that the patch received status
    LAUNCHING, WAITING, and FAILED |launching|, |waiting|, and |failed| times
    respectively.
    """
    self.testCommitManifestChange(runs=runs)
    self.assertEqual(self.pre_cq.calls.get(self.STATUS_LAUNCHING, 0), launching)
    self.assertEqual(self.pre_cq.calls.get(self.STATUS_WAITING, 0), waiting)
    self.assertEqual(self.pre_cq.calls.get(self.STATUS_FAILED, 0), failed)

  def testLaunchTrybotTimesOutOnce(self):
    """Test what happens when a trybot launch times out."""
    self._PrepareValidationPoolMock()
    it = itertools.chain([True], itertools.repeat(False))
    self.PatchObject(sync_stages.PreCQLauncherStage, '_HasLaunchTimedOut',
                     side_effect=it)
    self.runTrybotTest(launching=2, waiting=1, failed=0, runs=3)

  def testLaunchTrybotTimesOutTwice(self):
    """Test what happens when a trybot launch times out."""
    self._PrepareValidationPoolMock()
    self.PatchObject(sync_stages.PreCQLauncherStage, '_HasLaunchTimedOut',
                     return_value=True)
    self.runTrybotTest(launching=2, waiting=1, failed=1, runs=3)

  def testInflightTrybotTimesOutOnce(self):
    """Test what happens when an inflight trybot times out."""
    self._PrepareValidationPoolMock(auto_launch=True)
    it = itertools.chain([True], itertools.repeat(False))
    self.PatchObject(sync_stages.PreCQLauncherStage, '_HasInflightTimedOut',
                     side_effect=it)
    self.runTrybotTest(launching=1, waiting=0, failed=1, runs=1)


if __name__ == '__main__':
  cros_test_lib.main()
