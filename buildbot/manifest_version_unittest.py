#!/usr/bin/python

# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for manifest_version. Needs to be run inside of chroot for mox."""

import mox
import os
import shutil
import sys
import tempfile
import unittest

import constants
if __name__ == '__main__':
  sys.path.insert(0, constants.SOURCE_ROOT)

from chromite.buildbot import cbuildbot_config
from chromite.buildbot import manifest_version
from chromite.buildbot import repository
from chromite.lib import cros_build_lib as cros_lib
from chromite.lib import osutils

# pylint: disable=W0212,R0904
FAKE_VERSION = """
CHROMEOS_BUILD=%(build_number)s
CHROMEOS_BRANCH=%(branch_build_number)s
CHROMEOS_PATCH=%(patch_number)s
CHROME_BRANCH=%(chrome_branch)s
"""

FAKE_VERSION_STRING = '1.2.3'
FAKE_VERSION_STRING_NEXT = '1.2.4'
CHROME_BRANCH = '13'

# Use the chromite repo to actually test git changes.
GIT_TEST_PATH = 'chromite'


def TouchFile(file_path):
  """Touches a file specified by file_path"""
  osutils.SafeMakedirs(os.path.dirname(file_path))
  osutils.WriteFile(file_path, 'w+')


class HelperMethodsTest(unittest.TestCase):
  """Test methods associated with methods not in a class."""

  def setUp(self):
    self.tmpdir = tempfile.mkdtemp()

  def testCreateSymlink(self):
    """Tests that we can create symlinks and remove a previous one."""
    (unused_fd, srcfile) = tempfile.mkstemp(dir=self.tmpdir)
    destfile1 = tempfile.mktemp(dir=os.path.join(self.tmpdir, 'other_dir1'))
    destfile2 = tempfile.mktemp(dir=os.path.join(self.tmpdir, 'other_dir2'))

    manifest_version.CreateSymlink(srcfile, destfile1, remove_file=None)
    self.assertTrue(os.path.lexists(destfile1),
                    'Unable to create symlink to %s' % destfile1)

    manifest_version.CreateSymlink(srcfile, destfile2, remove_file=destfile1)
    self.assertTrue(os.path.lexists(destfile2),
                    'Unable to create symlink to %s' % destfile2)
    self.assertFalse(os.path.lexists(destfile1),
                    'Unable to remove symlink %s' % destfile1)

  def testRemoveDirs(self):
    """Tests if _RemoveDirs works with a recursive directory structure."""
    otherdir1 = tempfile.mkdtemp(dir=self.tmpdir)
    tempfile.mkdtemp(dir=otherdir1)
    manifest_version._RemoveDirs(otherdir1)
    self.assertFalse(os.path.exists(otherdir1), 'Failed to rmdirs.')

  def testPushGitChangesWithRealPrep(self):
    """Another push test that tests push but on non-repo does it on a branch."""
    manifest_versions_url = cbuildbot_config.GetManifestVersionsRepoUrl(
        internal_build=False, read_only=False)
    git_dir = os.path.join(constants.SOURCE_ROOT, 'manifest-versions')
    manifest_version.RefreshManifestCheckout(git_dir, manifest_versions_url)
    cros_lib.CreatePushBranch(manifest_version.PUSH_BRANCH, git_dir, sync=False)
    cros_lib.RunCommand(('tee --append %s/AUTHORS' % git_dir).split(),
                        input='TEST USER <test_user@chromium.org>')
    manifest_version._PushGitChanges(git_dir, 'Test appending user.',
                                     dry_run=True)

  def tearDown(self):
    shutil.rmtree(self.tmpdir)


class VersionInfoTest(mox.MoxTestBase):
  """Test methods testing methods in VersionInfo class."""

  def setUp(self):
    mox.MoxTestBase.setUp(self)
    self.tmpdir = tempfile.mkdtemp()

  @classmethod
  def CreateFakeVersionFile(cls, tmpdir, version=FAKE_VERSION_STRING):
    """Helper method to create a version file from specified version number."""
    (version_file_fh, version_file) = tempfile.mkstemp(dir=tmpdir)
    info = manifest_version.VersionInfo(version, CHROME_BRANCH)
    os.write(version_file_fh, FAKE_VERSION % info.__dict__)
    os.close(version_file_fh)
    return version_file

  def testLoadFromFile(self):
    """Tests whether we can load from a version file."""
    version_file = self.CreateFakeVersionFile(self.tmpdir)
    info = manifest_version.VersionInfo(version_file=version_file)
    self.assertEqual(info.VersionString(), FAKE_VERSION_STRING)

  def testLoadFromString(self):
    """Tests whether we can load from a string."""
    info = manifest_version.VersionInfo(FAKE_VERSION_STRING, CHROME_BRANCH)
    self.assertEqual(info.VersionString(), FAKE_VERSION_STRING)

  def CommonTestIncrementVersion(self, incr_type, version):
    """Common test increment.  Returns path to new incremented file."""
    message = 'Incrementing cuz I sed so'
    self.mox.StubOutWithMock(cros_lib, 'CreatePushBranch')
    self.mox.StubOutWithMock(manifest_version, '_PushGitChanges')
    self.mox.StubOutWithMock(manifest_version, '_GitCleanAndCheckoutOrigin')

    cros_lib.CreatePushBranch(manifest_version.PUSH_BRANCH, self.tmpdir)

    version_file = self.CreateFakeVersionFile(self.tmpdir, version)

    manifest_version._PushGitChanges(self.tmpdir, message, dry_run=False)

    manifest_version._GitCleanAndCheckoutOrigin(self.tmpdir)
    self.mox.ReplayAll()
    info = manifest_version.VersionInfo(version_file=version_file,
                                        incr_type=incr_type)
    info.IncrementVersion(message, dry_run=False)
    self.mox.VerifyAll()
    return version_file

  def testIncrementVersionPatch(self):
    """Tests whether we can increment a version file by patch number."""
    version_file = self.CommonTestIncrementVersion('branch', '1.2.3')
    new_info = manifest_version.VersionInfo(version_file=version_file,
                                            incr_type='branch')
    self.assertEqual(new_info.VersionString(), '1.2.4')

  def testIncrementVersionBranch(self):
    """Tests whether we can increment a version file by branch number."""
    version_file = self.CommonTestIncrementVersion('branch', '1.2.0')
    new_info = manifest_version.VersionInfo(version_file=version_file,
                                            incr_type='branch')
    self.assertEqual(new_info.VersionString(), '1.3.0')

  def testIncrementVersionBuild(self):
    """Tests whether we can increment a version file by build number."""
    version_file = self.CommonTestIncrementVersion('build', '1.0.0')
    new_info = manifest_version.VersionInfo(version_file=version_file,
                                            incr_type='build')
    self.assertEqual(new_info.VersionString(), '2.0.0')

  def tearDown(self):
    shutil.rmtree(self.tmpdir)


class BuildSpecsManagerTest(mox.MoxTestBase):
  """Tests for the BuildSpecs manager."""

  def setUp(self):
    mox.MoxTestBase.setUp(self)

    self.tmpdir = tempfile.mkdtemp()
    os.makedirs(os.path.join(self.tmpdir, '.repo'))
    self.source_repo = 'ssh://source/repo'
    self.manifest_repo = 'ssh://manifest/repo'
    self.version_file = 'version-file.sh'
    self.branch = 'master'
    self.build_name = 'x86-generic'
    self.incr_type = 'branch'

    repo = repository.RepoRepository(
      self.source_repo, self.tmpdir, self.branch)
    self.manager = manifest_version.BuildSpecsManager(
      repo, self.manifest_repo, self.build_name, self.incr_type, False,
      dry_run=True)

    # Change default to something we clean up.
    self.tmpmandir = tempfile.mkdtemp()
    self.manager.manifest_dir = self.tmpmandir

  def testLoadSpecs(self):
    """Tests whether we can load specs correctly."""
    info = manifest_version.VersionInfo(
        FAKE_VERSION_STRING, CHROME_BRANCH, incr_type='branch')
    m1 = os.path.join(self.manager.manifest_dir, 'buildspecs',
                      CHROME_BRANCH, '1.2.2.xml')
    m2 = os.path.join(self.manager.manifest_dir, 'buildspecs',
                      CHROME_BRANCH, '1.2.3.xml')
    m3 = os.path.join(self.manager.manifest_dir, 'buildspecs',
                      CHROME_BRANCH, '1.2.4.xml')
    m4 = os.path.join(self.manager.manifest_dir, 'buildspecs',
                      CHROME_BRANCH, '1.2.5.xml')
    for_build = os.path.join(self.manager.manifest_dir, 'build-name',
                             self.build_name)

    # Create fake buildspecs.
    TouchFile(m1)
    TouchFile(m2)
    TouchFile(m3)
    TouchFile(m4)

    # Fail 1, pass 2, leave 3,4 unprocessed.
    manifest_version.CreateSymlink(m1, os.path.join(
        for_build, 'fail', CHROME_BRANCH, os.path.basename(m1)))
    manifest_version.CreateSymlink(m1, os.path.join(
        for_build, 'pass', CHROME_BRANCH, os.path.basename(m2)))
    self.mox.StubOutWithMock(self.manager, '_GetSpecAge')
    self.manager._GetSpecAge('1.2.5').AndReturn(100)
    self.mox.ReplayAll()
    self.manager.InitializeManifestVariables(info)
    self.mox.VerifyAll()
    self.assertEqual(self.manager.latest_unprocessed, '1.2.5')

  def testLatestSpecFromDir(self):
    """Tests whether we can get sorted specs correctly from a directory."""
    self.mox.StubOutWithMock(manifest_version, '_RemoveDirs')
    self.mox.StubOutWithMock(repository, 'CloneGitRepo')
    info = manifest_version.VersionInfo(
        '99.1.2', CHROME_BRANCH, incr_type='branch')

    specs_dir = os.path.join(self.manager.manifest_dir, 'buildspecs',
                             CHROME_BRANCH)
    m1 = os.path.join(specs_dir, '100.0.0.xml')
    m2 = os.path.join(specs_dir, '99.3.3.xml')
    m3 = os.path.join(specs_dir, '99.1.10.xml')
    m4 = os.path.join(specs_dir, '99.1.5.xml')

    # Create fake buildspecs.
    TouchFile(m1)
    TouchFile(m2)
    TouchFile(m3)
    TouchFile(m4)

    self.mox.ReplayAll()
    spec = self.manager._LatestSpecFromDir(info, specs_dir)
    self.mox.VerifyAll()
    # Should be the latest on the 99.1 branch.
    self.assertEqual(spec, '99.1.10')

  def testGetNextVersionNoIncrement(self):
    """Tests whether we can get the next version to be built correctly.

    Tests without pre-existing version in manifest dir.
    """
    info = manifest_version.VersionInfo(
        FAKE_VERSION_STRING, CHROME_BRANCH, incr_type='branch')

    self.manager.latest = None
    self.mox.ReplayAll()
    version = self.manager.GetNextVersion(info)
    self.mox.VerifyAll()
    self.assertEqual(FAKE_VERSION_STRING, version)

  def testGetNextVersionIncrement(self):
    """Tests that we create a new version if a previous one exists."""
    self.mox.StubOutWithMock(manifest_version.VersionInfo, 'IncrementVersion')
    version_file = VersionInfoTest.CreateFakeVersionFile(self.tmpdir)
    info = manifest_version.VersionInfo(version_file=version_file,
                                        incr_type='branch')
    info.IncrementVersion(
        'Automatic: %s - Updating to a new version number from %s' % (
            self.build_name, FAKE_VERSION_STRING),
        dry_run=True).AndReturn(FAKE_VERSION_STRING_NEXT)

    self.manager.latest = FAKE_VERSION_STRING
    self.mox.ReplayAll()
    version = self.manager.GetNextVersion(info)
    self.mox.VerifyAll()
    self.assertEqual(FAKE_VERSION_STRING_NEXT, version)

  def NotestGetNextBuildSpec(self):
    """Meta test.  Re-enable if you want to use it to do a big test."""
    print self.manager.GetNextBuildSpec(retries=0)
    print self.manager.UpdateStatus('pass')

  def tearDown(self):
    if os.path.exists(self.tmpdir): shutil.rmtree(self.tmpdir)
    shutil.rmtree(self.tmpmandir)


if __name__ == '__main__':
  unittest.main()
