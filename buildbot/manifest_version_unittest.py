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

if __name__ == '__main__':
  import constants
  sys.path.append(constants.SOURCE_ROOT)

from chromite.buildbot import manifest_version
from chromite.buildbot import repository
from chromite.lib import cros_build_lib as cros_lib

FAKE_VERSION = """
CHROMEOS_VERSION_MAJOR=1
CHROMEOS_VERSION_MINOR=2
CHROMEOS_VERSION_BRANCH=3
CHROMEOS_VERSION_PATCH=4
"""

FAKE_VERSION_STRING = '1.2.3.4'
FAKE_VERSION_STRING_NEXT = '1.2.3.5'

# Dir to use to sync repo for git testing.
GIT_DIR = '/tmp/repo_for_manifest_version_unittest'

# Use the chromite repo to actually test git changes.
GIT_TEST_PATH = 'chromite'

def TouchFile(file_path):
  """Touches a file specified by file_path"""
  if not os.path.exists(os.path.dirname(file_path)):
    os.makedirs(os.path.dirname(file_path))

  touch_file = open(file_path, 'w+')
  touch_file.close()


class HelperMethodsTest(unittest.TestCase):
  """Test methods associated with methods not in a class."""

  def setUp(self):
    self.tmpdir = tempfile.mkdtemp()

  def testCreateSymlink(self):
    """Tests that we can create symlinks and remove a previous one."""
    (unused_fd, srcfile) = tempfile.mkstemp(dir=self.tmpdir)
    destfile1 = tempfile.mktemp(dir=os.path.join(self.tmpdir, 'other_dir1'))
    destfile2 = tempfile.mktemp(dir=os.path.join(self.tmpdir, 'other_dir2'))

    manifest_version._CreateSymlink(srcfile, destfile1, remove_file=None)
    self.assertTrue(os.path.lexists(destfile1),
                    'Unable to create symlink to %s' % destfile1)

    manifest_version._CreateSymlink(srcfile, destfile2, remove_file=destfile1)
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

  def testPushGitChanges(self):
    """Tests if we can append to an authors file and push it using dryrun."""
    if not os.path.exists(GIT_DIR): os.makedirs(GIT_DIR)
    cros_lib.RunCommand(
        ('repo init -u http://git.chromium.org/chromiumos/manifest.git '
         '-m minilayout.xml -q').split(), cwd=GIT_DIR, input='\n\ny\n')
    cros_lib.RunCommand(('repo sync --jobs 8').split(), cwd=GIT_DIR)
    git_dir = os.path.join(GIT_DIR, GIT_TEST_PATH)
    cros_lib.RunCommand(
        ('git config url.ssh://gerrit.chromium.org:29418.insteadof'
         ' http://git.chromium.org').split(), cwd=git_dir)


    manifest_version._PrepForChanges(git_dir)

    # Change something.
    cros_lib.RunCommand(('tee --append %s/AUTHORS' % git_dir).split(),
                        input='TEST USER <test_user@chromium.org>')

    # Push the change with dryrun.
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
  def CreateFakeVersionFile(cls, tmpdir):
    """Helper method to create a version file from FAKE_VERSION."""
    (version_file_fh, version_file) = tempfile.mkstemp(dir=tmpdir)
    os.write(version_file_fh, FAKE_VERSION)
    os.close(version_file_fh)
    return version_file

  def testLoadFromFile(self):
    """Tests whether we can load from a version file."""
    version_file = self.CreateFakeVersionFile(self.tmpdir)
    info = manifest_version.VersionInfo(version_file=version_file)
    self.assertEqual(info.VersionString(), FAKE_VERSION_STRING)

  def testLoadFromString(self):
    """Tests whether we can load from a string."""
    info = manifest_version.VersionInfo(version_string=FAKE_VERSION_STRING)
    self.assertEqual(info.VersionString(), FAKE_VERSION_STRING)

  def testIncrementVersionPatch(self):
    """Tests whether we can increment a version file."""
    message = 'Incrementing cuz I sed so'
    self.mox.StubOutWithMock(manifest_version, '_PrepForChanges')
    self.mox.StubOutWithMock(manifest_version, '_PushGitChanges')

    manifest_version._PrepForChanges(self.tmpdir, use_repo=True)

    version_file = self.CreateFakeVersionFile(self.tmpdir)

    manifest_version._PushGitChanges(self.tmpdir, message, dry_run=False)

    self.mox.ReplayAll()
    info = manifest_version.VersionInfo(version_file=version_file,
                                         incr_type='patch')
    info.IncrementVersion(message, dry_run=False)
    new_info = manifest_version.VersionInfo(version_file=version_file,
                                             incr_type='patch')
    self.assertEqual(new_info.VersionString(), FAKE_VERSION_STRING_NEXT)
    self.mox.VerifyAll()

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
    self.incr_type = 'patch'

    self.manager = manifest_version.BuildSpecsManager(
      self.tmpdir, self.source_repo, self.manifest_repo, self.branch,
      self.build_name, self.incr_type, dry_run=True)

  def testLoadSpecs(self):
    """Tests whether we can load specs correctly."""
    self.mox.StubOutWithMock(manifest_version, '_RemoveDirs')
    self.mox.StubOutWithMock(manifest_version, '_CloneGitRepo')
    info = manifest_version.VersionInfo(version_string=FAKE_VERSION_STRING,
                                        incr_type='patch')
    dir_pfx = '1.2'
    m1 = os.path.join(self.manager.manifests_dir, 'buildspecs', dir_pfx,
                      '1.2.3.2.xml')
    m2 = os.path.join(self.manager.manifests_dir, 'buildspecs', dir_pfx,
                      '1.2.3.3.xml')
    m3 = os.path.join(self.manager.manifests_dir, 'buildspecs', dir_pfx,
                      '1.2.3.4.xml')
    m4 = os.path.join(self.manager.manifests_dir, 'buildspecs', dir_pfx,
                      '1.2.3.5.xml')
    for_build = os.path.join(self.manager.manifests_dir, 'build-name',
                             self.build_name)

    # Create fake buildspecs.
    TouchFile(m1)
    TouchFile(m2)
    TouchFile(m3)
    TouchFile(m4)

    # Fail 1, pass 2, leave 3,4 unprocessed.
    manifest_version._CreateSymlink(m1, os.path.join(for_build, 'fail', dir_pfx,
                                                     os.path.basename(m1)))
    manifest_version._CreateSymlink(m1, os.path.join(for_build, 'pass', dir_pfx,
                                                     os.path.basename(m2)))
    manifest_version._RemoveDirs(self.manager.manifests_dir)
    manifest_version._CloneGitRepo(self.manager.manifests_dir,
                                   self.manifest_repo)
    self.mox.ReplayAll()
    self.manager._LoadSpecs(info)
    self.mox.VerifyAll()
    self.assertEqual(self.manager.latest_unprocessed, '1.2.3.5')

  def testGetMatchingSpecs(self):
    """Tests whether we can get sorted specs correctly from a directory."""
    self.mox.StubOutWithMock(manifest_version, '_RemoveDirs')
    self.mox.StubOutWithMock(manifest_version, '_CloneGitRepo')
    info = manifest_version.VersionInfo(version_string=FAKE_VERSION_STRING,
                                         incr_type='patch')
    dir_pfx = '1.2'
    specs_dir = os.path.join(self.manager.manifests_dir, 'buildspecs', dir_pfx)
    m1 = os.path.join(specs_dir, '1.2.3.5.xml')
    m2 = os.path.join(specs_dir, '1.2.3.10.xml')
    m3 = os.path.join(specs_dir, '1.2.555.6.xml')
    m4 = os.path.join(specs_dir, '1.2.3.4.xml')

    # Create fake buildspecs.
    TouchFile(m1)
    TouchFile(m2)
    TouchFile(m3)
    TouchFile(m4)

    self.mox.ReplayAll()
    specs = self.manager._GetMatchingSpecs(info, specs_dir)
    self.mox.VerifyAll()
    # Should be the latest on the 1.2.3 branch.
    self.assertEqual(specs[-1], '1.2.555.6')

  def testCreateNewBuildSpecNoCopy(self):
    """Tests whether we can create a new build spec correctly.

    Tests without pre-existing version file in manifest dir.
    """
    self.mox.StubOutWithMock(repository.RepoRepository, 'ExportManifest')
    self.mox.StubOutWithMock(manifest_version, '_PrepForChanges')
    self.mox.StubOutWithMock(manifest_version, '_PushGitChanges')

    info = manifest_version.VersionInfo(version_string=FAKE_VERSION_STRING,
                                         incr_type='patch')
    self.manager.all_specs_dir = os.path.join(self.manager.manifests_dir,
                                              'buildspecs', '1.2')
    manifest_version._PrepForChanges(mox.IsA(str))
    repository.RepoRepository.ExportManifest(mox.IgnoreArg())
    manifest_version._PushGitChanges(mox.IsA(str), mox.IsA(str), dry_run=True)

    self.mox.ReplayAll()
    self.manager.all = []
    version = self.manager._CreateNewBuildSpec(info)
    self.mox.VerifyAll()
    self.assertEqual(FAKE_VERSION_STRING, version)

  def testCreateNewBuildIncrement(self):
    """Tests that we create a new version if a previous one exists."""
    self.mox.StubOutWithMock(manifest_version.VersionInfo, 'IncrementVersion')
    self.mox.StubOutWithMock(repository.RepoRepository, 'ExportManifest')

    self.mox.StubOutWithMock(repository.RepoRepository, 'Sync')
    self.mox.StubOutWithMock(manifest_version, '_PrepForChanges')
    self.mox.StubOutWithMock(manifest_version, '_PushGitChanges')

    version_file = VersionInfoTest.CreateFakeVersionFile(self.tmpdir)
    info = manifest_version.VersionInfo(version_file=version_file,
                                         incr_type='patch')
    self.manager.all_specs_dir = os.path.join(self.manager.manifests_dir,
                                              'buildspecs', '1.2')
    info.IncrementVersion('Automatic: Updating the new version number %s' %
                          FAKE_VERSION_STRING, dry_run=True).AndReturn(
                              FAKE_VERSION_STRING_NEXT)
    repository.RepoRepository.Sync('default')
    manifest_version._PrepForChanges(mox.IsA(str))
    repository.RepoRepository.ExportManifest(mox.IgnoreArg())
    manifest_version._PushGitChanges(mox.IsA(str), mox.IsA(str), dry_run=True)

    self.mox.ReplayAll()
    # Add to existing so we are forced to increment.
    self.manager.all = [FAKE_VERSION_STRING]
    version = self.manager._CreateNewBuildSpec(info)
    self.mox.VerifyAll()
    self.assertEqual(FAKE_VERSION_STRING_NEXT, version)

  def NotestGetNextBuildSpec(self):
    """Meta test.  Re-enable if you want to use it to do a big test."""
    print self.manager.GetNextBuildSpec(self.version_file, latest=True,
                                        retries=0)
    print self.manager.UpdateStatus('pass')

  def tearDown(self):
    if os.path.exists(self.tmpdir):
      shutil.rmtree(self.tmpdir)


if __name__ == '__main__':
  unittest.main()
