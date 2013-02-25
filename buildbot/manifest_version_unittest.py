#!/usr/bin/python

# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for manifest_version. Needs to be run inside of chroot for mox."""

import os
import sys
import tempfile

import constants
if __name__ == '__main__':
  sys.path.insert(0, constants.SOURCE_ROOT)

from chromite.buildbot import cbuildbot_config
from chromite.buildbot import manifest_version
from chromite.buildbot import repository
from chromite.lib import cros_build_lib
from chromite.lib import git
from chromite.lib import cros_test_lib
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


class HelperMethodsTest(cros_test_lib.TempDirTestCase):
  """Test methods associated with methods not in a class."""

  def testCreateSymlink(self):
    """Tests that we can create symlinks and remove a previous one."""
    srcfile = os.path.join(self.tempdir, 'src')
    osutils.Touch(srcfile)
    other_dir = os.path.join(self.tempdir, 'other_dir')
    os.makedirs(other_dir)
    destfile = os.path.join(other_dir, 'dest')

    manifest_version.CreateSymlink(srcfile, destfile)
    self.assertTrue(os.path.lexists(destfile),
                    'Unable to create symlink to %s' % destfile)

  def testRemoveDirs(self):
    """Tests if _RemoveDirs works with a recursive directory structure."""
    otherdir = os.path.join(self.tempdir, "foo")
    osutils.SafeMakedirs(os.path.join(otherdir, "bar"))
    manifest_version._RemoveDirs(otherdir)
    self.assertFalse(os.path.exists(otherdir), 'Failed to rmdirs.')

  def testPushGitChangesWithRealPrep(self):
    """Another push test that tests push but on non-repo does it on a branch."""
    manifest_versions_url = cbuildbot_config.GetManifestVersionsRepoUrl(
        internal_build=False, read_only=False)
    git_dir = os.path.join(constants.SOURCE_ROOT, 'manifest-versions')
    manifest_version.RefreshManifestCheckout(git_dir, manifest_versions_url)
    git.CreatePushBranch(manifest_version.PUSH_BRANCH, git_dir, sync=False)
    cros_build_lib.RunCommand(('tee --append %s/AUTHORS' % git_dir).split(),
                              input='TEST USER <test_user@chromium.org>')
    manifest_version._PushGitChanges(git_dir, 'Test appending user.',
                                     dry_run=True)


class VersionInfoTest(cros_test_lib.MoxTempDirTestCase):
  """Test methods testing methods in VersionInfo class."""

  @classmethod
  def WriteFakeVersionFile(cls, version_file, version=FAKE_VERSION_STRING):
    """Helper method to write a version file from specified version number."""
    osutils.SafeMakedirs(os.path.split(version_file)[0])
    info = manifest_version.VersionInfo(version, CHROME_BRANCH)
    osutils.WriteFile(version_file, FAKE_VERSION % info.__dict__)

  @classmethod
  def CreateFakeVersionFile(cls, tmpdir, version=FAKE_VERSION_STRING):
    """Helper method to create a version file from specified version number."""
    version_file = tempfile.mktemp(dir=tmpdir)
    cls.WriteFakeVersionFile(version_file, version=version)
    return version_file

  def testLoadFromFile(self):
    """Tests whether we can load from a version file."""
    version_file = self.CreateFakeVersionFile(self.tempdir)
    info = manifest_version.VersionInfo(version_file=version_file)
    self.assertEqual(info.VersionString(), FAKE_VERSION_STRING)

  def testLoadFromRepo(self):
    """Tests whether we can load from a source repo."""
    version_file = os.path.join(self.tempdir, constants.VERSION_FILE)
    self.WriteFakeVersionFile(version_file)
    info = manifest_version.VersionInfo.from_repo(self.tempdir)
    self.assertEqual(info.VersionString(), FAKE_VERSION_STRING)

  def testLoadFromString(self):
    """Tests whether we can load from a string."""
    info = manifest_version.VersionInfo(FAKE_VERSION_STRING, CHROME_BRANCH)
    self.assertEqual(info.VersionString(), FAKE_VERSION_STRING)

  def CommonTestIncrementVersion(self, incr_type, version):
    """Common test increment.  Returns path to new incremented file."""
    message = 'Incrementing cuz I sed so'
    self.mox.StubOutWithMock(git, 'CreatePushBranch')
    self.mox.StubOutWithMock(manifest_version, '_PushGitChanges')
    self.mox.StubOutWithMock(git, 'CleanAndCheckoutUpstream')

    git.CreatePushBranch(manifest_version.PUSH_BRANCH, self.tempdir)

    version_file = self.CreateFakeVersionFile(self.tempdir, version)

    manifest_version._PushGitChanges(self.tempdir, message, dry_run=False)

    git.CleanAndCheckoutUpstream(self.tempdir)
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


class BuildSpecsManagerTest(cros_test_lib.MoxTempDirTestCase):
  """Tests for the BuildSpecs manager."""

  def setUp(self):
    os.makedirs(os.path.join(self.tempdir, '.repo'))
    self.source_repo = 'ssh://source/repo'
    self.manifest_repo = 'ssh://manifest/repo'
    self.version_file = 'version-file.sh'
    self.branch = 'master'
    self.build_name = 'x86-generic'
    self.incr_type = 'branch'

    repo = repository.RepoRepository(
      self.source_repo, self.tempdir, self.branch)
    self.manager = manifest_version.BuildSpecsManager(
      repo, self.manifest_repo, self.build_name, self.incr_type, False,
      branch=self.branch, dry_run=True)

    # Change default to something we clean up.
    self.tmpmandir = os.path.join(self.tempdir, "man")
    osutils.SafeMakedirs(self.tmpmandir)
    self.manager.manifest_dir = self.tmpmandir

  def testLoadSpecs(self):
    """Tests whether we can load specs correctly."""
    info = manifest_version.VersionInfo(
        FAKE_VERSION_STRING, CHROME_BRANCH, incr_type='branch')
    mpath = os.path.join(self.manager.manifest_dir, 'buildspecs', CHROME_BRANCH)
    m1, m2, m3, m4 = [os.path.join(mpath, '1.2.%d.xml' % x)
                      for x in [2,3,4,5]]
    for_build = os.path.join(self.manager.manifest_dir, 'build-name',
                             self.build_name)

    # Create fake buildspecs.
    osutils.SafeMakedirs(os.path.join(mpath))
    for m in [m1, m2, m3, m4]: osutils.Touch(m)

    # Fail 1, pass 2, leave 3,4 unprocessed.
    manifest_version.CreateSymlink(m1, os.path.join(
        for_build, 'fail', CHROME_BRANCH, os.path.basename(m1)))
    manifest_version.CreateSymlink(m1, os.path.join(
        for_build, 'pass', CHROME_BRANCH, os.path.basename(m2)))
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
    m1, m2, m3, m4 = [os.path.join(specs_dir, x)
                      for x in ['100.0.0.xml', '99.3.3.xml', '99.1.10.xml',
                                '99.1.5.xml']]

    # Create fake buildspecs.
    osutils.SafeMakedirs(specs_dir)
    for m in [m1, m2, m3, m4]: osutils.Touch(m)

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
    version_file = VersionInfoTest.CreateFakeVersionFile(self.tempdir)
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


if __name__ == '__main__':
  cros_test_lib.main()
