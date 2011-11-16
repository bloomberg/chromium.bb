#!/usr/bin/python

# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for cros_mark_as_stable.py."""

import filecmp
import fileinput
import mox
import os
import re
import sys
import tempfile
import unittest

import constants
sys.path.append(constants.SOURCE_ROOT)
from chromite.lib import cros_build_lib

import cros_mark_as_stable


# pylint: disable=W0212,R0904
class NonClassTests(mox.MoxTestBase):
  def setUp(self):
    mox.MoxTestBase.setUp(self)
    self.mox.StubOutWithMock(cros_mark_as_stable, '_SimpleRunCommand')
    self._branch = 'test_branch'
    self._tracking_branch = 'cros/master'

  def testPushChange(self):
    git_log = 'Marking test_one as stable\nMarking test_two as stable\n'
    fake_description = 'Marking set of ebuilds as stable\n\n%s' % git_log
    self.mox.StubOutWithMock(cros_mark_as_stable, '_DoWeHaveLocalCommits')
    self.mox.StubOutWithMock(cros_mark_as_stable.GitBranch, 'CreateBranch')
    self.mox.StubOutWithMock(cros_mark_as_stable.GitBranch, 'Exists')
    self.mox.StubOutWithMock(cros_build_lib, 'GitPushWithRetry')

    cros_mark_as_stable._DoWeHaveLocalCommits(
        self._branch, self._tracking_branch).AndReturn(True)
    cros_mark_as_stable.GitBranch.Exists().AndReturn(False)
    cros_mark_as_stable.GitBranch.CreateBranch()
    cros_mark_as_stable.GitBranch.Exists().AndReturn(True)
    cros_mark_as_stable._SimpleRunCommand('git log --format=format:%s%n%n%b ' +
                          self._tracking_branch + '..').AndReturn(git_log)
    cros_mark_as_stable._SimpleRunCommand('repo sync .')
    cros_mark_as_stable._SimpleRunCommand('git merge --squash %s' %
                                          self._branch)
    cros_mark_as_stable._SimpleRunCommand('git commit -m "%s"' %
                                          fake_description)
    cros_mark_as_stable._SimpleRunCommand('git config push.default tracking')
    cros_build_lib.GitPushWithRetry('merge_branch', cwd='.', dryrun=False)
    self.mox.ReplayAll()
    cros_mark_as_stable.PushChange(self._branch, self._tracking_branch, False)
    self.mox.VerifyAll()


class GitBranchTest(mox.MoxTestBase):

  def setUp(self):
    mox.MoxTestBase.setUp(self)
    # Always stub RunCommmand out as we use it in every method.
    self.mox.StubOutWithMock(cros_mark_as_stable, '_SimpleRunCommand')
    self._branch = self.mox.CreateMock(cros_mark_as_stable.GitBranch)
    self._branch_name = 'test_branch'
    self._branch.branch_name = self._branch_name
    self._tracking_branch = 'cros/test'
    self._branch.tracking_branch = self._tracking_branch

  def testCheckoutCreate(self):
    # Test init with no previous branch existing.
    self._branch.Exists().AndReturn(False)
    cros_mark_as_stable._SimpleRunCommand(
        'repo start %s .' % self._branch_name)
    self.mox.ReplayAll()
    cros_mark_as_stable.GitBranch.Checkout(self._branch)
    self.mox.VerifyAll()

  def testCheckoutNoCreate(self):
    # Test init with previous branch existing.
    self._branch.Exists().AndReturn(True)
    cros_mark_as_stable._SimpleRunCommand('git checkout %s -f' % (
        self._branch_name))
    self.mox.ReplayAll()
    cros_mark_as_stable.GitBranch.Checkout(self._branch)
    self.mox.VerifyAll()

  def testDelete(self):
    self.mox.StubOutWithMock(cros_mark_as_stable.GitBranch, 'Checkout')
    branch = cros_mark_as_stable.GitBranch(self._branch_name,
                                           self._tracking_branch)
    cros_mark_as_stable.GitBranch.Checkout(mox.IgnoreArg())
    cros_mark_as_stable._SimpleRunCommand('repo abandon ' + self._branch_name)
    self.mox.ReplayAll()
    branch.Delete()
    self.mox.VerifyAll()

  def testExists(self):
    branch = cros_mark_as_stable.GitBranch(self._branch_name,
                                           self._tracking_branch)
    # Test if branch exists that is created
    cros_mark_as_stable._SimpleRunCommand('git branch').AndReturn(
        '%s' % self._branch_name)
    self.mox.ReplayAll()
    self.assertTrue(branch.Exists())
    self.mox.VerifyAll()


class EBuildTest(mox.MoxTestBase):

  def setUp(self):
    mox.MoxTestBase.setUp(self)

  def _makeFakeEbuild(self, fake_ebuild_path):
    self.mox.StubOutWithMock(fileinput, 'input')
    fileinput.input(fake_ebuild_path).AndReturn('')
    self.mox.ReplayAll()
    fake_ebuild = cros_mark_as_stable.EBuild(fake_ebuild_path)
    self.mox.VerifyAll()
    return fake_ebuild

  def testParseEBuildPath(self):
    # Test with ebuild with revision number.
    fake_ebuild_path = '/path/to/test_package/test_package-0.0.1-r1.ebuild'
    fake_ebuild = self._makeFakeEbuild(fake_ebuild_path)

    self.assertEquals(fake_ebuild.version_no_rev, '0.0.1')
    self.assertEquals(fake_ebuild.ebuild_path_no_revision,
                      '/path/to/test_package/test_package-0.0.1')
    self.assertEquals(fake_ebuild.ebuild_path_no_version,
                      '/path/to/test_package/test_package')
    self.assertEquals(fake_ebuild.current_revision, 1)

  def testParseEBuildPathNoRevisionNumber(self):
    # Test with ebuild without revision number.
    fake_ebuild_path = '/path/to/test_package/test_package-9999.ebuild'
    fake_ebuild = self._makeFakeEbuild(fake_ebuild_path)

    self.assertEquals(fake_ebuild.version_no_rev, '9999')
    self.assertEquals(fake_ebuild.ebuild_path_no_revision,
                      '/path/to/test_package/test_package-9999')
    self.assertEquals(fake_ebuild.ebuild_path_no_version,
                      '/path/to/test_package/test_package')
    self.assertEquals(fake_ebuild.current_revision, 0)

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
    self.mox.StubOutWithMock(cros_mark_as_stable.cros_build_lib, 'Die')
    self.mox.StubOutWithMock(cros_mark_as_stable.os.path, 'isdir')
    self.mox.StubOutWithMock(cros_mark_as_stable, '_SimpleRunCommand')

    # The first command does 'grep' magic on the ebuild.
    cros_mark_as_stable._SimpleRunCommand(
        mox.IgnoreArg()).AndReturn(' '.join([fake_project, fake_subdir]) + '\n')

    # ... the result is used to construct a path, which the EBuild
    # code expects to be a directory.
    cros_mark_as_stable.os.path.isdir(
      os.path.join(fake_sources, 'third_party', fake_subdir)).AndReturn(True)

    # ... the next command does 'git config --get ...'
    cros_mark_as_stable._SimpleRunCommand(
        mox.IgnoreArg()).AndReturn(fake_project + '\n')

    # ... and the final command does 'git rev-parse HEAD'
    cros_mark_as_stable._SimpleRunCommand(
        mox.IgnoreArg()).AndReturn(fake_hash + '\n')

    self.mox.ReplayAll()
    test_hash = fake_ebuild.GetCommitId(fake_sources)
    self.mox.VerifyAll()
    self.assertEquals(test_hash, fake_hash)


class EBuildStableMarkerTest(mox.MoxTestBase):

  def setUp(self):
    mox.MoxTestBase.setUp(self)
    self.mox.StubOutWithMock(cros_mark_as_stable, '_SimpleRunCommand')
    self.mox.StubOutWithMock(cros_build_lib, 'RunCommand')
    self.mox.StubOutWithMock(os, 'unlink')
    self.m_ebuild = self.mox.CreateMock(cros_mark_as_stable.EBuild)
    self.m_ebuild.is_stable = True
    self.m_ebuild.package = 'test_package/test_package'
    self.m_ebuild.version_no_rev = '0.0.1'
    self.m_ebuild.current_revision = 1
    self.m_ebuild.ebuild_path_no_revision = '/path/test_package-0.0.1'
    self.m_ebuild.ebuild_path_no_version = '/path/test_package'
    self.m_ebuild.ebuild_path = '/path/test_package-0.0.1-r1.ebuild'
    self.revved_ebuild_path = '/path/test_package-0.0.1-r2.ebuild'
    self.unstable_ebuild_path = '/path/test_package-9999.ebuild'

  def testRevWorkOnEBuild(self):
    self.mox.StubOutWithMock(cros_mark_as_stable.fileinput, 'input')
    self.mox.StubOutWithMock(cros_mark_as_stable.os.path, 'exists')
    self.mox.StubOutWithMock(cros_mark_as_stable.shutil, 'copyfile')
    self.mox.StubOutWithMock(filecmp, 'cmp')
    m_file = self.mox.CreateMock(file)

    # Prepare mock fileinput.  This tests to make sure both the commit id
    # and keywords are changed correctly.
    mock_file = ['EAPI=2', 'CROS_WORKON_COMMIT=old_id',
                 'KEYWORDS=\"~x86 ~arm\"', 'src_unpack(){}']

    ebuild_9999 = self.m_ebuild.ebuild_path_no_version + '-9999.ebuild'
    cros_mark_as_stable.os.path.exists(ebuild_9999).AndReturn(True)
    cros_mark_as_stable.shutil.copyfile(ebuild_9999, self.revved_ebuild_path)
    cros_mark_as_stable.fileinput.input(self.revved_ebuild_path,
                                        inplace=1).AndReturn(mock_file)
    m_file.write('EAPI=2')
    m_file.write('CROS_WORKON_COMMIT="my_id"\n')
    m_file.write('KEYWORDS="x86 arm"')
    m_file.write('src_unpack(){}')
    filecmp.cmp(self.m_ebuild.ebuild_path, self.revved_ebuild_path,
                shallow=False).AndReturn(False)
    cros_mark_as_stable._SimpleRunCommand('git add ' + self.revved_ebuild_path)
    cros_mark_as_stable._SimpleRunCommand('git rm ' + self.m_ebuild.ebuild_path)

    self.mox.ReplayAll()
    marker = cros_mark_as_stable.EBuildStableMarker(self.m_ebuild)
    result = marker.RevWorkOnEBuild('my_id', redirect_file=m_file)
    self.mox.VerifyAll()
    self.assertEqual(result, 'test_package/test_package-0.0.1-r2')

  def testRevUnchangedEBuild(self):
    self.mox.StubOutWithMock(cros_mark_as_stable.fileinput, 'input')
    self.mox.StubOutWithMock(cros_mark_as_stable.os.path, 'exists')
    self.mox.StubOutWithMock(cros_mark_as_stable.shutil, 'copyfile')
    self.mox.StubOutWithMock(filecmp, 'cmp')
    m_file = self.mox.CreateMock(file)

    # Prepare mock fileinput.  This tests to make sure both the commit id
    # and keywords are changed correctly.
    mock_file = ['EAPI=2', 'CROS_WORKON_COMMIT=old_id',
                 'KEYWORDS=\"~x86 ~arm\"', 'src_unpack(){}']

    ebuild_9999 = self.m_ebuild.ebuild_path_no_version + '-9999.ebuild'
    cros_mark_as_stable.os.path.exists(ebuild_9999).AndReturn(True)
    cros_mark_as_stable.shutil.copyfile(ebuild_9999, self.revved_ebuild_path)
    cros_mark_as_stable.fileinput.input(self.revved_ebuild_path,
                                        inplace=1).AndReturn(mock_file)
    m_file.write('EAPI=2')
    m_file.write('CROS_WORKON_COMMIT="my_id"\n')
    m_file.write('KEYWORDS="x86 arm"')
    m_file.write('src_unpack(){}')
    filecmp.cmp(self.m_ebuild.ebuild_path, self.revved_ebuild_path,
                shallow=False).AndReturn(True)
    cros_mark_as_stable.os.unlink(self.revved_ebuild_path)

    self.mox.ReplayAll()
    marker = cros_mark_as_stable.EBuildStableMarker(self.m_ebuild)
    result = marker.RevWorkOnEBuild('my_id', redirect_file=m_file)
    self.mox.VerifyAll()
    self.assertEqual(result, None)

  def testRevMissingEBuild(self):
    self.mox.StubOutWithMock(cros_mark_as_stable.fileinput, 'input')
    self.mox.StubOutWithMock(cros_mark_as_stable.os.path, 'exists')
    self.mox.StubOutWithMock(cros_mark_as_stable.shutil, 'copyfile')
    self.mox.StubOutWithMock(cros_build_lib, 'Die')
    self.mox.StubOutWithMock(filecmp, 'cmp')
    m_file = self.mox.CreateMock(file)

    revved_ebuild_path = self.m_ebuild.ebuild_path
    self.m_ebuild.ebuild_path = self.unstable_ebuild_path
    self.m_ebuild.is_stable = False
    self.m_ebuild.current_revision = 0

    # Prepare mock fileinput.  This tests to make sure both the commit id
    # and keywords are changed correctly.
    mock_file = ['EAPI=2', 'CROS_WORKON_COMMIT=old_id',
                 'KEYWORDS=\"~x86 ~arm\"', 'src_unpack(){}']

    ebuild_9999 = self.m_ebuild.ebuild_path_no_version + '-9999.ebuild'
    cros_mark_as_stable.os.path.exists(ebuild_9999).AndReturn(False)
    cros_build_lib.Die("Missing unstable ebuild: %s" % ebuild_9999)
    cros_mark_as_stable.shutil.copyfile(ebuild_9999, revved_ebuild_path)
    cros_mark_as_stable.fileinput.input(revved_ebuild_path,
                                        inplace=1).AndReturn(mock_file)
    m_file.write('EAPI=2')
    m_file.write('CROS_WORKON_COMMIT="my_id"\n')
    m_file.write('KEYWORDS="x86 arm"')
    m_file.write('src_unpack(){}')
    filecmp.cmp(self.unstable_ebuild_path, revved_ebuild_path,
                shallow=False).AndReturn(False)
    cros_mark_as_stable._SimpleRunCommand('git add ' + revved_ebuild_path)

    self.mox.ReplayAll()
    marker = cros_mark_as_stable.EBuildStableMarker(self.m_ebuild)
    result = marker.RevWorkOnEBuild('my_id', redirect_file=m_file)
    self.mox.VerifyAll()
    self.assertEqual(result, 'test_package/test_package-0.0.1-r1')


  def testCommitChange(self):
    mock_message = 'Commit me'
    cros_mark_as_stable._SimpleRunCommand(
        'git commit -am "%s"' % mock_message)
    self.mox.ReplayAll()
    marker = cros_mark_as_stable.EBuildStableMarker(self.m_ebuild)
    marker.CommitChange(mock_message)
    self.mox.VerifyAll()


class _Package(object):
  def __init__(self, package):
    self.package = package


class BuildEBuildDictionaryTest(mox.MoxTestBase):

  def setUp(self):
    mox.MoxTestBase.setUp(self)
    self.mox.StubOutWithMock(cros_mark_as_stable.os, 'walk')
    self.mox.StubOutWithMock(cros_build_lib, 'RunCommand')
    self.package = 'chromeos-base/test_package'
    self.root = '/overlay/chromeos-base/test_package'
    self.package_path = self.root + '/test_package-0.0.1.ebuild'
    paths = [[self.root, [], []]]
    cros_mark_as_stable.os.walk("/overlay").AndReturn(paths)
    self.mox.StubOutWithMock(cros_mark_as_stable, '_FindUprevCandidates')


  def testWantedPackage(self):
    overlays = {"/overlay": []}
    package = _Package(self.package)
    cros_mark_as_stable._FindUprevCandidates([], None).AndReturn(package)
    self.mox.ReplayAll()
    cros_mark_as_stable._BuildEBuildDictionary(overlays, False, [self.package],
                                               None)
    self.mox.VerifyAll()
    self.assertEquals(len(overlays), 1)
    self.assertEquals(overlays["/overlay"], [package])

  def testUnwantedPackage(self):
    overlays = {"/overlay": []}
    package = _Package(self.package)
    cros_mark_as_stable._FindUprevCandidates([], None).AndReturn(package)
    self.mox.ReplayAll()
    cros_mark_as_stable._BuildEBuildDictionary(overlays, False, [], None)
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
    try:
      cros_mark_as_stable._BlackListManager.BLACK_LIST_FILE = file_path
      black_list_manager = cros_mark_as_stable._BlackListManager()
      self.assertTrue(black_list_manager.IsPackageBlackListed(
          '/some/crazy/path/'
          'chromeos-base/fake-package/fake-package-0.0.5.ebuild'))
      self.assertEqual(len(black_list_manager.black_list_re_array), 1)
    finally:
      os.remove(file_path)

  def testIsPackageBlackListed(self):
    """Tests if we can correctly check if a package is blacklisted."""
    self.mox.StubOutWithMock(cros_mark_as_stable._BlackListManager,
                             '_Initialize')
    cros_mark_as_stable._BlackListManager._Initialize()

    self.mox.ReplayAll()
    black_list_manager = cros_mark_as_stable._BlackListManager()
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
