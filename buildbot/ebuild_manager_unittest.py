#!/usr/bin/python

# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for ebuild_manager.py."""

import fileinput
import mox
import os
import re
import sys
import tempfile
import unittest

import constants
if __name__ == '__main__':
  sys.path.append(constants.SOURCE_ROOT)

from chromite.lib import cros_build_lib
from chromite.buildbot import ebuild_manager


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
    fake_ebuild = ebuild_manager.EBuild(fake_ebuild_path)
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
    self.mox.StubOutWithMock(ebuild_manager.cros_build_lib, 'Die')
    self.mox.StubOutWithMock(ebuild_manager.os.path, 'isdir')
    self.mox.StubOutWithMock(
        ebuild_manager.cros_build_lib, 'RunCommand')

    # The first RunCommand does 'grep' magic on the ebuild.
    result1 = _DummyCommandResult(' '.join([fake_project, fake_subdir]))
    ebuild_manager.cros_build_lib.RunCommand(
        mox.IgnoreArg(),
        print_cmd=fake_ebuild.verbose,
        redirect_stdout=True,
        shell=True).AndReturn(result1)

    # ... the result is used to construct a path, which the EBuild
    # code expects to be a directory.
    ebuild_manager.os.path.isdir(
      os.path.join(fake_sources, 'third_party', fake_subdir)).AndReturn(True)

    # ... the next RunCommand does 'git config --get ...'
    result2 = _DummyCommandResult(fake_project)
    ebuild_manager.cros_build_lib.RunCommand(
        mox.IgnoreArg(),
        print_cmd=fake_ebuild.verbose,
        redirect_stdout=True,
        shell=True).AndReturn(result2)

    # ... and the final RunCommand does 'git rev-parse HEAD'
    result3 = _DummyCommandResult(fake_hash)
    ebuild_manager.cros_build_lib.RunCommand(
        mox.IgnoreArg(),
        print_cmd=fake_ebuild.verbose,
        redirect_stdout=True,
        shell=True).AndReturn(result3)

    self.mox.ReplayAll()
    test_hash = fake_ebuild.GetCommitId(fake_sources)
    self.mox.VerifyAll()
    self.assertEquals(test_hash, fake_hash)


class BuildEBuildDictionaryTest(mox.MoxTestBase):

  def setUp(self):
    mox.MoxTestBase.setUp(self)
    self.mox.StubOutWithMock(ebuild_manager.os, 'walk')
    self.mox.StubOutWithMock(cros_build_lib, 'RunCommand')
    self.package = 'chromeos-base/test_package'
    self.root = '/overlay/chromeos-base/test_package'
    self.package_path = self.root + '/test_package-0.0.1.ebuild'
    paths = [[self.root, [], []]]
    ebuild_manager.os.walk("/overlay").AndReturn(paths)
    self.mox.StubOutWithMock(ebuild_manager, '_FindUprevCandidates')


  def testWantedPackage(self):
    overlays = {"/overlay": []}
    package = _Package(self.package)
    ebuild_manager._FindUprevCandidates([], mox.IgnoreArg()).AndReturn(package)
    self.mox.ReplayAll()
    ebuild_manager.BuildEBuildDictionary(overlays, False, [self.package])
    self.mox.VerifyAll()
    self.assertEquals(len(overlays), 1)
    self.assertEquals(overlays["/overlay"], [package])

  def testUnwantedPackage(self):
    overlays = {"/overlay": []}
    package = _Package(self.package)
    ebuild_manager._FindUprevCandidates([], mox.IgnoreArg()).AndReturn(package)
    self.mox.ReplayAll()
    ebuild_manager.BuildEBuildDictionary(overlays, False, [])
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
    saved_black_list_file = ebuild_manager._BlackListManager.BLACK_LIST_FILE
    try:
      ebuild_manager._BlackListManager.BLACK_LIST_FILE = file_path
      black_list_manager = ebuild_manager._BlackListManager()
      self.assertTrue(black_list_manager.IsPackageBlackListed(
          '/some/crazy/path/'
          'chromeos-base/fake-package/fake-package-0.0.5.ebuild'))
      self.assertEqual(len(black_list_manager.black_list_re_array), 1)
    finally:
      os.remove(file_path)
      ebuild_manager._BlackListManager.BLACK_LIST_FILE = saved_black_list_file

  def testIsPackageBlackListed(self):
    """Tests if we can correctly check if a package is blacklisted."""
    self.mox.StubOutWithMock(ebuild_manager._BlackListManager,
                             '_Initialize')
    ebuild_manager._BlackListManager._Initialize()

    self.mox.ReplayAll()
    black_list_manager = ebuild_manager._BlackListManager()
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
