#!/usr/bin/python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for sync stages."""

from __future__ import print_function

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
from chromite.cbuildbot import tree_status
from chromite.cbuildbot import validation_pool
from chromite.cbuildbot.stages import sync_stages
from chromite.cbuildbot.stages import generic_stages_unittest
from chromite.lib import cros_build_lib_unittest
from chromite.lib import cros_test_lib
from chromite.lib import cidb
from chromite.lib import fake_cidb
from chromite.lib import gerrit
from chromite.lib import git_unittest
from chromite.lib import gob_util
from chromite.lib import osutils
from chromite.lib import timeout_util


# TODO(build): Finish test wrapper (http://crosbug.com/37517).
# Until then, this has to be after the chromite imports.
import mock

MOCK_BUILD_ID = 69178

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
      repo, self.manifest_version_url, [self.build_name], self.incr_type,
      force=False, branch=self.branch, dry_run=True)

    self._Prepare()

  def _Prepare(self, bot_id=None, **kwargs):
    super(ManifestVersionedSyncStageTest, self)._Prepare(bot_id, **kwargs)

    self._run.config['manifest_version'] = self.manifest_version_url
    self.sync_stage = sync_stages.ManifestVersionedSyncStage(self._run)
    self.sync_stage.manifest_manager = self.manager
    self._run.attrs.manifest_manager = self.manager
    self._run.attrs.metadata.UpdateWithDict({'build_id': MOCK_BUILD_ID})

  def testManifestVersionedSyncOnePartBranch(self):
    """Tests basic ManifestVersionedSyncStage with branch ooga_booga"""
    # pylint: disable=E1120
    self.mox.StubOutWithMock(sync_stages.ManifestVersionedSyncStage,
                             'Initialize')
    self.mox.StubOutWithMock(sync_stages.ManifestVersionedSyncStage,
                             '_SetChromeVersionIfApplicable')
    self.mox.StubOutWithMock(manifest_version.BuildSpecsManager,
                             'GetNextBuildSpec')
    self.mox.StubOutWithMock(manifest_version.BuildSpecsManager,
                             'GetLatestPassingSpec')
    self.mox.StubOutWithMock(sync_stages.SyncStage, 'ManifestCheckout')

    sync_stages.ManifestVersionedSyncStage.Initialize()
    self.manager.GetNextBuildSpec(
        build_id=MOCK_BUILD_ID,
        dashboard_url=self.sync_stage.ConstructDashboardURL()
        ).AndReturn(self.next_version)
    self.manager.GetLatestPassingSpec().AndReturn(None)

    sync_stages.ManifestVersionedSyncStage._SetChromeVersionIfApplicable(
        self.next_version)
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


class BaseCQTestCase(generic_stages_unittest.StageTest):
  """Helper class for testing the CommitQueueSync stage"""
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

    # Create and set up a fake cidb instance.
    self.fake_db = fake_cidb.FakeCIDBConnection()
    cidb.CIDBConnectionFactory.SetupMockCidb(self.fake_db)

    self.sync_stage = None
    self._Prepare()

  def tearDown(self):
    cidb.CIDBConnectionFactory.ClearMock()

  def _Prepare(self, bot_id=None, **kwargs):
    super(BaseCQTestCase, self)._Prepare(bot_id, **kwargs)

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

    Returns:
      A list of MockPatch objects which were created and used in PerformSync.
    """
    p = MockPatch(remote=remote, tracking_branch=tracking_branch)
    my_patches = [p] * num_patches
    self.PatchObject(gerrit.GerritHelper, 'IsChangeCommitted',
                     return_value=committed, autospec=True)
    self.PatchObject(gerrit.GerritHelper, 'Query',
                     return_value=my_patches, autospec=True)
    if tree_throttled:
      self.PatchObject(tree_status, 'WaitForTreeStatus',
                       return_value=constants.TREE_THROTTLED, autospec=True)
    elif tree_open:
      self.PatchObject(tree_status, 'WaitForTreeStatus',
                       return_value=constants.TREE_OPEN, autospec=True)
    else:
      self.PatchObject(tree_status, 'WaitForTreeStatus',
                       side_effect=timeout_util.TimeoutError())

    exit_it = itertools.chain([False] * runs, itertools.repeat(True))
    self.PatchObject(validation_pool.ValidationPool, 'ShouldExitEarly',
                     side_effect=exit_it)
    self.sync_stage.PerformStage()

    return my_patches

  def ReloadPool(self):
    """Save the pool to disk and reload it."""
    with tempfile.NamedTemporaryFile() as f:
      cPickle.dump(self.sync_stage.pool, f)
      f.flush()
      self._run.options.validation_pool = f.name
      self.sync_stage = sync_stages.CommitQueueSyncStage(self._run)
      self.sync_stage.HandleSkip()


class SlaveCQSyncTest(BaseCQTestCase):
  """Tests the CommitQueueSync stage for the paladin slaves."""
  BOT_ID = 'x86-alex-paladin'

  def testReload(self):
    """Test basic ability to sync and reload the patches from disk."""
    self.PatchObject(lkgm_manager.LKGMManager, 'GetLatestCandidate',
                     return_value=self.manifest_path, autospec=True)
    self.sync_stage.PerformStage()
    self.ReloadPool()


class MasterCQSyncTestCase(BaseCQTestCase):
  """Helper class for testing the CommitQueueSync stage masters."""

  BOT_ID = 'master-paladin'

  def setUp(self):
    """Setup patchers for specified bot id."""
    self.AutoPatch([[validation_pool.ValidationPool, 'ApplyPoolIntoRepo']])
    self.PatchObject(lkgm_manager.LKGMManager, 'CreateNewCandidate',
                     return_value=self.manifest_path, autospec=True)
    self.PatchObject(lkgm_manager.LKGMManager, 'CreateFromManifest',
                     return_value=self.manifest_path, autospec=True)

  def _testCommitNonManifestChange(self, **kwargs):
    """Test the commit of a non-manifest change.

    Returns:
      List of MockPatch objects that were used in PerformSync
    """
    # Setting tracking_branch=foo makes this a non-manifest change.
    kwargs.setdefault('committed', True)
    return self.PerformSync(tracking_branch='foo', **kwargs)

  def _testFailedCommitOfNonManifestChange(self):
    """Test that the commit of a non-manifest change fails.

    Returns:
      List of MockPatch objects that were used in PerformSync
    """
    return self._testCommitNonManifestChange(committed=False)

  def _testCommitManifestChange(self, **kwargs):
    """Test committing a change to a project that's part of the manifest.

    Returns:
      List of MockPatch objects that were used in PerformSync
    """
    self.PatchObject(validation_pool.ValidationPool, '_FilterNonCrosProjects',
                     side_effect=lambda x, _: (x, []))
    return self.PerformSync(**kwargs)

  def _testDefaultSync(self):
    """Test basic ability to sync with standard options.

    Returns:
      List of MockPatch objects that were used in PerformSync
    """
    return self.PerformSync()


class MasterCQSyncTest(MasterCQSyncTestCase):
  """Tests the CommitQueueSync stage for the paladin masters."""

  def testCommitNonManifestChange(self):
    """See MasterCQSyncTestCase"""
    return self._testCommitNonManifestChange()

  def testFailedCommitOfNonManifestChange(self):
    """See MasterCQSyncTestCase"""
    return self._testFailedCommitOfNonManifestChange()

  def testCommitManifestChange(self):
    """See MasterCQSyncTestCase"""
    return self._testCommitManifestChange()

  def testDefaultSync(self):
    """See MasterCQSyncTestCase"""
    return self._testDefaultSync()

  def testReload(self):
    """Test basic ability to sync and reload the patches from disk."""
    # Use zero patches because MockPatches can't be pickled.
    self.PerformSync(num_patches=0, runs=0)
    self.ReloadPool()

  def testTreeClosureBlocksCommit(self):
    """Test that tree closures block commits."""
    self.assertRaises(SystemExit, self._testCommitNonManifestChange,
                      tree_open=False)

  def testTreeThrottleUsesAlternateGerritQuery(self):
    """Test that if the tree is throttled, we use an alternate gerrit query."""
    self.PerformSync(tree_throttled=True)
    gerrit.GerritHelper.Query.assert_called_with(
        mock.ANY, constants.THROTTLED_CQ_READY_QUERY,
        sort='lastUpdated')

class PreCQLauncherStageTest(MasterCQSyncTestCase):
  """Tests for the PreCQLauncherStage."""

  BOT_ID = constants.PRE_CQ_LAUNCHER_CONFIG
  STATUS_LAUNCHING = validation_pool.ValidationPool.STATUS_LAUNCHING
  STATUS_WAITING = validation_pool.ValidationPool.STATUS_WAITING
  STATUS_FAILED = validation_pool.ValidationPool.STATUS_FAILED
  STATUS_READY_TO_SUBMIT = validation_pool.ValidationPool.STATUS_READY_TO_SUBMIT
  STATUS_INFLIGHT = validation_pool.ValidationPool.STATUS_INFLIGHT

  def setUp(self):
    self.PatchObject(time, 'sleep', autospec=True)

  def _PrepareAutoLaunch(self):
    """Cause CLs with launching status to be automatically launched."""
    # Mock out UpdateCLPreCQStatus so that when a "Launching" action is
    # recorded, automatically pretend to start a new build which records
    # an "Inflight" action for the same change.
    original_method = validation_pool.ValidationPool.UpdateCLPreCQStatus

    def new_method(change, status, build_id):
      original_method(change, status, build_id)
      if (status == self.STATUS_LAUNCHING):
        new_build_id = self.fake_db.InsertBuild('Pre cq group',
                                                constants.WATERFALL_TRYBOT,
                                                1,
                                                constants.PRE_CQ_GROUP_CONFIG,
                                                'bot-hostname')
        original_method(change, self.STATUS_INFLIGHT,
                        new_build_id)

    self.PatchObject(validation_pool.ValidationPool, 'UpdateCLPreCQStatus',
                     side_effect=new_method)


  def _Prepare(self, bot_id=None, **kwargs):
    build_id = self.fake_db.InsertBuild(
        constants.PRE_CQ_LAUNCHER_NAME, constants.WATERFALL_INTERNAL, 1,
        constants.PRE_CQ_LAUNCHER_CONFIG, 'bot-hostname')

    super(PreCQLauncherStageTest, self)._Prepare(
        bot_id, build_id=build_id, **kwargs)

    self.sync_stage = sync_stages.PreCQLauncherStage(self._run)

  def testCommitNonManifestChange(self):
    """See MasterCQSyncTestCase"""
    self._testCommitNonManifestChange()

  def testFailedCommitOfNonManifestChange(self):
    """See MasterCQSyncTestCase"""
    self._testFailedCommitOfNonManifestChange()

  def testCommitManifestChange(self):
    """See MasterCQSyncTestCase"""
    self._testCommitManifestChange()

  def testDefaultSync(self):
    """See MasterCQSyncTestCase"""
    self._testDefaultSync()

  def testTreeClosureIsOK(self):
    """Test that tree closures block commits."""
    self._testCommitNonManifestChange(tree_open=False)

  def testLaunchTrybot(self):
    """Test launching a trybot."""
    change = self._testCommitManifestChange()[0]

    self.assertEqual(validation_pool.ValidationPool.GetCLPreCQStatus(change),
                     self.STATUS_LAUNCHING)

  def testLaunchTrybotWithAutolaunch(self):
    """Test launching a trybot with auto-launch."""
    self._PrepareAutoLaunch()
    change = self._testCommitManifestChange()[0]

    self.assertEqual(validation_pool.ValidationPool.GetCLPreCQStatus(change),
                     self.STATUS_INFLIGHT)

  def runTrybotTest(self, launching=0, waiting=0, failed=0, runs=0):
    """Helper function for testing PreCQLauncher.

    Create a mock patch to be picked up by the PreCQ. Allow up to
    |runs|+1 calls to ProcessChanges. Assert that the patch received status
    LAUNCHING, WAITING, and FAILED |launching|, |waiting|, and |failed| times
    respectively.
    """
    change = self._testCommitManifestChange(runs=runs)[0]
    # Count the number of recorded actions corresponding to launching, watiting,
    # and failed, and ensure they are correct.
    validation_pool.ValidationPool.ClearActionCache()

    expected = (launching, waiting, failed)
    actions = (constants.CL_ACTION_PRE_CQ_LAUNCHING,
               constants.CL_ACTION_PRE_CQ_WAITING,
               constants.CL_ACTION_PRE_CQ_FAILED)

    for exp, action in zip(expected, actions):
      self.assertEqual(
          exp,
          validation_pool.ValidationPool.GetCLActionCount(
              change, [constants.PRE_CQ_LAUNCHER_CONFIG], action))

  def testLaunchTrybotTimesOutOnce(self):
    """Test what happens when a trybot launch times out."""
    it = itertools.chain([True], itertools.repeat(False))
    self.PatchObject(sync_stages.PreCQLauncherStage, '_HasLaunchTimedOut',
                     side_effect=it)
    self.runTrybotTest(launching=2, waiting=1, failed=0, runs=3)

  def testLaunchTrybotTimesOutTwice(self):
    """Test what happens when a trybot launch times out."""
    self.PatchObject(sync_stages.PreCQLauncherStage, '_HasLaunchTimedOut',
                     return_value=True)
    self.runTrybotTest(launching=2, waiting=1, failed=1, runs=3)

  def testInflightTrybotTimesOutOnce(self):
    """Test what happens when an inflight trybot times out."""
    self._PrepareAutoLaunch()
    it = itertools.chain([True], itertools.repeat(False))
    self.PatchObject(sync_stages.PreCQLauncherStage, '_HasInflightTimedOut',
                     side_effect=it)
    self.runTrybotTest(launching=1, waiting=0, failed=1, runs=1)

  def testSubmit(self):
    """Test submission of patches."""
    self.PatchObject(validation_pool.ValidationPool, 'GetCLPreCQStatus',
                     return_value=self.STATUS_READY_TO_SUBMIT)
    m = self.PatchObject(validation_pool.ValidationPool, 'SubmitChanges')
    self.runTrybotTest(runs=1)
    m.assert_called_with([mock.ANY], check_tree_open=False)


if __name__ == '__main__':
  cros_test_lib.main()
