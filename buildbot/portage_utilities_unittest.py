#!/usr/bin/python

# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for portage_utilities.py."""

import fileinput
import mox
import os
import re
import shutil
import sys
import tempfile
import unittest

import constants
if __name__ == '__main__':
  sys.path.insert(0, constants.SOURCE_ROOT)

from chromite.lib import cros_build_lib
from chromite.buildbot import patch as cros_patch
from chromite.buildbot import gerrit_helper
from chromite.buildbot import portage_utilities


class _Package(object):
  def __init__(self, package):
    self.package = package


class _DummyCommandResult(object):
  """Create mock RunCommand results."""
  def __init__(self, output):
    # Members other than 'output' are expected to be unused, so
    # we omit them here.
    #
    # All shell output will be newline terminated; we add the
    # newline here for convenience.
    self.output = output + '\n'


class EBuildTest(mox.MoxTestBase):

  def setUp(self):
    mox.MoxTestBase.setUp(self)

  def _makeFakeEbuild(self, fake_ebuild_path):
    self.mox.StubOutWithMock(fileinput, 'input')
    fileinput.input(fake_ebuild_path).AndReturn('')
    self.mox.ReplayAll()
    fake_ebuild = portage_utilities.EBuild(fake_ebuild_path)
    self.mox.VerifyAll()
    return fake_ebuild

  def testParseEBuildPath(self):
    # Test with ebuild with revision number.
    fake_ebuild_path = '/path/to/test_package/test_package-0.0.1-r1.ebuild'
    fake_ebuild = self._makeFakeEbuild(fake_ebuild_path)

    self.assertEquals(fake_ebuild._category, 'to')
    self.assertEquals(fake_ebuild._pkgname, 'test_package')
    self.assertEquals(fake_ebuild._version_no_rev, '0.0.1')
    self.assertEquals(fake_ebuild.current_revision, 1)
    self.assertEquals(fake_ebuild.version, '0.0.1-r1')
    self.assertEquals(fake_ebuild.package, 'to/test_package')
    self.assertEquals(fake_ebuild._ebuild_path_no_version,
                      '/path/to/test_package/test_package')
    self.assertEquals(fake_ebuild.ebuild_path_no_revision,
                      '/path/to/test_package/test_package-0.0.1')
    self.assertEquals(fake_ebuild._unstable_ebuild_path,
                      '/path/to/test_package/test_package-9999.ebuild')
    self.assertEquals(fake_ebuild.ebuild_path, fake_ebuild_path)

  def testParseEBuildPathNoRevisionNumber(self):
    # Test with ebuild without revision number.
    fake_ebuild_path = '/path/to/test_package/test_package-9999.ebuild'
    fake_ebuild = self._makeFakeEbuild(fake_ebuild_path)

    self.assertEquals(fake_ebuild._category, 'to')
    self.assertEquals(fake_ebuild._pkgname, 'test_package')
    self.assertEquals(fake_ebuild._version_no_rev, '9999')
    self.assertEquals(fake_ebuild.current_revision, 0)
    self.assertEquals(fake_ebuild.version, '9999')
    self.assertEquals(fake_ebuild.package, 'to/test_package')
    self.assertEquals(fake_ebuild._ebuild_path_no_version,
                      '/path/to/test_package/test_package')
    self.assertEquals(fake_ebuild.ebuild_path_no_revision,
                      '/path/to/test_package/test_package-9999')
    self.assertEquals(fake_ebuild._unstable_ebuild_path,
                      '/path/to/test_package/test_package-9999.ebuild')
    self.assertEquals(fake_ebuild.ebuild_path, fake_ebuild_path)

  def testGetCommitId(self):
    fake_sources = '/path/to/sources'
    fake_category = 'test_category'
    fake_package = 'test_package'
    fake_ebuild_path = os.path.join(fake_sources, 'overlay',
                                    fake_category, fake_package,
                                    fake_package + '-9999.ebuild')
    fake_ebuild = self._makeFakeEbuild(fake_ebuild_path)
    self.mox.UnsetStubs()

    fake_hash = '24ab3c9f6d6b5c744382dba2ca8fb444b9808e9f'
    fake_project = 'chromiumos/third_party/test_project'
    fake_subdir = 'test_project/'

    # We expect Die() will not be called; mock it out so that if
    # we're wrong, it won't just terminate the test.
    self.mox.StubOutWithMock(portage_utilities.cros_build_lib, 'Die')
    self.mox.StubOutWithMock(portage_utilities.os.path, 'isdir')
    self.mox.StubOutWithMock(
        portage_utilities.cros_build_lib, 'RunCommand')

    # The first RunCommand does 'grep' magic on the ebuild.
    result1 = _DummyCommandResult(' '.join([fake_project, fake_subdir]))
    portage_utilities.cros_build_lib.RunCommand(
        mox.IgnoreArg(),
        print_cmd=fake_ebuild.VERBOSE,
        redirect_stdout=True,
        shell=True).AndReturn(result1)

    # ... the result is used to construct a path, which the EBuild
    # code expects to be a directory.  The path passes through
    # os.path.realpath(), which strips the trailing '/' from
    # fake_subdir.
    expected_dir = os.path.join(
        fake_sources, 'third_party', fake_subdir[0:-1])
    portage_utilities.os.path.isdir(expected_dir).AndReturn(True)

    # ... the next RunCommand does 'git config --get ...'
    result2 = _DummyCommandResult(fake_project)
    portage_utilities.cros_build_lib.RunCommand(
        mox.IgnoreArg(),
        print_cmd=fake_ebuild.VERBOSE,
        redirect_stdout=True,
        shell=True).AndReturn(result2)

    # ... and the final RunCommand does 'git rev-parse HEAD'
    result3 = _DummyCommandResult(fake_hash)
    portage_utilities.cros_build_lib.RunCommand(
        mox.IgnoreArg(),
        print_cmd=fake_ebuild.VERBOSE,
        redirect_stdout=True,
        shell=True).AndReturn(result3)

    self.mox.ReplayAll()
    test_hash = fake_ebuild.GetCommitId(fake_sources)
    self.mox.VerifyAll()
    self.assertEquals(test_hash, fake_hash)


class StubEBuild(portage_utilities.EBuild):
  def __init__(self, path):
    super(StubEBuild, self).__init__(path)
    self.is_workon = True
    self.is_stable = True

  def _ReadEBuild(self, path):
    pass

  def GetCommitId(self, srcroot):
    if srcroot == '/sources':
      return 'my_id'
    else:
      return 'you_lose'


class EBuildRevWorkonTest(mox.MoxTestBase):
  # Lines that we will feed as fake ebuild contents to
  # EBuild.MarAsStable().  This is the minimum content needed
  # to test the various branches in the function's main processing
  # loop.
  _mock_ebuild = ['EAPI=2\n',
                  'CROS_WORKON_COMMIT=old_id\n',
                  'KEYWORDS=\"~x86 ~arm\"\n',
                  'src_unpack(){}\n']

  def setUp(self):
    mox.MoxTestBase.setUp(self)
    package_name = '/sources/overlay/category/test_package/test_package-0.0.1'
    ebuild_path = package_name + '-r1.ebuild'
    self.m_ebuild = StubEBuild(ebuild_path)
    self.revved_ebuild_path = package_name + '-r2.ebuild'

  def createRevWorkOnMocks(self, ebuild_content):
    self.mox.StubOutWithMock(portage_utilities.os.path, 'exists')
    self.mox.StubOutWithMock(portage_utilities.cros_build_lib, 'Die')
    self.mox.StubOutWithMock(portage_utilities.shutil, 'copyfile')
    self.mox.StubOutWithMock(portage_utilities.os, 'unlink')
    self.mox.StubOutWithMock(portage_utilities.EBuild, '_RunCommand')
    self.mox.StubOutWithMock(cros_build_lib, 'RunCommand')
    self.mox.StubOutWithMock(portage_utilities.filecmp, 'cmp')
    self.mox.StubOutWithMock(portage_utilities.fileinput, 'input')
    self.mox.StubOutWithMock(portage_utilities.EBuild, 'GetVersion')

    portage_utilities.EBuild.GetVersion('/sources', '0.0.1').AndReturn('0.0.1')

    ebuild_9999 = self.m_ebuild._unstable_ebuild_path
    portage_utilities.os.path.exists(ebuild_9999).AndReturn(True)

    # These calls come from MarkAsStable()
    portage_utilities.shutil.copyfile(ebuild_9999, self.revved_ebuild_path)

    m_file = self.mox.CreateMock(file)
    portage_utilities.fileinput.input(self.revved_ebuild_path,
                                      inplace=1).AndReturn(ebuild_content)
    m_file.write('EAPI=2\n')
    m_file.write('CROS_WORKON_COMMIT="my_id"\n')
    m_file.write('KEYWORDS=\"x86 arm\"\n')
    m_file.write('src_unpack(){}\n')
    # MarkAsStable() returns here

    return m_file

  def testRevWorkOnEBuild(self):
    m_file = self.createRevWorkOnMocks(self._mock_ebuild)
    portage_utilities.filecmp.cmp(self.m_ebuild.ebuild_path,
                                  self.revved_ebuild_path,
                                  shallow=False).AndReturn(False)
    portage_utilities.EBuild._RunCommand(
      'git add ' + self.revved_ebuild_path)
    portage_utilities.EBuild._RunCommand(
      'git rm ' + self.m_ebuild.ebuild_path)
    message = portage_utilities._GIT_COMMIT_MESSAGE % (
      self.m_ebuild.package, 'my_id')
    cros_build_lib.RunCommand(
        ['git', 'commit', '-a', '-m', message], cwd='.', print_cmd=False)

    self.mox.ReplayAll()
    result = self.m_ebuild.RevWorkOnEBuild('/sources', redirect_file=m_file)
    self.mox.VerifyAll()
    self.assertEqual(result, 'category/test_package-0.0.1-r2')

  def testRevUnchangedEBuild(self):
    m_file = self.createRevWorkOnMocks(self._mock_ebuild)
    portage_utilities.filecmp.cmp(self.m_ebuild.ebuild_path,
                                  self.revved_ebuild_path,
                                  shallow=False).AndReturn(True)
    portage_utilities.os.unlink(self.revved_ebuild_path)

    self.mox.ReplayAll()
    result = self.m_ebuild.RevWorkOnEBuild('/sources', redirect_file=m_file)
    self.mox.VerifyAll()
    self.assertEqual(result, None)

  def testRevMissingEBuild(self):
    self.revved_ebuild_path = self.m_ebuild.ebuild_path
    self.m_ebuild.ebuild_path = self.m_ebuild._unstable_ebuild_path
    self.m_ebuild.current_revision = 0
    self.m_ebuild.is_stable = False

    m_file = self.createRevWorkOnMocks(
      self._mock_ebuild[0:1] + self._mock_ebuild[2:])
    portage_utilities.filecmp.cmp(self.m_ebuild.ebuild_path,
                                  self.revved_ebuild_path,
                                  shallow=False).AndReturn(False)
    portage_utilities.EBuild._RunCommand(
      'git add ' + self.revved_ebuild_path)
    message = portage_utilities._GIT_COMMIT_MESSAGE % (
      self.m_ebuild.package, 'my_id')

    cros_build_lib.RunCommand(
        ['git', 'commit', '-a', '-m', message], cwd='.', print_cmd=False)

    self.mox.ReplayAll()
    result = self.m_ebuild.RevWorkOnEBuild('/sources', redirect_file=m_file)
    self.mox.VerifyAll()
    self.assertEqual(result, 'category/test_package-0.0.1-r1')

  def testCommitChange(self):
    self.mox.StubOutWithMock(cros_build_lib, 'RunCommand')
    mock_message = 'Commitme'
    cros_build_lib.RunCommand(
        ['git', 'commit', '-a', '-m', mock_message], cwd='.', print_cmd=False)
    self.mox.ReplayAll()
    self.m_ebuild.CommitChange(mock_message)
    self.mox.VerifyAll()

  def testUpdateCommitHashesForChanges(self):
    """Tests that we can update the commit hashes for changes correctly."""
    ebuild1 = self.mox.CreateMock(portage_utilities.EBuild)
    ebuild1.ebuild_path = 'public_overlay/ebuild.ebuild'
    ebuild1.package = 'test/project'

    # Mimics the behavior of BuildEBuildDictionary.
    def addEBuild(overlay_dict, *other_args):
      overlay_dict['public_overlay'] = [ebuild1]

    self.mox.StubOutWithMock(portage_utilities, 'BuildEBuildDictionary')
    self.mox.StubOutWithMock(portage_utilities, 'FindOverlays')
    self.mox.StubOutWithMock(portage_utilities.EBuild, 'UpdateEBuild')
    self.mox.StubOutWithMock(portage_utilities.EBuild, 'CommitChange')
    self.mox.StubOutWithMock(cros_build_lib, 'RunCommand')
    self.mox.StubOutWithMock(portage_utilities.EBuild, 'GitRepoHasChanges')

    patch1 = self.mox.CreateMock(cros_patch.GerritPatch)
    patch2 = self.mox.CreateMock(cros_patch.GerritPatch)

    patch1.id = 'ChangeId1'
    patch2.id = 'ChangeId2'
    patch1.internal = False
    patch2.internal = False
    patch1.project = 'fake_project1'
    patch2.project = 'fake_project2'
    patch1.tracking_branch = 'chromiumos/chromite'
    patch2.tracking_branch = 'not/a/real/project'

    build_root = 'fakebuildroot'
    overlays = ['public_overlay']
    portage_utilities.FindOverlays(build_root, 'both').AndReturn(overlays)
    overlay_dict = dict(public_overlay=[])
    portage_utilities.BuildEBuildDictionary(overlay_dict,
                                            True, None).WithSideEffects(
                                                addEBuild)

    ebuild1.GetSourcePath(os.path.join(build_root, 'src')).AndReturn(
        ('fake_project1', 'p1_path'))

    self.mox.StubOutWithMock(gerrit_helper, 'GetGerritHelperForChange')
    helper = self.mox.CreateMock(gerrit_helper.GerritHelper)
    self.mox.StubOutWithMock(helper, 'GetLatestSHA1ForBranch')
    gerrit_helper.GetGerritHelperForChange(patch1).AndReturn(helper)
    helper.GetLatestSHA1ForBranch(patch1.project,
                                  patch1.tracking_branch).AndReturn('sha1')

    portage_utilities.EBuild.UpdateEBuild(ebuild1.ebuild_path,
                                          'CROS_WORKON_COMMIT', 'sha1')
    portage_utilities.EBuild.GitRepoHasChanges('public_overlay').AndReturn(True)
    portage_utilities.EBuild.CommitChange(mox.IgnoreArg(),
                                          overlay='public_overlay')
    self.mox.ReplayAll()
    portage_utilities.EBuild.UpdateCommitHashesForChanges([patch1, patch2],
                                                          build_root)
    self.mox.VerifyAll()

  def testGitRepoHasChanges(self):
    """Tests that GitRepoHasChanges works correctly."""
    tmp_dir = tempfile.mkdtemp('portage_utilities_unittest')
    cros_build_lib.RunCommand(
        ['git', 'clone', '--depth=1',
         'http://git.chromium.org/chromiumos/chromite.git', tmp_dir])
    # No changes yet as we just cloned the repo.
    self.assertFalse(portage_utilities.EBuild.GitRepoHasChanges(tmp_dir))
    # Update metadata but no real changes.
    cros_build_lib.RunCommand('touch LICENSE', cwd=tmp_dir, shell=True)
    self.assertFalse(portage_utilities.EBuild.GitRepoHasChanges(tmp_dir))
    # A real change.
    cros_build_lib.RunCommand('echo hi > LICENSE', cwd=tmp_dir, shell=True)
    self.assertTrue(portage_utilities.EBuild.GitRepoHasChanges(tmp_dir))
    shutil.rmtree(tmp_dir)


class FindOverlaysTest(mox.MoxTestBase):

  def setUp(self):
    mox.MoxTestBase.setUp(self)
    self.build_root = '/fake_root'
    self.overlay = os.path.join(self.build_root,
                                'src/third_party/chromiumos-overlay')

  def testFindOverlays(self):
    self.mox.StubOutWithMock(cros_build_lib, 'RunCommand')
    stub_list_cmd = '/bin/true'
    public_output = 'public1\npublic2\n'
    private_output = 'private1\nprivate2\n'

    output_obj = cros_build_lib.CommandResult()
    output_obj.output = public_output
    cros_build_lib.RunCommand(
        [stub_list_cmd, '--all_boards', '--noprivate'],
        print_cmd=False, redirect_stdout=True).AndReturn(output_obj)

    output_obj = cros_build_lib.CommandResult()
    output_obj.output = private_output
    cros_build_lib.RunCommand(
        [stub_list_cmd, '--all_boards', '--nopublic'],
        print_cmd=False, redirect_stdout=True).AndReturn(output_obj)

    output_obj = cros_build_lib.CommandResult()
    output_obj.output = private_output + public_output
    cros_build_lib.RunCommand(
        [stub_list_cmd, '--all_boards'],
        print_cmd=False, redirect_stdout=True).AndReturn(output_obj)

    self.mox.ReplayAll()
    portage_utilities._OVERLAY_LIST_CMD = stub_list_cmd
    public_overlays = ['public1', 'public2', self.overlay]
    private_overlays = ['private1', 'private2']

    self.assertEqual(portage_utilities.FindOverlays(self.build_root, 'public'),
                     public_overlays)
    self.assertEqual(portage_utilities.FindOverlays(self.build_root, 'private'),
                     private_overlays)
    self.assertEqual(portage_utilities.FindOverlays(self.build_root, 'both'),
                     private_overlays + public_overlays)
    self.mox.VerifyAll()


class BuildEBuildDictionaryTest(mox.MoxTestBase):

  def setUp(self):
    mox.MoxTestBase.setUp(self)
    self.mox.StubOutWithMock(portage_utilities.os, 'walk')
    self.mox.StubOutWithMock(cros_build_lib, 'RunCommand')
    self.package = 'chromeos-base/test_package'
    self.root = '/overlay/chromeos-base/test_package'
    self.package_path = self.root + '/test_package-0.0.1.ebuild'
    paths = [[self.root, [], []]]
    portage_utilities.os.walk("/overlay").AndReturn(paths)
    self.mox.StubOutWithMock(portage_utilities, '_FindUprevCandidates')


  def testWantedPackage(self):
    overlays = {"/overlay": []}
    package = _Package(self.package)
    portage_utilities._FindUprevCandidates(
        [], mox.IgnoreArg()).AndReturn(package)
    self.mox.ReplayAll()
    portage_utilities.BuildEBuildDictionary(
        overlays, False, [self.package])
    self.mox.VerifyAll()
    self.assertEquals(len(overlays), 1)
    self.assertEquals(overlays["/overlay"], [package])

  def testUnwantedPackage(self):
    overlays = {"/overlay": []}
    package = _Package(self.package)
    portage_utilities._FindUprevCandidates(
        [], mox.IgnoreArg()).AndReturn(package)
    self.mox.ReplayAll()
    portage_utilities.BuildEBuildDictionary(overlays, False, [])
    self.assertEquals(len(overlays), 1)
    self.assertEquals(overlays["/overlay"], [])
    self.mox.VerifyAll()


class BlacklistManagerTest(mox.MoxTestBase):
  """Class that tests the blacklist manager."""
  FAKE_BLACKLIST = """
    # A Fake blacklist file.

    chromeos-base/fake-package
  """

  def setUp(self):
    mox.MoxTestBase.setUp(self)

  def testInitializeFromFile(self):
    """Tests whether we can correctly initialize from a fake blacklist file."""
    file_path = tempfile.mktemp()
    with open(file_path, 'w+') as fh:
      fh.write(self.FAKE_BLACKLIST)
    saved_black_list = portage_utilities._BlackListManager.BLACK_LIST_FILE
    try:
      portage_utilities._BlackListManager.BLACK_LIST_FILE = file_path
      black_list_manager = portage_utilities._BlackListManager()
      self.assertTrue(black_list_manager.IsPackageBlackListed(
          '/some/crazy/path/'
          'chromeos-base/fake-package/fake-package-0.0.5.ebuild'))
      self.assertEqual(len(black_list_manager.black_list_re_array), 1)
    finally:
      os.remove(file_path)
      portage_utilities._BlackListManager.BLACK_LIST_FILE = saved_black_list

  def testIsPackageBlackListed(self):
    """Tests if we can correctly check if a package is blacklisted."""
    self.mox.StubOutWithMock(portage_utilities._BlackListManager,
                             '_Initialize')
    portage_utilities._BlackListManager._Initialize()

    self.mox.ReplayAll()
    black_list_manager = portage_utilities._BlackListManager()
    black_list_manager.black_list_re_array = [
        re.compile('.*/fake/pkg/pkg-.*\.ebuild') ]
    self.assertTrue(black_list_manager.IsPackageBlackListed(
        '/some/crazy/path/'
        'fake/pkg/pkg-version.ebuild'))
    self.assertFalse(black_list_manager.IsPackageBlackListed(
        '/some/crazy/path/'
        'fake/diff-pkg/diff-pkg-version.ebuild'))
    self.mox.VerifyAll()


if __name__ == '__main__':
  unittest.main()
