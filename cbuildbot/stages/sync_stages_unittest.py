# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for sync stages."""

from __future__ import print_function

import pickle
import itertools
import os
import time
import tempfile

import mock

from chromite.cbuildbot import commands
from chromite.cbuildbot import lkgm_manager
from chromite.cbuildbot import manifest_version
from chromite.cbuildbot import manifest_version_unittest
from chromite.cbuildbot import repository
from chromite.cbuildbot import trybot_patch_pool
from chromite.cbuildbot import validation_pool
from chromite.cbuildbot.stages import generic_stages_unittest
from chromite.cbuildbot.stages import sync_stages
from chromite.lib import auth
from chromite.lib import buildbucket_lib
from chromite.lib import cidb
from chromite.lib import clactions
from chromite.lib import cl_messages
from chromite.lib import constants
from chromite.lib import cros_test_lib
from chromite.lib import fake_cidb
from chromite.lib import failures_lib
from chromite.lib import gerrit
from chromite.lib import git
from chromite.lib import git_unittest
from chromite.lib import gob_util
from chromite.lib import metadata_lib
from chromite.lib import osutils
from chromite.lib import patch as cros_patch
from chromite.lib import patch_unittest
from chromite.lib.buildstore import FakeBuildStore

# It's normal for unittests to access protected members.
# pylint: disable=protected-access


class BootstrapStageTest(generic_stages_unittest.AbstractStageTestCase,
                         cros_test_lib.RunCommandTestCase):
  """Tests the Bootstrap stage."""

  BOT_ID = 'sync-test-cbuildbot'
  RELEASE_TAG = ''

  def setUp(self):
    # Pretend API version is always current.
    self.PatchObject(
        commands, 'GetTargetChromiteApiVersion',
        return_value=(constants.REEXEC_API_MAJOR, constants.REEXEC_API_MINOR))

    self._Prepare()
    self.buildstore = FakeBuildStore()

  def ConstructStage(self):
    patch_pool = trybot_patch_pool.TrybotPatchPool()
    return sync_stages.BootstrapStage(self._run, self.buildstore, patch_pool)

  def testSimpleBootstrap(self):
    """Verify Bootstrap behavior in a simple case (with a branch)."""

    self.RunStage()

    # Clone next chromite checkout.
    self.assertCommandContains([
        'git',
        'clone',
        constants.CHROMITE_URL,
        mock.ANY,  # Can't predict new chromium checkout diretory.
        '--reference',
        mock.ANY
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
        'chromite/cbuildbot/cbuildbot',
        'sync-test-cbuildbot',
        '-r',
        os.path.join(self.tempdir, 'buildroot'),
        '--buildbot',
        '--noprebuilts',
        '--buildnumber',
        '1234321',
        '--branch',
        'ooga_booga',
        '--sourceroot',
        mock.ANY,
        '--nobootstrap',
    ])


class ManifestVersionedSyncStageTest(
    generic_stages_unittest.AbstractStageTestCase):
  """Tests the ManifestVersionedSync stage."""

  # pylint: disable=abstract-method

  def setUp(self):
    self.source_repo = 'ssh://source/repo'
    self.manifest_version_url = 'fake manifest url'
    self.branch = 'master'
    self.build_name = 'amd64-generic'
    self.incr_type = 'branch'
    self.next_version = 'next_version'
    self.sync_stage = None
    self.buildstore = FakeBuildStore()

    self.repo = repository.RepoRepository(self.source_repo, self.tempdir,
                                          self.branch)
    self.manager = manifest_version.BuildSpecsManager(
        self.repo,
        self.manifest_version_url, [self.build_name],
        self.incr_type,
        force=False,
        branch=self.branch,
        buildstore=self.buildstore,
        dry_run=True)

    self._Prepare()

  # Our API here is not great when it comes to kwargs passing.
  def _Prepare(self, bot_id=None, **kwargs):  # pylint: disable=arguments-differ
    super(ManifestVersionedSyncStageTest, self)._Prepare(bot_id, **kwargs)

    self._run.config['manifest_version'] = self.manifest_version_url
    self.sync_stage = sync_stages.ManifestVersionedSyncStage(
        self._run, self.buildstore)
    self.sync_stage.manifest_manager = self.manager
    self._run.attrs.manifest_manager = self.manager

  def testManifestVersionedSyncOnePartBranch(self):
    """Tests basic ManifestVersionedSyncStage with branch ooga_booga"""
    self.PatchObject(sync_stages.ManifestVersionedSyncStage, 'Initialize')
    self.PatchObject(sync_stages.ManifestVersionedSyncStage,
                     '_SetAndroidVersionIfApplicable')
    self.PatchObject(sync_stages.ManifestVersionedSyncStage,
                     '_SetChromeVersionIfApplicable')
    self.PatchObject(
        manifest_version.BuildSpecsManager,
        'GetNextBuildSpec',
        return_value=self.next_version)
    self.PatchObject(manifest_version.BuildSpecsManager, 'GetLatestPassingSpec')
    self.PatchObject(
        sync_stages.SyncStage,
        'ManifestCheckout',
        return_value=self.next_version)
    self.PatchObject(
        sync_stages.ManifestVersionedSyncStage,
        '_GetMasterVersion',
        return_value='foo',
        autospec=True)
    self.PatchObject(
        manifest_version.BuildSpecsManager,
        'BootstrapFromVersion',
        autospec=True)
    self.PatchObject(repository.RepoRepository, 'Sync', autospec=True)

    self.sync_stage.Run()

  def testInitialize(self):
    self.PatchObject(sync_stages.SyncStage, '_InitializeRepo')
    self.sync_stage.repo = self.repo
    self.sync_stage.Initialize()


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
        'LCQ': '1',
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

  BOT_ID = 'master-paladin'

  def setUp(self):
    self.PatchObject(
        buildbucket_lib, 'GetServiceAccount', return_value='server_account')
    self.PatchObject(auth.AuthorizedHttp, '__init__', return_value=None)
    self.PatchObject(
        buildbucket_lib.BuildbucketClient,
        '_GetHost',
        return_value=buildbucket_lib.BUILDBUCKET_TEST_HOST)
    # Create and set up a fake cidb instance.
    self.fake_db = fake_cidb.FakeCIDBConnection()
    cidb.CIDBConnectionFactory.SetupMockCidb(self.fake_db)

    self._Prepare()
    self.buildstore = FakeBuildStore()

  def ConstructStage(self):
    return sync_stages.SyncStage(self._run, self.buildstore)

  def testWriteChangesToMetadata(self):
    """Test whether WriteChangesToMetadata can handle duplicates properly."""
    change_1 = cros_patch.GerritFetchOnlyPatch(
        'https://host/chromite/tacos', 'chromite/tacos',
        'refs/changes/11/12345/4', 'master', 'cros-internal',
        '7181e4b5e182b6f7d68461b04253de095bad74f9',
        'I47ea30385af60ae4cc2acc5d1a283a46423bc6e1', '12345', '4',
        'foo@chromium.org', 1, 1, 3)
    change_2 = cros_patch.GerritFetchOnlyPatch(
        'https://host/chromite/foo', 'chromite/foo', 'refs/changes/11/12344/3',
        'master', 'cros-internal', 'cf23df2207d99a74fbe169e3eba035e633b65d94',
        'Iab9bf08b9b9bd4f72721cfc36e843ed302aca11a', '12344', '3',
        'foo@chromium.org', 0, 0, 1)
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
    self._patch_factory = patch_unittest.MockPatchFactory()
    # Mock out methods as needed.
    self.PatchObject(lkgm_manager, 'GenerateBlameList')
    self.PatchObject(
        repository.RepoRepository,
        'ExportManifest',
        return_value=self.MANIFEST_CONTENTS,
        autospec=True)
    self.PatchObject(sync_stages.SyncStage, 'WriteChangesToMetadata')
    self.StartPatcher(git_unittest.ManifestMock())
    self.StartPatcher(git_unittest.ManifestCheckoutMock())
    version_file = os.path.join(self.build_root, constants.VERSION_FILE)
    manifest_version_unittest.VersionInfoTest.WriteFakeVersionFile(version_file)
    rc_mock = self.StartPatcher(cros_test_lib.RunCommandMock())
    rc_mock.SetDefaultCmdResult()

    # Block the CQ from contacting GoB.
    self.PatchObject(gerrit.GerritHelper, 'RemoveReady')
    self.PatchObject(cl_messages.PaladinMessage, 'Send')
    self.PatchObject(
        validation_pool.ValidationPool, 'SubmitChanges', return_value=({}, {}))

    # If a test is still contacting GoB, something is busted.
    self.PatchObject(
        gob_util,
        'CreateHttpConn',
        side_effect=AssertionError('Test should not contact GoB'))
    self.PatchObject(
        git, 'GitPush', side_effect=AssertionError('Test should not push.'))

    self.PatchObject(
        buildbucket_lib, 'GetServiceAccount', return_value='server_account')
    self.PatchObject(auth.AuthorizedHttp, '__init__', return_value=None)
    self.PatchObject(
        buildbucket_lib.BuildbucketClient,
        'SendBuildbucketRequest',
        return_value=None)
    self.PatchObject(
        buildbucket_lib.BuildbucketClient,
        '_GetHost',
        return_value=buildbucket_lib.BUILDBUCKET_TEST_HOST)

    # Create a fake repo / manifest on disk that is used by subclasses.
    for subdir in ('repo', 'manifests'):
      osutils.SafeMakedirs(os.path.join(self.build_root, '.repo', subdir))
    self.manifest_path = os.path.join(self.build_root, '.repo', 'manifest.xml')
    osutils.WriteFile(self.manifest_path, self.MANIFEST_CONTENTS)
    self.PatchObject(
        validation_pool.ValidationPool,
        'ReloadChanges',
        side_effect=lambda x: x)

    # Create and set up a fake cidb instance.
    self.fake_db = fake_cidb.FakeCIDBConnection()
    self.buildstore = FakeBuildStore(self.fake_db)
    cidb.CIDBConnectionFactory.SetupMockCidb(self.fake_db)

    self.sync_stage = None
    self._Prepare()

  def tearDown(self):
    cidb.CIDBConnectionFactory.ClearMock()

  # Our API here is not great when it comes to kwargs passing.
  def _Prepare(self, bot_id=None, **kwargs):  # pylint: disable=arguments-differ
    super(BaseCQTestCase, self)._Prepare(bot_id, **kwargs)
    self._run.config.overlays = constants.PUBLIC_OVERLAYS
    self.sync_stage = sync_stages.CommitQueueSyncStage(self._run,
                                                       self.buildstore)

    # BuildStart stage would have seeded the build.
    build_id = self.buildstore.InsertBuild(
        'test_builder',
        666,
        'test_config',
        'test_hostname',
        timeout_seconds=23456)
    self._run.attrs.metadata.UpdateWithDict({'build_id': build_id})

  def PerformSync(self,
                  committed=False,
                  num_patches=1,
                  pre_cq_status=constants.CL_STATUS_PASSED,
                  runs=0,
                  changes=None,
                  patch_objects=True,
                  **kwargs):
    """Helper to perform a basic sync for master commit queue.

    Args:
      committed: Value to be returned by mock patches' IsChangeCommitted.
                 Default: False.
      num_patches: The number of mock patches to create. Default: 1.
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
    if pre_cq_status is not None:
      config = constants.PRE_CQ_DEFAULT_CONFIGS[0]
      new_build_id = self.buildstore.InsertBuild('Pre cq group', 1, config,
                                                 'bot-hostname')
      for change in changes:
        action = clactions.TranslatePreCQStatusToAction(pre_cq_status)
        self.fake_db.InsertCLActions(
            new_build_id,
            [clactions.CLAction.FromGerritPatchAndAction(change, action)])

    if patch_objects:
      self.PatchObject(
          gerrit.GerritHelper,
          'IsChangeCommitted',
          return_value=committed,
          autospec=True)

      # Validation pool will mutate the return value it receives from
      # Query, therefore return a copy of the changes list.
      def Query(*_args, **_kwargs):
        return list(changes)

      self.PatchObject(
          gerrit.GerritHelper, 'Query', side_effect=Query, autospec=True)

      exit_it = itertools.chain([False] * runs, itertools.repeat(True))
      self.PatchObject(
          validation_pool.ValidationPool,
          'ShouldExitEarly',
          side_effect=exit_it)

    self.sync_stage.PerformStage()

    return changes

  def ReloadPool(self):
    """Save the pool to disk and reload it."""
    with tempfile.NamedTemporaryFile() as f:
      pickle.dump(self.sync_stage.pool, f)
      f.flush()
      self._run.options.validation_pool = f.name
      self.sync_stage = sync_stages.CommitQueueSyncStage(
          self._run, self.buildstore)
      self.sync_stage.HandleSkip()


class SlaveCQSyncTest(BaseCQTestCase):
  """Tests the CommitQueueSync stage for the paladin slaves."""
  BOT_ID = 'eve-paladin'
  MILESTONE_VERSION = '10'

  def setUp(self):
    self._run.options.master_buildbucket_id = 1234
    self._run.options.master_build_id = self.buildstore.InsertBuild(
        'master builder name', 1, 'master-paladin', 'bot hostname',
        buildbucket_id=1234)
    self.fake_db.UpdateMetadata(
        self._run.options.master_build_id,
        {'version': {
            'milestone': self.MILESTONE_VERSION
        }})
    self.PatchObject(
        sync_stages.ManifestVersionedSyncStage,
        '_GetMasterVersion',
        return_value='foo',
        autospec=True)
    self.PatchObject(
        lkgm_manager.LKGMManager,
        'BootstrapFromVersion',
        return_value=self.manifest_path,
        autospec=True)
    self.PatchObject(repository.RepoRepository, 'Sync', autospec=True)

  def testReload(self):
    """Test basic ability to sync and reload the patches from disk."""
    self.sync_stage.PerformStage()
    self.ReloadPool()

  def testSupplantedMaster(self):
    """Test that stage fails if master has been supplanted."""
    new_master_build_id = self.buildstore.InsertBuild(
        'master builder name', 2, 'master-paladin', 'bot hostname')
    self.fake_db.UpdateMetadata(
        new_master_build_id, {'version': {
            'milestone': self.MILESTONE_VERSION
        }})
    with self.assertRaises(failures_lib.MasterSlaveVersionMismatchFailure):
      self.sync_stage.PerformStage()

  def testSupplantedMasterDifferentBranch(self):
    """Test that master supplanting logic respects branch.

    The master-was-supplanted logic should ignore masters for different
    milestone version.
    """
    self.buildstore.InsertBuild(
        'master builder name', 2, 'master-paladin', 'bot hostname',
        branch='foo')
    self.sync_stage.PerformStage()


class MasterCQSyncTestCase(BaseCQTestCase):
  """Helper class for testing the CommitQueueSync stage masters."""

  BOT_ID = 'master-paladin'

  def setUp(self):
    """Setup patchers for specified bot id."""
    self.AutoPatch([[validation_pool.ValidationPool, 'ApplyPoolIntoRepo']])
    self.PatchObject(
        lkgm_manager.LKGMManager,
        'CreateNewCandidate',
        return_value=self.manifest_path,
        autospec=True)
    self.PatchObject(
        lkgm_manager.LKGMManager,
        'CreateFromManifest',
        return_value=self.manifest_path,
        autospec=True)

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
    self.PatchObject(
        validation_pool.ValidationPool,
        '_FilterNonLcqProjects',
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
    self.assertCountEqual(self.sync_stage.pool.candidates, changes)
    self.assertCountEqual(self.sync_stage.pool.non_manifest_changes, [])

  def testFailedCommitOfNonManifestChange(self):
    """See MasterCQSyncTestCase"""
    changes = self._testFailedCommitOfNonManifestChange()
    self.assertCountEqual(self.sync_stage.pool.candidates, changes)
    self.assertCountEqual(self.sync_stage.pool.non_manifest_changes, [])

  def testCommitManifestChange(self):
    """See MasterCQSyncTestCase"""
    changes = self._testCommitManifestChange()
    self.assertCountEqual(self.sync_stage.pool.candidates, changes)
    self.assertCountEqual(self.sync_stage.pool.non_manifest_changes, [])

  def testCommitManifestChangeWithoutPreCQ(self):
    """Changes get ignored if they aren't approved by pre-cq."""
    self._testCommitManifestChange(pre_cq_status=None)
    self.assertCountEqual(self.sync_stage.pool.candidates, [])
    self.assertCountEqual(self.sync_stage.pool.non_manifest_changes, [])

  def testCommitManifestChangeWithoutPreCQAndOldPatches(self):
    """Changes get tested without pre-cq if the approval_timestamp is old."""
    changes = self._testCommitManifestChange(
        pre_cq_status=None, approval_timestamp=0)
    self.assertCountEqual(self.sync_stage.pool.candidates, changes)
    self.assertCountEqual(self.sync_stage.pool.non_manifest_changes, [])

  def testDefaultSync(self):
    """See MasterCQSyncTestCase"""
    changes = self._testDefaultSync()
    self.assertCountEqual(self.sync_stage.pool.candidates, changes)
    self.assertCountEqual(self.sync_stage.pool.non_manifest_changes, [])

  def testReload(self):
    """Test basic ability to sync and reload the patches from disk."""
    # Use zero patches because mock patches can't be pickled.
    changes = self.PerformSync(num_patches=0, runs=0)
    self.ReloadPool()
    self.assertCountEqual(self.sync_stage.pool.candidates, changes)
    self.assertCountEqual(self.sync_stage.pool.non_manifest_changes, [])


class MasterSlaveLKGMSyncTest(generic_stages_unittest.StageTestCase):
  """Unit tests for MasterSlaveLKGMSyncStage"""

  BOT_ID = constants.MST_ANDROID_PFQ_MASTER

  def setUp(self):
    """Setup"""
    self.source_repo = 'ssh://source/repo'
    self.manifest_version_url = 'fake manifest url'
    self.branch = 'master'
    self.build_name = 'master-mst-android-pfq'
    self.incr_type = 'branch'
    self.next_version = 'next_version'
    self.sync_stage = None

    self.repo = repository.RepoRepository(self.source_repo, self.tempdir,
                                          self.branch)

    # Create and set up a fake cidb instance.
    self.fake_db = fake_cidb.FakeCIDBConnection()
    self.buildstore = FakeBuildStore(self.fake_db)
    cidb.CIDBConnectionFactory.SetupMockCidb(self.fake_db)

    self.manager = lkgm_manager.LKGMManager(
        source_repo=self.repo,
        manifest_repo=self.manifest_version_url,
        build_names=[self.build_name],
        build_type=constants.ANDROID_PFQ_TYPE,
        incr_type=self.incr_type,
        force=False,
        branch=self.branch,
        buildstore=self.buildstore,
        dry_run=True)

    self.PatchObject(buildbucket_lib, 'GetServiceAccount', return_value=True)
    self.PatchObject(auth.AuthorizedHttp, '__init__', return_value=None)
    self.PatchObject(
        buildbucket_lib.BuildbucketClient,
        '_GetHost',
        return_value=buildbucket_lib.BUILDBUCKET_TEST_HOST)

    self._Prepare()

  # Our API here is not great when it comes to kwargs passing.
  def _Prepare(self, bot_id=None, **kwargs):  # pylint: disable=arguments-differ
    super(MasterSlaveLKGMSyncTest, self)._Prepare(bot_id, **kwargs)

    self._run.config['manifest_version'] = self.manifest_version_url
    self.sync_stage = sync_stages.MasterSlaveLKGMSyncStage(
        self._run, self.buildstore)
    self.sync_stage.manifest_manager = self.manager
    self._run.attrs.manifest_manager = self.manager

  def testGetLastChromeOSVersion(self):
    """Test GetLastChromeOSVersion"""
    id1 = self.buildstore.InsertBuild(
        builder_name='test_builder',
        build_number=666,
        build_config='master-mst-android-pfq',
        bot_hostname='test_hostname')
    id2 = self.buildstore.InsertBuild(
        builder_name='test_builder',
        build_number=667,
        build_config='master-mst-android-pfq',
        bot_hostname='test_hostname')
    metadata_1 = metadata_lib.CBuildbotMetadata()
    metadata_1.UpdateWithDict({'version': {'full': 'R42-7140.0.0-rc1'}})
    metadata_2 = metadata_lib.CBuildbotMetadata()
    metadata_2.UpdateWithDict({'version': {'full': 'R43-7141.0.0-rc1'}})
    self._run.attrs.metadata.UpdateWithDict({
        'version': {
            'full': 'R44-7142.0.0-rc1'
        }
    })
    self.fake_db.UpdateMetadata(id1, metadata_1)
    self.fake_db.UpdateMetadata(id2, metadata_2)
    v = self.sync_stage.GetLastChromeOSVersion()
    self.assertEqual(v.milestone, '43')
    self.assertEqual(v.platform, '7141.0.0-rc1')

  def testGetInitializedManager(self):
    self.sync_stage.repo = self.repo
    self.sync_stage._GetInitializedManager(True)
