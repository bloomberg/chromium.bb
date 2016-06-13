# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for sync stages."""

from __future__ import print_function

import cPickle
import datetime
import itertools
import mock
import os
import shutil
import time
import tempfile

from chromite.cbuildbot import chromeos_config
from chromite.cbuildbot import constants
from chromite.cbuildbot import failures_lib
from chromite.cbuildbot import lkgm_manager
from chromite.cbuildbot import manifest_version
from chromite.cbuildbot import manifest_version_unittest
from chromite.cbuildbot import metadata_lib
from chromite.cbuildbot import repository
from chromite.cbuildbot import tree_status
from chromite.cbuildbot import triage_lib
from chromite.cbuildbot import trybot_patch_pool
from chromite.cbuildbot import validation_pool
from chromite.cbuildbot.stages import generic_stages_unittest
from chromite.cbuildbot.stages import sync_stages
from chromite.lib import cidb
from chromite.lib import clactions
from chromite.lib import cros_build_lib
from chromite.lib import cros_build_lib_unittest
from chromite.lib import fake_cidb
from chromite.lib import gerrit
from chromite.lib import git
from chromite.lib import git_unittest
from chromite.lib import gob_util
from chromite.lib import osutils
from chromite.lib import patch as cros_patch
from chromite.lib import timeout_util

# It's normal for unittests to access protected members.
# pylint: disable=protected-access


class BootstrapStageTest(
    generic_stages_unittest.AbstractStageTestCase,
    cros_build_lib_unittest.RunCommandTestCase):
  """Tests the Bootstrap stage."""

  BOT_ID = 'sync-test-cbuildbot'
  RELEASE_TAG = ''

  def setUp(self):
    # Pretend API version is always current.
    self.PatchObject(cros_build_lib, 'GetTargetChromiteApiVersion',
                     return_value=(constants.REEXEC_API_MAJOR,
                                   constants.REEXEC_API_MINOR))

    self._Prepare()

  def ConstructStage(self):
    patch_pool = trybot_patch_pool.TrybotPatchPool()
    return sync_stages.BootstrapStage(self._run, patch_pool)

  def testSimpleBootstrap(self):
    """Verify Bootstrap behavior in a simple case (with a branch)."""

    self.RunStage()

    # Clone next chromite checkout.
    self.assertCommandContains([
        'git', 'clone', constants.CHROMITE_URL,
        mock.ANY,  # Can't predict new chromium checkout diretory.
        '--reference', mock.ANY
    ])

    # Switch to the test branch.
    self.assertCommandContains(['git', 'checkout', 'ooga_booga'])

    # Re-exec cbuildbot. We mostly only want to test the CL options Bootstrap
    # changes.
    #   '--sourceroot=%s'
    #   '--test-bootstrap'
    #   '--nobootstrap'
    #   '--manifest-repo-url'
    self.assertCommandContains([
        'chromite/cbuildbot/cbuildbot', 'sync-test-cbuildbot',
        '-r', os.path.join(self.tempdir, 'buildroot'),
        '--buildbot', '--noprebuilts', '--buildnumber', '1234321',
        '--branch', 'ooga_booga',
        '--sourceroot', mock.ANY,
        '--nobootstrap',
    ])


  def testSiteConfigBootstrap(self):
    """Verify Bootstrap behavior, if config_repo is passed in."""

    # Set a new command line option to set the repo.
    self._run.options.config_repo = 'http://happy/config/repo'

    self.RunStage()

    # Clone next chromite.
    self.assertCommandContains([
        'git', 'clone', 'https://chromium.googlesource.com/chromiumos/chromite',
        mock.ANY, # Can't predict new chromium checkout diretory.
        '--reference', mock.ANY
    ])

    # Switch to the test branch.
    self.assertCommandContains(['git', 'checkout', 'ooga_booga'])

    # Clone the site config.
    self.assertCommandContains([
        'git', 'clone', 'http://happy/config/repo',
        mock.ANY, # Can't predict new chromium checkout diretory.
        '--reference', mock.ANY
    ])

    # Switch to the test branch.
    self.assertCommandContains(['git', 'checkout', 'ooga_booga'])

    # Re-exec cbuildbot. We mostly only want to test the CL options Bootstrap
    # changes.
    #   '--sourceroot=%s'
    #   '--test-bootstrap'
    #   '--nobootstrap'
    #   '--manifest-repo-url'
    self.assertCommandContains([
        'chromite/cbuildbot/cbuildbot', 'sync-test-cbuildbot',
        '-r', os.path.join(self.tempdir, 'buildroot'),
        '--buildbot', '--noprebuilts', '--buildnumber', '1234321',
        '--branch', 'ooga_booga',
        '--sourceroot', mock.ANY,
        '--nobootstrap',
    ])


class SyncStageRepoCacheTest(
    generic_stages_unittest.AbstractStageTestCase):
  """Tests the SyncStage base class."""
  # pylint: disable=abstract-method

  def setUp(self):
    self.cache_dir = os.path.join(self.tempdir, 'cache')
    self.sync_stage = None

  def _Prepare(self, root_populated, cache_arg, cache_populated, **kwargs):
    cmd_args = ['-r', self.build_root]

    if not root_populated:
      shutil.rmtree(os.path.join(self.build_root, '.repo'))

    if cache_arg:
      cmd_args += ['--repo-cache', self.cache_dir]

    if cache_populated:
      osutils.Touch(
          os.path.join(self.cache_dir, '.repo', 'contents'),
          makedirs=True)
      osutils.Touch(
          os.path.join(self.cache_dir, '.repo', 'nested', 'contents'),
          makedirs=True)
      os.symlink(
          'contents',
          os.path.join(self.cache_dir, '.repo', 'relative_symlink'))
      os.symlink(
          '/nonexistent',
          os.path.join(self.cache_dir, '.repo', 'broken_symlink'))

    super(SyncStageRepoCacheTest, self)._Prepare(cmd_args=cmd_args, **kwargs)
    self.sync_stage = sync_stages.SyncStage(self._run)

  def validateNoCache(self):
    # This file exists only we copied from the repo cache.
    self.assertFalse(os.path.exists(
        os.path.join(self.build_root, '.repo', 'contents')))

  def validateCache(self):
    # This file exists only we copied from the repo cache.
    contents = os.path.join(self.cache_dir, '.repo', 'contents')
    nested = os.path.join(self.cache_dir, '.repo', 'nested', 'contents')
    relative = os.path.join(self.cache_dir, '.repo', 'relative_symlink')
    broken = os.path.join(self.cache_dir, '.repo', 'broken_symlink')

    # Assert expected files exist.
    self.assertTrue(os.path.exists(contents))
    self.assertTrue(os.path.exists(nested))
    self.assertTrue(os.path.exists(relative))
    self.assertFalse(os.path.exists(broken))

    # Assert symlinks are still links.
    self.assertTrue(os.path.islink(relative))
    self.assertTrue(os.path.islink(broken))

    # Assert relative symlink is relative to the new location.
    self.assertEqual(os.path.realpath(relative), contents)

  def testInitializeRepoPopulatedNoCache(self):
    """Tests basic SyncStage repo cache initialization code."""
    self._Prepare(root_populated=True, cache_arg=False, cache_populated=False)
    self.sync_stage._InitializeRepo()
    self.validateNoCache()

  def testInitializeRepoNotPopulatedNoCache(self):
    """Tests basic SyncStage repo cache initialization code."""
    self._Prepare(root_populated=False, cache_arg=False, cache_populated=False)
    self.sync_stage._InitializeRepo()
    self.validateNoCache()

  def testInitializeRepoPopulatedCache(self):
    """Tests basic SyncStage repo cache initialization code."""
    self._Prepare(root_populated=True, cache_arg=True, cache_populated=True)
    self.sync_stage._InitializeRepo()
    self.validateNoCache()

  def testInitializeRepoNotPopulatedCache(self):
    """Tests basic SyncStage repo cache initialization code."""
    self._Prepare(root_populated=False, cache_arg=True, cache_populated=True)
    self.sync_stage._InitializeRepo()
    self.validateCache()

  def testInitializeRepoNotPopulatedEmptyCache(self):
    """Tests basic SyncStage repo cache initialization code."""
    self._Prepare(root_populated=False, cache_arg=True, cache_populated=False)
    self.sync_stage._InitializeRepo()
    self.validateNoCache()

class ManifestVersionedSyncStageTest(
    generic_stages_unittest.AbstractStageTestCase):
  """Tests the ManifestVersionedSync stage."""
  # pylint: disable=abstract-method

  def setUp(self):
    self.source_repo = 'ssh://source/repo'
    self.manifest_version_url = 'fake manifest url'
    self.branch = 'master'
    self.build_name = 'x86-generic'
    self.incr_type = 'branch'
    self.next_version = 'next_version'
    self.sync_stage = None
    self.PatchObject(manifest_version.BuildSpecsManager, 'SetInFlight')

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

  def testManifestVersionedSyncOnePartBranch(self):
    """Tests basic ManifestVersionedSyncStage with branch ooga_booga"""
    self.PatchObject(sync_stages.ManifestVersionedSyncStage, 'Initialize')
    self.PatchObject(sync_stages.ManifestVersionedSyncStage,
                     '_SetAndroidVersionIfApplicable')
    self.PatchObject(sync_stages.ManifestVersionedSyncStage,
                     '_SetChromeVersionIfApplicable')
    self.PatchObject(manifest_version.BuildSpecsManager, 'GetNextBuildSpec',
                     return_value=self.next_version)
    self.PatchObject(manifest_version.BuildSpecsManager, 'GetLatestPassingSpec')
    self.PatchObject(sync_stages.SyncStage, 'ManifestCheckout',
                     return_value=self.next_version)
    self.PatchObject(sync_stages.ManifestVersionedSyncStage,
                     '_GetMasterVersion', return_value='foo',
                     autospec=True)
    self.PatchObject(manifest_version.BuildSpecsManager, 'BootstrapFromVersion',
                     autospec=True)
    self.PatchObject(repository.RepoRepository, 'Sync', autospec=True)

    self.sync_stage.Run()


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
  mock_diff_status = {}

  def __init__(self, *args, **kwargs):
    super(MockPatch, self).__init__(*args, **kwargs)

    # Flags can vary per-patch.
    self.flags = {
        'CRVW': '2',
        'VRIF': '1',
        'COMR': '1',
    }

  def HasApproval(self, field, allowed):
    """Pretends the patch is good.

    Pretend the patch has all of the values listed in
    constants.DEFAULT_CQ_READY_FIELDS, but not any other fields.

    Args:
      field: The name of the field as a string. 'CRVW', etc.
      allowed: Value, or list of values that are acceptable expressed as
               strings.
    """
    flag_value = self.flags.get(field, 0)
    if isinstance(allowed, (tuple, list)):
      return flag_value in allowed
    else:
      return flag_value == allowed

  def IsDraft(self):
    """Return whether this patch is a draft patchset."""
    return self.current_patch_set['draft']

  def IsBeingMerged(self):
    """Return whether this patch is merged or in the middle of being merged."""
    return self.status in ('SUBMITTED', 'MERGED')

  def IsMergeable(self):
    """Default implementation of IsMergeable, stubbed out by some tests."""
    return True

  def GetDiffStatus(self, _):
    return self.mock_diff_status


class SyncStageTest(generic_stages_unittest.AbstractStageTestCase):
  """Tests the SyncStage."""

  def setUp(self):
    self._Prepare()

  def ConstructStage(self):
    return sync_stages.SyncStage(self._run)

  def testWriteChangesToMetadata(self):
    """Test whether WriteChangesToMetadata can handle duplicates properly."""
    change_1 = cros_patch.GerritFetchOnlyPatch(
        'https://host/chromite/tacos',
        'chromite/tacos',
        'refs/changes/11/12345/4',
        'master',
        'cros-internal',
        '7181e4b5e182b6f7d68461b04253de095bad74f9',
        'I47ea30385af60ae4cc2acc5d1a283a46423bc6e1',
        '12345',
        '4',
        'foo@chromium.org',
        1,
        1,
        3)
    change_2 = cros_patch.GerritFetchOnlyPatch(
        'https://host/chromite/foo',
        'chromite/foo',
        'refs/changes/11/12344/3',
        'master',
        'cros-internal',
        'cf23df2207d99a74fbe169e3eba035e633b65d94',
        'Iab9bf08b9b9bd4f72721cfc36e843ed302aca11a',
        '12344',
        '3',
        'foo@chromium.org',
        0,
        0,
        1)
    stage = self.ConstructStage()
    stage.WriteChangesToMetadata([change_1, change_1, change_2])
    # Test whether the sort function works.
    expected = [change_2.GetAttributeDict(), change_1.GetAttributeDict()]
    result = self._run.attrs.metadata.GetValue('changes')
    self.assertEqual(expected, result)

class BaseCQTestCase(generic_stages_unittest.StageTestCase):
  """Helper class for testing the CommitQueueSync stage"""
  MANIFEST_CONTENTS = '<manifest/>'

  def setUp(self):
    """Setup patchers for specified bot id."""
    # Mock out methods as needed.
    self.PatchObject(lkgm_manager, 'GenerateBlameList')
    self.PatchObject(lkgm_manager.LKGMManager, 'SetInFlight')
    self.PatchObject(repository.RepoRepository, 'ExportManifest',
                     return_value=self.MANIFEST_CONTENTS, autospec=True)
    self.PatchObject(sync_stages.SyncStage, 'WriteChangesToMetadata')
    self.StartPatcher(git_unittest.ManifestMock())
    self.StartPatcher(git_unittest.ManifestCheckoutMock())
    version_file = os.path.join(self.build_root, constants.VERSION_FILE)
    manifest_version_unittest.VersionInfoTest.WriteFakeVersionFile(version_file)
    rc_mock = self.StartPatcher(cros_build_lib_unittest.RunCommandMock())
    rc_mock.SetDefaultCmdResult()

    # Block the CQ from contacting GoB.
    self.PatchObject(gerrit.GerritHelper, 'RemoveReady')
    self.PatchObject(validation_pool.PaladinMessage, 'Send')
    self.PatchObject(validation_pool.ValidationPool, 'SubmitChanges')

    # If a test is still contacting GoB, something is busted.
    self.PatchObject(gob_util, 'CreateHttpConn',
                     side_effect=AssertionError('Test should not contact GoB'))
    self.PatchObject(git, 'GitPush',
                     side_effect=AssertionError('Test should not push.'))

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

    # BuildStart stage would have seeded the build.
    build_id = self.fake_db.InsertBuild(
        'test_builder', constants.WATERFALL_TRYBOT, 666, 'test_config',
        'test_hostname',
        timeout_seconds=23456)
    self._run.attrs.metadata.UpdateWithDict({'build_id': build_id})

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
    kwargs.setdefault(
        'approval_timestamp',
        time.time() - sync_stages.PreCQLauncherStage.LAUNCH_DELAY * 60)
    changes = changes or [MockPatch(**kwargs)] * num_patches
    if tree_throttled:
      for change in changes:
        change.flags['COMR'] = '2'
    if pre_cq_status is not None:
      config = constants.PRE_CQ_DEFAULT_CONFIGS[0]
      new_build_id = self.fake_db.InsertBuild('Pre cq group',
                                              constants.WATERFALL_TRYBOT,
                                              1,
                                              config,
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
      def Query(*_args, **_kwargs):
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
  MILESTONE_VERSION = '10'

  def setUp(self):
    self._run.options.master_build_id = self.fake_db.InsertBuild(
        'master builder name', 'waterfall', 1, 'master-paladin', 'bot hostname')
    self.fake_db.UpdateMetadata(
        self._run.options.master_build_id,
        {'version': {'milestone': self.MILESTONE_VERSION}})
    self.PatchObject(sync_stages.ManifestVersionedSyncStage,
                     '_GetMasterVersion', return_value='foo',
                     autospec=True)
    self.PatchObject(lkgm_manager.LKGMManager, 'BootstrapFromVersion',
                     return_value=self.manifest_path, autospec=True)
    self.PatchObject(repository.RepoRepository, 'Sync', autospec=True)

  def testReload(self):
    """Test basic ability to sync and reload the patches from disk."""
    self.sync_stage.PerformStage()
    self.ReloadPool()

  def testSupplantedMaster(self):
    """Test that stage fails if master has been supplanted."""
    new_master_build_id = self.fake_db.InsertBuild(
        'master builder name', 'waterfall', 2, 'master-paladin', 'bot hostname')
    self.fake_db.UpdateMetadata(
        new_master_build_id,
        {'version': {'milestone': self.MILESTONE_VERSION}})
    with self.assertRaises(failures_lib.MasterSlaveVersionMismatchFailure):
      self.sync_stage.PerformStage()

  def testSupplantedMasterDifferentMilestone(self):
    """Test that master supplanting logic respects milestone.

    The master-was-supplanted logic should ignore masters for different
    milestone version.
    """
    new_master_build_id = self.fake_db.InsertBuild(
        'master builder name', 'waterfall', 2, 'master-paladin', 'bot hostname')
    self.fake_db.UpdateMetadata(
        new_master_build_id,
        {'version': {'milestone': 'foo'}})
    self.sync_stage.PerformStage()


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
    self.assertItemsEqual(self.sync_stage.pool.candidates, changes)
    self.assertItemsEqual(self.sync_stage.pool.non_manifest_changes, [])

  def testFailedCommitOfNonManifestChange(self):
    """See MasterCQSyncTestCase"""
    changes = self._testFailedCommitOfNonManifestChange()
    self.assertItemsEqual(self.sync_stage.pool.candidates, changes)
    self.assertItemsEqual(self.sync_stage.pool.non_manifest_changes, [])

  def testCommitManifestChange(self):
    """See MasterCQSyncTestCase"""
    changes = self._testCommitManifestChange()
    self.assertItemsEqual(self.sync_stage.pool.candidates, changes)
    self.assertItemsEqual(self.sync_stage.pool.non_manifest_changes, [])

  def testCommitManifestChangeWithoutPreCQ(self):
    """Changes get ignored if they aren't approved by pre-cq."""
    self._testCommitManifestChange(pre_cq_status=None)
    self.assertItemsEqual(self.sync_stage.pool.candidates, [])
    self.assertItemsEqual(self.sync_stage.pool.non_manifest_changes, [])

  def testCommitManifestChangeWithoutPreCQAndOldPatches(self):
    """Changes get tested without pre-cq if the approval_timestamp is old."""
    changes = self._testCommitManifestChange(pre_cq_status=None,
                                             approval_timestamp=0)
    self.assertItemsEqual(self.sync_stage.pool.candidates, changes)
    self.assertItemsEqual(self.sync_stage.pool.non_manifest_changes, [])

  def testDefaultSync(self):
    """See MasterCQSyncTestCase"""
    changes = self._testDefaultSync()
    self.assertItemsEqual(self.sync_stage.pool.candidates, changes)
    self.assertItemsEqual(self.sync_stage.pool.non_manifest_changes, [])

  def testReload(self):
    """Test basic ability to sync and reload the patches from disk."""
    # Use zero patches because mock patches can't be pickled.
    changes = self.PerformSync(num_patches=0, runs=0)
    self.ReloadPool()
    self.assertItemsEqual(self.sync_stage.pool.candidates, changes)
    self.assertItemsEqual(self.sync_stage.pool.non_manifest_changes, [])

  def testTreeClosureBlocksCommit(self):
    """Test that tree closures block commits."""
    self.assertRaises(failures_lib.ExitEarlyException,
                      self._testCommitNonManifestChange,
                      tree_open=False)

  def testTreeThrottleUsesAlternateGerritQuery(self):
    """Test that if the tree is throttled, we use an alternate gerrit query."""
    changes = self.PerformSync(tree_throttled=True)
    gerrit.GerritHelper.Query.assert_called_with(
        mock.ANY, constants.THROTTLED_CQ_READY_QUERY[0],
        sort='lastUpdated')
    self.assertItemsEqual(self.sync_stage.pool.candidates, changes)
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
    self.PatchObject(validation_pool.ValidationPool, 'HandlePreCQSuccess',
                     autospec=True)

  def _Prepare(self, bot_id=None, **kwargs):
    build_id = self.fake_db.InsertBuild(
        constants.PRE_CQ_LAUNCHER_NAME, constants.WATERFALL_INTERNAL, 1,
        constants.PRE_CQ_LAUNCHER_CONFIG, 'bot-hostname')

    super(PreCQLauncherStageTest, self)._Prepare(
        bot_id, build_id=build_id, **kwargs)

    self.sync_stage = sync_stages.PreCQLauncherStage(self._run)

  def testVerificationsForChangeValidConfig(self):
    change = MockPatch()
    configs_to_test = chromeos_config.GetConfig().keys()[:5]
    return_string = ' '.join(configs_to_test)
    self.PatchObject(triage_lib, 'GetOptionForChange',
                     return_value=return_string)
    self.assertItemsEqual(self.sync_stage.VerificationsForChange(change),
                          configs_to_test)

  def testVerificationsForChangeNoSuchConfig(self):
    change = MockPatch()
    self.PatchObject(triage_lib, 'GetOptionForChange',
                     return_value='this_config_does_not_exist')
    self.assertItemsEqual(self.sync_stage.VerificationsForChange(change),
                          constants.PRE_CQ_DEFAULT_CONFIGS)

  def testVerificationsForChangeEmptyField(self):
    change = MockPatch()
    self.PatchObject(triage_lib, 'GetOptionForChange',
                     return_value=' ')
    self.assertItemsEqual(self.sync_stage.VerificationsForChange(change),
                          constants.PRE_CQ_DEFAULT_CONFIGS)

  def testVerificationsForChangeNoneField(self):
    change = MockPatch()
    self.PatchObject(triage_lib, 'GetOptionForChange',
                     return_value=None)
    self.assertItemsEqual(self.sync_stage.VerificationsForChange(change),
                          constants.PRE_CQ_DEFAULT_CONFIGS)

  def testOverlayVerifications(self):
    change = MockPatch(project='chromiumos/overlays/chromiumos-overlay')
    self.PatchObject(triage_lib, 'GetOptionForChange',
                     return_value=None)
    configs = constants.PRE_CQ_DEFAULT_CONFIGS + [constants.BINHOST_PRE_CQ]
    self.assertItemsEqual(self.sync_stage.VerificationsForChange(change),
                          configs)

  def testRequestedDefaultVerifications(self):
    change = MockPatch()
    self.PatchObject(triage_lib, 'GetOptionForChange',
                     return_value='default x86-zgb-pre-cq')
    configs = constants.PRE_CQ_DEFAULT_CONFIGS + ['x86-zgb-pre-cq']
    self.assertItemsEqual(self.sync_stage.VerificationsForChange(change),
                          configs)

  def testVerificationsForChangeFromInvalidCommitMessage(self):
    change = MockPatch(commit_message="""First line.

Third line.
pre-cq-configs: insect-pre-cq
""")
    self.PatchObject(triage_lib, 'GetOptionForChange',
                     return_value='lumpy-pre-cq')
    self.assertItemsEqual(self.sync_stage.VerificationsForChange(change),
                          ['lumpy-pre-cq'])

  def testVerificationsForChangeFromCommitMessage(self):
    change = MockPatch(commit_message="""First line.

Third line.
pre-cq-configs: stumpy-pre-cq
""")
    self.PatchObject(triage_lib, 'GetOptionForChange',
                     return_value='lumpy-pre-cq')
    self.assertItemsEqual(self.sync_stage.VerificationsForChange(change),
                          ['stumpy-pre-cq'])

  def testMultiVerificationsForChangeFromCommitMessage(self):
    change = MockPatch(commit_message="""First line.

Third line.
pre-cq-configs: stumpy-pre-cq
pre-cq-configs: link-pre-cq
""")
    self.PatchObject(triage_lib, 'GetOptionForChange',
                     return_value='lumpy-pre-cq')
    self.assertItemsEqual(self.sync_stage.VerificationsForChange(change),
                          ['stumpy-pre-cq', 'link-pre-cq'])

  def _PrepareChangesWithPendingVerifications(self, verifications=None):
    """Prepare changes and pending verifications for them.

    This helper creates changes in the validation pool, each of which
    require its own set of verifications.

    Args:
      verifications: A list of lists of configs. Each element in the
                     outer list corresponds to a different CL. Defaults
                     to [constants.PRE_CQ_DEFAULT_CONFIGS]

    Returns:
      A list of len(verifications) MockPatch instances.
    """
    verifications = verifications or [constants.PRE_CQ_DEFAULT_CONFIGS]
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

    for config in constants.PRE_CQ_DEFAULT_CONFIGS:
      build_id = self.fake_db.InsertBuild(
          'builder name', constants.WATERFALL_TRYBOT, 2, config,
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
    submit_reason = constants.STRATEGY_PRECQ_SUBMIT
    verified_cls = {c:submit_reason for c in set([change])}
    m.assert_called_with(verified_cls, check_tree_open=False)


  def testSubmitUnableInPreCQ(self):
    change = self._PrepareSubmittableChange()

    # Change should throw a DependencyError when trying to create a transaction
    e = cros_patch.DependencyError(change, cros_patch.PatchException(change))
    self.PatchObject(validation_pool.PatchSeries, 'CreateTransaction',
                     side_effect=e)
    self.PerformSync(pre_cq_status=None, changes=[change], patch_objects=False)
    # Change should be marked as pre-cq passed, rather than being submitted.
    self.assertEqual(constants.CL_STATUS_PASSED, self._GetPreCQStatus(change))

  def assertAllStatuses(self, changes, status):
    """Verify that all configs for |changes| all have status |status|.

    Args:
      changes: List of changes.
      status: Desired status value.
    """
    action_history = self.fake_db.GetActionsForChanges(changes)
    progress_map = clactions.GetPreCQProgressMap(changes, action_history)
    for change in changes:
      for config in progress_map[change]:
        self.assertEqual(progress_map[change][config][0], status)

  def testNewPatches(self):
    # Create a change that is ready to be tested.
    change = self._PrepareChangesWithPendingVerifications()[0]
    change.approval_timestamp = 0

    # Change should be launched now.
    self.PerformSync(pre_cq_status=None, changes=[change], runs=2)
    self.assertAllStatuses([change], constants.CL_PRECQ_CONFIG_STATUS_LAUNCHED)

  def testLaunchPerCycleLimit(self):
    # Create 4x as many changes as we can launch in one cycle.
    change_count = (
        sync_stages.PreCQLauncherStage.MAX_LAUNCHES_PER_CYCLE_DERIVATIVE * 4)
    changes = self._PrepareChangesWithPendingVerifications(
        [['lumpy-pre-cq']] * change_count)
    for c in changes:
      c.approval_timestamp = 0

    def count_launches():
      action_history = self.fake_db.GetActionsForChanges(changes)
      return len(
          [a for a in action_history
           if a.action == constants.CL_ACTION_TRYBOT_LAUNCHING])

    # After one cycle of the launcher, exactly MAX_LAUNCHES_PER_CYCLE_DERIVATIVE
    # should have launched.
    self.PerformSync(pre_cq_status=None, changes=changes, runs=1)
    self.assertEqual(
        count_launches(),
        sync_stages.PreCQLauncherStage.MAX_LAUNCHES_PER_CYCLE_DERIVATIVE)

    # After the next cycle, exactly 3 * MAX_LAUNCHES_PER_CYCLE_DERIVATIVE should
    # have launched in total.
    self.PerformSync(pre_cq_status=None, changes=changes, runs=1,
                     patch_objects=False)
    self.assertEqual(
        count_launches(),
        3 * sync_stages.PreCQLauncherStage.MAX_LAUNCHES_PER_CYCLE_DERIVATIVE)

  def testNoLaunchClosedTree(self):
    self.PatchObject(tree_status, 'IsTreeOpen', return_value=False)

    # Create a change that is ready to be tested.
    change = self._PrepareChangesWithPendingVerifications()[0]
    change.approval_timestamp = 0

    # Change should still be pending.
    self.PerformSync(pre_cq_status=None, changes=[change], runs=2)
    self.assertAllStatuses([change], constants.CL_PRECQ_CONFIG_STATUS_PENDING)

  def testDontTestSubmittedPatches(self):
    # Create a change that has been submitted.
    change = self._PrepareChangesWithPendingVerifications()[0]
    change.approval_timestamp = 0
    change.status = 'SUBMITTED'

    # Change should not be touched by the Pre-CQ if it's submitted.
    self.PerformSync(pre_cq_status=None, changes=[change], runs=1)
    action_history = self.fake_db.GetActionsForChanges([change])
    progress_map = clactions.GetPreCQProgressMap([change], action_history)
    self.assertEqual(progress_map, {})

  def testRetryInPreCQ(self):
    # Create a change that is ready to be tested.
    change = self._PrepareChangesWithPendingVerifications([['orange']])[0]
    change.approval_timestamp = 0

    # Change should be launched now.
    self.PerformSync(pre_cq_status=None, changes=[change], runs=2)
    self.assertAllStatuses([change], constants.CL_PRECQ_CONFIG_STATUS_LAUNCHED)

    # Fake all these tryjobs starting
    build_ids = self._FakeLaunchTryjobs([change])

    # After 1 more Sync all configs should now be inflight.
    self.PerformSync(pre_cq_status=None, changes=[change], patch_objects=False)
    self.assertAllStatuses([change], constants.CL_PRECQ_CONFIG_STATUS_INFLIGHT)

    # Pretend that the build failed with an infrastructure failure so the change
    # should be retried.
    self.fake_db.InsertCLActions(
        build_ids['orange'],
        [clactions.CLAction.FromGerritPatchAndAction(
            change, constants.CL_ACTION_FORGIVEN)])

    # Change should relaunch again.
    self.PerformSync(pre_cq_status=None, changes=[change], runs=1,
                     patch_objects=False)
    self.assertAllStatuses([change], constants.CL_PRECQ_CONFIG_STATUS_LAUNCHED)

  def testPreCQ(self):
    changes = self._PrepareChangesWithPendingVerifications(
        [['orange', 'apple'], ['banana'], ['banana'], ['banana'], ['banana']])
    # After 2 runs, the changes should be screened but not
    # yet launched (due to pre-launch timeout).
    for c in changes:
      c.approval_timestamp = time.time()

    # Mark a change as trybot ready, but not approved. It should also be tried
    # by the pre-cq.
    for change in changes[2:5]:
      change.flags = {'TRY': '1'}
      change.IsMergeable = lambda: False

    self.PerformSync(pre_cq_status=None, changes=changes, runs=2)

    self.assertAllStatuses(changes, constants.CL_PRECQ_CONFIG_STATUS_PENDING)

    action_history = self.fake_db.GetActionsForChanges(changes)
    progress_map = clactions.GetPreCQProgressMap(changes, action_history)
    self.assertEqual(2, len(progress_map[changes[0]]))
    for change in changes[1:]:
      self.assertEqual(1, len(progress_map[change]))

    # Fake that launch delay has expired by changing change approval times.
    for c in changes:
      c.approval_timestamp = 0

    # After 1 more Sync all configs for all changes should be launched.
    self.PerformSync(pre_cq_status=None, changes=changes, patch_objects=False)

    self.assertAllStatuses(changes, constants.CL_PRECQ_CONFIG_STATUS_LAUNCHED)

    # Fake all these tryjobs starting
    build_ids = self._FakeLaunchTryjobs(changes)

    # After 1 more Sync all configs should now be inflight.
    self.PerformSync(pre_cq_status=None, changes=changes, patch_objects=False)
    self.assertAllStatuses(changes, constants.CL_PRECQ_CONFIG_STATUS_INFLIGHT)

    # Fake INFLIGHT_TIMEOUT+1 passing with banana and orange config succeeding,
    # and apple never launching. The first change should pass the pre-cq, the
    # second should fail due to inflight timeout.
    fake_time = datetime.datetime.now() + datetime.timedelta(
        minutes=sync_stages.PreCQLauncherStage.INFLIGHT_TIMEOUT + 1)
    self.fake_db.SetTime(fake_time)
    self.fake_db.InsertCLActions(
        build_ids['orange'],
        [clactions.CLAction.FromGerritPatchAndAction(
            changes[0], constants.CL_ACTION_VERIFIED)])
    for change in changes[1:3]:
      self.fake_db.InsertCLActions(
          build_ids['banana'],
          [clactions.CLAction.FromGerritPatchAndAction(
              change, constants.CL_ACTION_VERIFIED)])

    self.PerformSync(pre_cq_status=None, changes=changes, patch_objects=False)

    self.assertEqual(self._GetPreCQStatus(changes[0]),
                     constants.CL_STATUS_FAILED)
    self.assertEqual(self._GetPreCQStatus(changes[1]),
                     constants.CL_STATUS_PASSED)
    self.assertEqual(self._GetPreCQStatus(changes[2]),
                     constants.CL_STATUS_FULLY_VERIFIED)
    for change in changes[3:5]:
      self.assertEqual(self._GetPreCQStatus(change),
                       constants.CL_STATUS_FAILED)

    # Failed CLs that are marked ready should be tried again, and changes that
    # aren't ready shouldn't be launched.
    changes[4].flags = {'CRVW': '2'}
    changes[4].HasReadyFlag = lambda: False
    self.PerformSync(pre_cq_status=None, changes=changes, patch_objects=False,
                     runs=3)
    action_history = self.fake_db.GetActionsForChanges(changes)
    progress_map = clactions.GetPreCQProgressMap(changes, action_history)
    self.assertEqual(progress_map[changes[0]]['apple'][0],
                     constants.CL_PRECQ_CONFIG_STATUS_LAUNCHED)
    self.assertEqual(progress_map[changes[1]]['banana'][0],
                     constants.CL_PRECQ_CONFIG_STATUS_VERIFIED)
    self.assertEqual(progress_map[changes[2]]['banana'][0],
                     constants.CL_PRECQ_CONFIG_STATUS_VERIFIED)
    self.assertEqual(progress_map[changes[3]]['banana'][0],
                     constants.CL_PRECQ_CONFIG_STATUS_LAUNCHED)
    self.assertEqual(progress_map[changes[4]]['banana'][0],
                     constants.CL_PRECQ_CONFIG_STATUS_FAILED)

    # These actions should only be recorded at most once for every
    # patch. We did not upload any new patch for changes, so there
    # should not be dupulicated actions.
    unique_actions = (constants.CL_ACTION_PRE_CQ_FULLY_VERIFIED,
                      constants.CL_ACTION_PRE_CQ_READY_TO_SUBMIT,
                      constants.CL_ACTION_PRE_CQ_PASSED)
    for change in changes:
      actions = self.fake_db.GetActionsForChanges([change])
      for action_type in unique_actions:
        self.assertTrue(
            len([x for x in actions if x.action == action_type]) <= 1)

    # Fake a long time elapsing, see that passed or fully verified changes
    # (changes 1 and 2 in this test) get status expired back to None.
    fake_time = self.fake_db.GetTime() + datetime.timedelta(
        minutes=sync_stages.PreCQLauncherStage.STATUS_EXPIRY_TIMEOUT + 1)
    self.fake_db.SetTime(fake_time)
    self.PerformSync(pre_cq_status=None, changes=changes, patch_objects=False)
    for c in changes[1:2]:
      self.assertEqual(self._GetPreCQStatus(c), None)

  def testSpeculativePreCQ(self):
    changes = self._PrepareChangesWithPendingVerifications(
        [constants.PRE_CQ_DEFAULT_CONFIGS] * 2)

    # Turn our changes into speculatifve PreCQ candidates.
    for change in changes:
      change.flags.pop('COMR')
      change.IsMergeable = lambda: False
      change.HasReadyFlag = lambda: False

    # Fake that launch delay has expired by changing change approval times.
    for change in changes:
      change.approval_timestamp = 0

    # This should cause the changes to be pending.
    self.PerformSync(pre_cq_status=None, changes=changes)

    self.assertAllStatuses(changes, constants.CL_PRECQ_CONFIG_STATUS_PENDING)

    # This should move the change from pending -> launched.
    self.PerformSync(pre_cq_status=None, changes=changes, patch_objects=False)

    self.assertAllStatuses(changes, constants.CL_PRECQ_CONFIG_STATUS_LAUNCHED)

    # Make sure every speculative change is marked that way.
    for change in changes:
      actions = [a.action for a in self.fake_db.GetActionsForChanges([change])]
      self.assertIn(constants.CL_ACTION_SPECULATIVE, actions)

    # Fake all these tryjobs starting.
    build_ids = self._FakeLaunchTryjobs(changes)

    # After 1 more Sync all configs should now be inflight.
    self.PerformSync(pre_cq_status=None, changes=changes, patch_objects=False)
    self.assertAllStatuses(changes, constants.CL_PRECQ_CONFIG_STATUS_INFLIGHT)

    # Verify that we mark the change as inflight.
    self.assertEqual(self._GetPreCQStatus(changes[0]),
                     constants.CL_STATUS_INFLIGHT)

    # Fake CL 0 being verified by all configs.
    for config in constants.PRE_CQ_DEFAULT_CONFIGS:
      self.fake_db.InsertCLActions(
          build_ids[config],
          [clactions.CLAction.FromGerritPatchAndAction(
              changes[0], constants.CL_ACTION_VERIFIED)])

    # Fake CL 1 being rejected and failed by all configs except the first.
    for config in constants.PRE_CQ_DEFAULT_CONFIGS[1:]:
      self.fake_db.InsertCLActions(
          build_ids[config],
          [clactions.CLAction.FromGerritPatchAndAction(
              changes[1], constants.CL_ACTION_KICKED_OUT)])
      self.fake_db.InsertCLActions(
          build_ids[config],
          [clactions.CLAction.FromGerritPatchAndAction(
              changes[1], constants.CL_ACTION_PRE_CQ_FAILED)])


    self.PerformSync(pre_cq_status=None, changes=changes, patch_objects=False)

    # Verify that we mark CL 0 as fully verified (not passed).
    self.assertEqual(self._GetPreCQStatus(changes[0]),
                     constants.CL_STATUS_FULLY_VERIFIED)
    # Verify that CL 1 has status failed.
    self.assertEqual(self._GetPreCQStatus(changes[1]),
                     constants.CL_STATUS_FAILED)

    # Mark our changes as ready, and see if they are immediately passed.
    for change in changes:
      change.flags['COMR'] = '1'
      change.IsMergeable = lambda: True
      change.HasReadyFlag = lambda: True

    self.PerformSync(pre_cq_status=None, changes=changes, patch_objects=False)

    self.assertEqual(self._GetPreCQStatus(changes[0]),
                     constants.CL_STATUS_PASSED)

  def _FakeLaunchTryjobs(self, changes):
    """Pretend to start all launched tryjobs."""
    action_history = self.fake_db.GetActionsForChanges(changes)
    progress_map = clactions.GetPreCQProgressMap(changes, action_history)
    build_ids_per_config = {}
    for change, change_status_dict in progress_map.iteritems():
      for config, (status, _, _) in change_status_dict.iteritems():
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


class MasterSlaveLKGMSyncTest(generic_stages_unittest.StageTestCase):
  """Unit tests for MasterSlaveLKGMSyncStage"""

  BOT_ID = constants.PFQ_MASTER

  def setUp(self):
    """Setup"""
    self.source_repo = 'ssh://source/repo'
    self.manifest_version_url = 'fake manifest url'
    self.branch = 'master'
    self.build_name = 'master-chromium-pfq'
    self.incr_type = 'branch'
    self.next_version = 'next_version'
    self.sync_stage = None

    repo = repository.RepoRepository(
        self.source_repo, self.tempdir, self.branch)
    self.manager = lkgm_manager.LKGMManager(
        source_repo=repo, manifest_repo=self.manifest_version_url,
        build_names=[self.build_name],
        build_type=constants.CHROME_PFQ_TYPE,
        incr_type=self.incr_type,
        force=False, branch=self.branch, dry_run=True)

    # Create and set up a fake cidb instance.
    self.fake_db = fake_cidb.FakeCIDBConnection()
    cidb.CIDBConnectionFactory.SetupMockCidb(self.fake_db)

    self._Prepare()

  def _Prepare(self, bot_id=None, **kwargs):
    super(MasterSlaveLKGMSyncTest, self)._Prepare(bot_id, **kwargs)

    self._run.config['manifest_version'] = self.manifest_version_url
    self.sync_stage = sync_stages.MasterSlaveLKGMSyncStage(self._run)
    self.sync_stage.manifest_manager = self.manager
    self._run.attrs.manifest_manager = self.manager

  def testGetLastChromeOSVersion(self):
    """Test GetLastChromeOSVersion"""
    id1 = self.fake_db.InsertBuild(
        builder_name='test_builder',
        waterfall=constants.WATERFALL_TRYBOT,
        build_number=666,
        build_config='master-chromium-pfq',
        bot_hostname='test_hostname')
    id2 = self.fake_db.InsertBuild(
        builder_name='test_builder',
        waterfall=constants.WATERFALL_TRYBOT,
        build_number=667,
        build_config='master-chromium-pfq',
        bot_hostname='test_hostname')
    metadata_1 = metadata_lib.CBuildbotMetadata()
    metadata_1.UpdateWithDict(
        {'version': {'full': 'R42-7140.0.0-rc1'}})
    metadata_2 = metadata_lib.CBuildbotMetadata()
    metadata_2.UpdateWithDict(
        {'version': {'full': 'R43-7141.0.0-rc1'}})
    self._run.attrs.metadata.UpdateWithDict(
        {'version': {'full': 'R44-7142.0.0-rc1'}})
    self.fake_db.UpdateMetadata(id1, metadata_1)
    self.fake_db.UpdateMetadata(id2, metadata_2)
    v = self.sync_stage.GetLastChromeOSVersion()
    self.assertEqual(v.milestone, '43')
    self.assertEqual(v.platform, '7141.0.0-rc1')
