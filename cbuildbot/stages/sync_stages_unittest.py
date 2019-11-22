# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for sync stages."""

from __future__ import print_function

import os

import mock

from chromite.cbuildbot import commands
from chromite.cbuildbot import lkgm_manager
from chromite.cbuildbot import manifest_version
from chromite.cbuildbot import repository
from chromite.cbuildbot import trybot_patch_pool
from chromite.cbuildbot.stages import generic_stages_unittest
from chromite.cbuildbot.stages import sync_stages
from chromite.lib import auth
from chromite.lib import buildbucket_lib
from chromite.lib import cidb
from chromite.lib import constants
from chromite.lib import cros_test_lib
from chromite.lib import fake_cidb
from chromite.lib import metadata_lib
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
