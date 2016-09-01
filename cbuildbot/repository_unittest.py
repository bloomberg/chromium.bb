# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for repository.py."""

from __future__ import print_function

import os
import time

from chromite.cbuildbot import commands
from chromite.cbuildbot import config_lib
from chromite.cbuildbot import constants
from chromite.cbuildbot import repository
from chromite.lib import cros_build_lib_unittest
from chromite.lib import cros_test_lib
from chromite.lib import git
from chromite.lib import osutils
from chromite.lib import cros_build_lib


site_config = config_lib.GetConfig()


class RepositoryTests(cros_build_lib_unittest.RunCommandTestCase):
  """Test cases related to repository checkout methods."""

  def testExternalRepoCheckout(self):
    """Test we detect external checkouts properly."""
    tests = [
        'https://chromium.googlesource.com/chromiumos/manifest.git',
        'test@abcdef.bla.com:39291/bla/manifest.git',
        'test@abcdef.bla.com:39291/bla/manifest',
        'test@abcdef.bla.com:39291/bla/Manifest-internal',
    ]

    for test in tests:
      self.rc.SetDefaultCmdResult(output=test)
      self.assertFalse(repository.IsInternalRepoCheckout('.'))

  def testInternalRepoCheckout(self):
    """Test we detect internal checkouts properly."""
    tests = [
        'https://chrome-internal.googlesource.com/chromeos/manifest-internal',
        'test@abcdef.bla.com:39291/bla/manifest-internal.git',
    ]

    for test in tests:
      self.rc.SetDefaultCmdResult(output=test)
      self.assertTrue(repository.IsInternalRepoCheckout('.'))


class RepoInitTests(cros_test_lib.TempDirTestCase, cros_test_lib.MockTestCase):
  """Test cases related to repository initialization."""

  def _Initialize(self, branch='master'):
    repo = repository.RepoRepository(site_config.params.MANIFEST_URL,
                                     self.tempdir, branch=branch)
    repo.Initialize()

  @cros_test_lib.NetworkTest()
  def testReInitialization(self):
    """Test ability to switch between branches."""
    self._Initialize('release-R19-2046.B')
    self._Initialize('master')

    # Test that a failed re-init due to bad branch doesn't leave repo in bad
    # state.
    self.assertRaises(Exception, self._Initialize, 'monkey')
    self._Initialize('release-R20-2268.B')


class RepoInitChromeBotTests(RepoInitTests):
  """Test that Re-init works with the chrome-bot account.

  In testing, repo init behavior on the buildbots is different from a
  local run, because there is some logic in 'repo' that filters changes based on
  GIT_COMMITTER_IDENT.  So for sanity's sake, try to emulate running on the
  buildbots.
  """
  def setUp(self):
    os.putenv('GIT_COMMITTER_EMAIL', 'chrome-bot@chromium.org')
    os.putenv('GIT_AUTHOR_EMAIL', 'chrome-bot@chromium.org')


class PrepManifestForRepoTests(cros_test_lib.TempDirTestCase):
  """Tests for our ability to init from a local repository."""

  def testCreateManifestRepo(self):
    """Test we can create a local git repository with a local manifest."""
    CONTENTS = 'manifest contents'

    src_manifest = os.path.join(self.tempdir, 'src_manifest')
    git_repo = os.path.join(self.tempdir, 'git_repo')
    dst_manifest = os.path.join(git_repo, 'default.xml')

    osutils.WriteFile(src_manifest, CONTENTS)
    repository.PrepManifestForRepo(git_repo, src_manifest)

    self.assertEqual(CONTENTS, osutils.ReadFile(dst_manifest))

    # This should fail if we don't have a valid Git repo. Not a perfect test.
    git.GetGitRepoRevision(git_repo)

  def testUpdatingManifestRepo(self):
    """Test we can update manifest in a local git repository."""
    CONTENTS = 'manifest contents'
    CONTENTS2 = 'manifest contents - PART 2'

    src_manifest = os.path.join(self.tempdir, 'src_manifest')
    git_repo = os.path.join(self.tempdir, 'git_repo')
    dst_manifest = os.path.join(git_repo, 'default.xml')

    # Do/verify initial repo setup.
    osutils.WriteFile(src_manifest, CONTENTS)
    repository.PrepManifestForRepo(git_repo, src_manifest)

    self.assertEqual(CONTENTS, osutils.ReadFile(dst_manifest))

    # Update it.
    osutils.WriteFile(src_manifest, CONTENTS2)
    repository.PrepManifestForRepo(git_repo, src_manifest)

    self.assertEqual(CONTENTS2, osutils.ReadFile(dst_manifest))

    # Update it again with same manifest.
    repository.PrepManifestForRepo(git_repo, src_manifest)

    self.assertEqual(CONTENTS2, osutils.ReadFile(dst_manifest))


class RepoSyncTests(cros_test_lib.TempDirTestCase, cros_test_lib.MockTestCase):
  """Test cases related to repository Sync"""

  def setUp(self):
    self.repo = repository.RepoRepository(site_config.params.MANIFEST_URL,
                                          self.tempdir, branch='master')
    self.PatchObject(repository.RepoRepository, 'Initialize')
    self.PatchObject(repository.RepoRepository, '_EnsureMirroring')
    self.PatchObject(commands, 'BuildRootGitCleanup')
    self.PatchObject(time, 'sleep')

  def testSyncWithException(self):
    """Test Sync retry on repo network sync failure"""
    result = cros_build_lib.CommandResult(
        cmd=['cmd'], returncode=0, error='error')
    ex = cros_build_lib.RunCommandError('msg', result)

    run_cmd_mock = self.PatchObject(cros_build_lib, 'RunCommand',
                                    side_effect=ex)

    # repo.Sync raises SrcCheckOutException.
    self.assertRaises(
        repository.SrcCheckOutException,
        self.repo.Sync, local_manifest='local_manifest', network_only=True)

    # RunCommand should be called SYNC_RETRIES + 1 times.
    self.assertEqual(run_cmd_mock.call_count,
                     constants.SYNC_RETRIES + 1)

  def testSyncWithoutException(self):
    """Test successful repo sync without exception and retry"""
    run_cmd_mock = self.PatchObject(cros_build_lib, 'RunCommand')
    self.repo.Sync(local_manifest='local_manifest', network_only=True)

    # RunCommand should be called once.
    self.assertEqual(run_cmd_mock.call_count, 1)
