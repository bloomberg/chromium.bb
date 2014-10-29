#!/usr/bin/python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for sync stages."""

from __future__ import print_function

import cPickle
import datetime
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
from chromite.lib import clactions
from chromite.lib import fake_cidb
from chromite.lib import gerrit
from chromite.lib import git_unittest
from chromite.lib import gob_util
from chromite.lib import osutils
from chromite.lib import patch as cros_patch
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
  remote = 'cros'

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
    self._run.config.overlays = constants.PUBLIC_OVERLAYS
    self.sync_stage = sync_stages.CommitQueueSyncStage(self._run)

  def PerformSync(self, committed=False, num_patches=1, tree_open=True,
                  tree_throttled=False,
                  pre_cq_status=constants.CL_STATUS_PASSED,
                  runs=0, changes=None, patch_objects=True,
                  **kwargs):
    """Helper to perform a basic sync for master commit queue.

    Args:
      committed: Value to be returned by mock patches' IsChangeCommitted.
                 Default: False.
      num_patches: The number of mock patches to create. Default: 1.
      tree_open: If True, behave as if tree is open. Default: True.
      tree_throttled: If True, behave as if tree is throttled
                      (overriding the tree_open arg). Default: False.
      pre_cq_status: PreCQ status for mock patches. Default: passed.
      runs: The maximum number of times to allow validation_pool.AcquirePool
            to wait for additional changes. runs=0 means never wait for
            additional changes. Default: 0.
      changes: Optional list of MockPatch instances that should be available
               in validation pool. If not specified, a set of |num_patches|
               patches will be created.
      patch_objects: If your test will call PerformSync more than once, set
                     this to false on subsequent calls to ensure that we do
                     not re-patch already patched methods with mocks.
      **kwargs: Additional arguments to pass to MockPatch when creating patches.

    Returns:
      A list of MockPatch objects which were created and used in PerformSync.
    """
    kwargs.setdefault('approval_timestamp',
        time.time() - sync_stages.PreCQLauncherStage.LAUNCH_DELAY * 60)
    changes = changes or [MockPatch(**kwargs)] * num_patches
    if pre_cq_status is not None:
      new_build_id = self.fake_db.InsertBuild('Pre cq group',
                                              constants.WATERFALL_TRYBOT,
                                              1,
                                              constants.PRE_CQ_GROUP_CONFIG,
                                              'bot-hostname')
      for change in changes:
        action = clactions.TranslatePreCQStatusToAction(pre_cq_status)
        self.fake_db.InsertCLActions(
            new_build_id,
            [clactions.CLAction.FromGerritPatchAndAction(change, action)])

    if patch_objects:
      self.PatchObject(gerrit.GerritHelper, 'IsChangeCommitted',
                       return_value=committed, autospec=True)
      # Validation pool will mutate the return value it receives from
      # Query, therefore return a copy of the changes list.
      # pylint: disable-msg=W0613
      def Query(*args, **kwargs):
        return list(changes)
      self.PatchObject(gerrit.GerritHelper, 'Query',
                       side_effect=Query, autospec=True)
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

    return changes

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
    kwargs.setdefault('tracking_branch', 'foo')
    return self.PerformSync(**kwargs)

  def _testFailedCommitOfNonManifestChange(self):
    """Test what happens when the commit of a non-manifest change fails.

    Returns:
      List of MockPatch objects that were used in PerformSync
    """
    return self._testCommitNonManifestChange(committed=False)

  def _testCommitManifestChange(self, changes=None, **kwargs):
    """Test committing a change to a project that's part of the manifest.

    Args:
      changes: Optional list of MockPatch instances to use in PerformSync.

    Returns:
      List of MockPatch objects that were used in PerformSync
    """
    self.PatchObject(validation_pool.ValidationPool, '_FilterNonCrosProjects',
                     side_effect=lambda x, _: (x, []))
    return self.PerformSync(changes=changes, **kwargs)

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
    changes = self._testCommitNonManifestChange()
    self.assertItemsEqual(self.sync_stage.pool.changes, changes)
    self.assertItemsEqual(self.sync_stage.pool.non_manifest_changes, [])

  def testFailedCommitOfNonManifestChange(self):
    """See MasterCQSyncTestCase"""
    changes = self._testFailedCommitOfNonManifestChange()
    self.assertItemsEqual(self.sync_stage.pool.changes, changes)
    self.assertItemsEqual(self.sync_stage.pool.non_manifest_changes, [])

  def testCommitManifestChange(self):
    """See MasterCQSyncTestCase"""
    changes = self._testCommitManifestChange()
    self.assertItemsEqual(self.sync_stage.pool.changes, changes)
    self.assertItemsEqual(self.sync_stage.pool.non_manifest_changes, [])

  def testCommitManifestChangeWithoutPreCQ(self):
    """Changes get ignored if they aren't approved by pre-cq."""
    self._testCommitManifestChange(pre_cq_status=None)
    self.assertItemsEqual(self.sync_stage.pool.changes, [])
    self.assertItemsEqual(self.sync_stage.pool.non_manifest_changes, [])

  def testCommitManifestChangeWithoutPreCQAndOldPatches(self):
    """Changes get tested without pre-cq if the approval_timestamp is old."""
    changes = self._testCommitManifestChange(pre_cq_status=None,
                                             approval_timestamp=0)
    self.assertItemsEqual(self.sync_stage.pool.changes, changes)
    self.assertItemsEqual(self.sync_stage.pool.non_manifest_changes, [])

  def testDefaultSync(self):
    """See MasterCQSyncTestCase"""
    changes = self._testDefaultSync()
    self.assertItemsEqual(self.sync_stage.pool.changes, changes)
    self.assertItemsEqual(self.sync_stage.pool.non_manifest_changes, [])

  def testReload(self):
    """Test basic ability to sync and reload the patches from disk."""
    # Use zero patches because mock patches can't be pickled.
    changes = self.PerformSync(num_patches=0, runs=0)
    self.ReloadPool()
    self.assertItemsEqual(self.sync_stage.pool.changes, changes)
    self.assertItemsEqual(self.sync_stage.pool.non_manifest_changes, [])

  def testTreeClosureBlocksCommit(self):
    """Test that tree closures block commits."""
    self.assertRaises(SystemExit, self._testCommitNonManifestChange,
                      tree_open=False)

  def testTreeThrottleUsesAlternateGerritQuery(self):
    """Test that if the tree is throttled, we use an alternate gerrit query."""
    changes = self.PerformSync(tree_throttled=True)
    gerrit.GerritHelper.Query.assert_called_with(
        mock.ANY, constants.THROTTLED_CQ_READY_QUERY,
        sort='lastUpdated')
    self.assertItemsEqual(self.sync_stage.pool.changes, changes)
    self.assertItemsEqual(self.sync_stage.pool.non_manifest_changes, [])


class PreCQLauncherStageTest(MasterCQSyncTestCase):
  """Tests for the PreCQLauncherStage."""

  BOT_ID = constants.PRE_CQ_LAUNCHER_CONFIG
  STATUS_LAUNCHING = constants.CL_STATUS_LAUNCHING
  STATUS_WAITING = constants.CL_STATUS_WAITING
  STATUS_FAILED = constants.CL_STATUS_FAILED
  STATUS_READY_TO_SUBMIT = constants.CL_STATUS_READY_TO_SUBMIT
  STATUS_INFLIGHT = constants.CL_STATUS_INFLIGHT

  def setUp(self):
    self.PatchObject(time, 'sleep', autospec=True)

  def _Prepare(self, bot_id=None, **kwargs):
    build_id = self.fake_db.InsertBuild(
        constants.PRE_CQ_LAUNCHER_NAME, constants.WATERFALL_INTERNAL, 1,
        constants.PRE_CQ_LAUNCHER_CONFIG, 'bot-hostname')

    super(PreCQLauncherStageTest, self)._Prepare(
        bot_id, build_id=build_id, **kwargs)

    self.sync_stage = sync_stages.PreCQLauncherStage(self._run)


  def _PrepareChangesWithPendingVerifications(self, verifications=None):
    """Prepare changes and pending verifications for them.

    This helper creates changes in the validation pool, each of which
    require its own set of verifications.

    Args:
      verifications: A list of lists of configs. Each element in the
                     outer list corresponds to a different CL. Defaults
                     to [[constants.PRE_CQ_GROUP_CONFIG]]

    Returns:
      A list of len(verifications) MockPatch instances.
    """
    verifications = verifications or [[constants.PRE_CQ_GROUP_CONFIG]]
    changes = [MockPatch(gerrit_number=n) for n in range(len(verifications))]
    changes_to_verifications = {c: v for c, v in zip(changes, verifications)}

    def VerificationsForChange(change):
      return changes_to_verifications.get(change) or []

    self.PatchObject(sync_stages.PreCQLauncherStage,
                     'VerificationsForChange',
                     side_effect=VerificationsForChange)
    return changes


  def _PrepareSubmittableChange(self):
    # Create a pre-cq submittable change, let it be screened,
    # and have the trybot mark it as verified.
    change = self._PrepareChangesWithPendingVerifications()[0]
    self.PatchObject(sync_stages.PreCQLauncherStage,
                     'CanSubmitChangeInPreCQ',
                     return_value=True)
    change[0].approval_timestamp = 0
    self.PerformSync(pre_cq_status=None, changes=[change],
                     runs=2)

    config_name = constants.PRE_CQ_GROUP_CONFIG

    build_id = self.fake_db.InsertBuild(
        'builder name', constants.WATERFALL_TRYBOT, 2, config_name,
        'bot hostname')
    self.fake_db.InsertCLActions(
      build_id,
      [clactions.CLAction.FromGerritPatchAndAction(
          change, constants.CL_ACTION_VERIFIED)])
    return change

  def testSubmitInPreCQ(self):
    change = self._PrepareSubmittableChange()

    # Change should be submitted by the pre-cq-launcher.
    m = self.PatchObject(validation_pool.ValidationPool, 'SubmitChanges')
    self.PerformSync(pre_cq_status=None, changes=[change], patch_objects=False)
    m.assert_called_with(set([change]), check_tree_open=False)


  def testSubmitUnableInPreCQ(self):
    change = self._PrepareSubmittableChange()

    # Change should throw a DependencyError when trying to create a transaction
    e = cros_patch.DependencyError(change, cros_patch.PatchException(change))
    self.PatchObject(validation_pool.PatchSeries, 'CreateTransaction',
                     side_effect=e)
    self.PerformSync(pre_cq_status=None, changes=[change], patch_objects=False)
    # Change should be marked as pre-cq passed, rather than being submitted.
    self.assertEqual(constants.CL_STATUS_PASSED, self._GetPreCQStatus(change))


  def testPreCQ(self):
    changes = self._PrepareChangesWithPendingVerifications(
        [['banana'], ['orange', 'apple']])
    # After 2 runs, the changes should be screened but not
    # yet launched (due to pre-launch timeout).
    for c in changes:
      c.approval_timestamp = time.time()
    self.PerformSync(pre_cq_status=None, changes=changes, runs=2)

    def assertAllStatuses(progress_map, status):
      for change in changes:
        for config in progress_map[change]:
          self.assertEqual(progress_map[change][config][0], status)

    action_history = self.fake_db.GetActionsForChanges(changes)
    progress_map = clactions.GetPreCQProgressMap(changes, action_history)
    assertAllStatuses(progress_map, constants.CL_PRECQ_CONFIG_STATUS_PENDING)

    self.assertEqual(1, len(progress_map[changes[0]]))
    self.assertEqual(2, len(progress_map[changes[1]]))

    # Fake that launch delay has expired by changing change approval times.
    for c in changes:
      c.approval_timestamp = 0

    # After 1 more Sync all configs for all changes should be launched.
    self.PerformSync(pre_cq_status=None, changes=changes, patch_objects=False)

    action_history = self.fake_db.GetActionsForChanges(changes)
    progress_map = clactions.GetPreCQProgressMap(changes, action_history)
    assertAllStatuses(progress_map, constants.CL_PRECQ_CONFIG_STATUS_LAUNCHED)

    # Fake all these tryjobs starting
    build_ids = self._FakeLaunchTryjobs(progress_map)

    # After 1 more Sync all configs should now be inflight.
    self.PerformSync(pre_cq_status=None, changes=changes, patch_objects=False)
    action_history = self.fake_db.GetActionsForChanges(changes)
    progress_map = clactions.GetPreCQProgressMap(changes, action_history)
    assertAllStatuses(progress_map, constants.CL_PRECQ_CONFIG_STATUS_INFLIGHT)

    # Fake INFLIGHT_TIMEOUT+1 passing with banana and orange config succeeding,
    # and apple never launching. The first change should pass the pre-cq, the
    # second should fail due to inflight timeout.
    fake_time = datetime.datetime.now() + datetime.timedelta(
        minutes=sync_stages.PreCQLauncherStage.INFLIGHT_TIMEOUT + 1)
    self.fake_db.SetTime(fake_time)
    self.fake_db.InsertCLActions(
        build_ids['banana'],
        [clactions.CLAction.FromGerritPatchAndAction(
            changes[0], constants.CL_ACTION_VERIFIED)])
    self.fake_db.InsertCLActions(
        build_ids['orange'],
        [clactions.CLAction.FromGerritPatchAndAction(
        changes[1], constants.CL_ACTION_VERIFIED)])

    self.PerformSync(pre_cq_status=None, changes=changes, patch_objects=False)
    self.assertEqual(self._GetPreCQStatus(changes[0]),
                     constants.CL_STATUS_PASSED)
    self.assertEqual(self._GetPreCQStatus(changes[1]),
                     constants.CL_STATUS_FAILED)


  def _FakeLaunchTryjobs(self, progress_map):
    """Pretend to start all launched tryjobs."""
    build_ids_per_config = {}
    for change, change_status_dict in progress_map.iteritems():
      for config, (status, _) in change_status_dict.iteritems():
        if status == constants.CL_PRECQ_CONFIG_STATUS_LAUNCHED:
          if not config in build_ids_per_config:
            build_ids_per_config[config] = self.fake_db.InsertBuild(
                config, constants.WATERFALL_TRYBOT, 1, config, config)
          self.fake_db.InsertCLActions(
              build_ids_per_config[config],
              [clactions.CLAction.FromGerritPatchAndAction(
                  change, constants.CL_ACTION_PICKED_UP)])
    return build_ids_per_config


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

  def _GetPreCQStatus(self, change):
    """Helper method to get pre-cq status of a CL from fake_db."""
    action_history = self.fake_db.GetActionsForChanges([change])
    return clactions.GetCLPreCQStatus(change, action_history)

  def testRequeued(self):
    """Test that a previously rejected patch gets marked as requeued."""
    p = MockPatch()
    previous_build_id = self.fake_db.InsertBuild(
        'some name', constants.WATERFALL_TRYBOT, 1, 'some_config',
        'some_hostname')
    action = clactions.CLAction.FromGerritPatchAndAction(
        p, constants.CL_ACTION_KICKED_OUT)
    self.fake_db.InsertCLActions(previous_build_id, [action])

    self.PerformSync(changes=[p])
    actions_for_patch = self.fake_db.GetActionsForChanges([p])
    requeued_actions = [a for a in actions_for_patch
                        if a.action == constants.CL_ACTION_REQUEUED]
    self.assertEqual(1, len(requeued_actions))


if __name__ == '__main__':
  cros_test_lib.main()
