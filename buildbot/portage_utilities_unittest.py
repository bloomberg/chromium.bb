#!/usr/bin/python

# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for portage_utilities.py."""

import fileinput
import mox
import os
import re
import sys
import tempfile

import constants
if __name__ == '__main__':
  sys.path.insert(0, constants.SOURCE_ROOT)

from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.buildbot import portage_utilities

# pylint: disable=W0212,E1120

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


class EBuildTest(cros_test_lib.MoxTestCase):

  def _makeFakeEbuild(self, fake_ebuild_path, fake_ebuild_content=''):
    self.mox.StubOutWithMock(fileinput, 'input')
    fileinput.input(fake_ebuild_path).AndReturn(fake_ebuild_content)
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
    self.assertEquals(fake_ebuild.version_no_rev, '0.0.1')
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
    self.assertEquals(fake_ebuild.version_no_rev, '9999')
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
    fake_hash = '24ab3c9f6d6b5c744382dba2ca8fb444b9808e9f'
    fake_ebuild_path = '/path/to/test_package/test_package-9999.ebuild'
    fake_ebuild = self._makeFakeEbuild(fake_ebuild_path)

    # git rev-parse HEAD
    self.mox.StubOutWithMock(cros_build_lib, 'RunCommandCaptureOutput')
    result = _DummyCommandResult(fake_hash)
    cros_build_lib.RunCommandCaptureOutput(
        mox.IgnoreArg(),
        cwd=mox.IgnoreArg(),
        print_cmd=portage_utilities.EBuild.VERBOSE).AndReturn(result)

    self.mox.ReplayAll()
    test_hash = fake_ebuild.GetCommitId(fake_sources)
    self.mox.VerifyAll()
    self.assertEquals(test_hash, fake_hash)

  def testEBuildStable(self):
    """Test ebuild w/keyword variations"""
    fake_ebuild_path = '/path/to/test_package/test_package-9999.ebuild'

    datasets = (
        ('~amd64', False),
        ('amd64', True),
        ('~amd64 ~arm ~x86', False),
        ('~amd64 arm ~x86', True),
        ('-* ~arm', False),
        ('-* x86', True),
    )
    for keywords, stable in datasets:
      fake_ebuild = self._makeFakeEbuild(
          fake_ebuild_path, fake_ebuild_content=['KEYWORDS="%s"\n' % keywords])
      self.assertEquals(fake_ebuild.is_stable, stable)
      self.mox.UnsetStubs()

  def testEBuildBlacklisted(self):
    """Test blacklisted ebuild"""
    fake_ebuild_path = '/path/to/test_package/test_package-9999.ebuild'

    fake_ebuild = self._makeFakeEbuild(fake_ebuild_path)
    self.assertEquals(fake_ebuild.is_blacklisted, False)
    self.mox.UnsetStubs()

    fake_ebuild = self._makeFakeEbuild(
        fake_ebuild_path, fake_ebuild_content=['CROS_WORKON_BLACKLIST="1"\n'])
    self.assertEquals(fake_ebuild.is_blacklisted, True)


class ProjectAndPathTest(cros_test_lib.MoxTempDirTestCase):

  def _MockParseWorkonVariables(self, fake_projects, _fake_localname,
                                _fake_subdir, fake_ebuild_contents):
    """Mock the necessary calls, start Replay mode, call GetSourcePath()."""
    self.mox.StubOutWithMock(os.path, 'isdir')
    self.mox.StubOutWithMock(portage_utilities.EBuild, 'GetGitProjectName')

    # We need 'chromeos-base' here because it controls default _SUBDIR values.
    ebuild_path = os.path.join(self.tempdir, 'chromeos-base', 'package',
                               'package-9999.ebuild')
    osutils.WriteFile(ebuild_path, fake_ebuild_contents, makedirs=True)
    for p in fake_projects:
      os.path.isdir(mox.IgnoreArg()).AndReturn(True)
      portage_utilities.EBuild.GetGitProjectName(
          mox.IgnoreArg()).AndReturn(p)
    self.mox.ReplayAll()

    ebuild = portage_utilities.EBuild(ebuild_path)
    result = ebuild.GetSourcePath(self.tempdir)
    self.mox.VerifyAll()
    return result

  def testParseLegacyWorkonVariables(self):
    """Tests if ebuilds in a single item format are correctly parsed."""
    fake_project = 'my_project1'
    fake_localname = 'foo'
    fake_subdir = 'bar'
    fake_ebuild_contents = """
CROS_WORKON_PROJECT=%s
CROS_WORKON_LOCALNAME=%s
CROS_WORKON_SUBDIR=%s
    """ % (fake_project, fake_localname, fake_subdir)
    project, subdir = self._MockParseWorkonVariables(
        [fake_project], [fake_localname], [fake_subdir], fake_ebuild_contents)
    self.assertEquals(project, [fake_project])
    self.assertEquals(subdir, [os.path.join(
        self.tempdir, 'platform', '%s/%s' % (fake_localname, fake_subdir))])

  def testParseArrayWorkonVariables(self):
    """Tests if ebuilds in an array format are correctly parsed."""
    fake_projects = ['my_project1', 'my_project2', 'my_project3']
    fake_localname = ['foo', 'bar', 'bas']
    fake_subdir = ['sub1', 'sub2', 'sub3']
    # The test content is formatted using the same function that
    # formats ebuild output, ensuring that we can parse our own
    # products.
    fake_ebuild_contents = """
CROS_WORKON_PROJECT=%s
CROS_WORKON_LOCALNAME=%s
CROS_WORKON_SUBDIR=%s
    """ % (portage_utilities.EBuild.FormatBashArray(fake_projects),
           portage_utilities.EBuild.FormatBashArray(fake_localname),
           portage_utilities.EBuild.FormatBashArray(fake_subdir))
    project, subdir = self._MockParseWorkonVariables(
        fake_projects, fake_localname, fake_subdir, fake_ebuild_contents)
    self.assertEquals(project, fake_projects)
    fake_path = ['%s/%s' % (fake_localname[i], fake_subdir[i])
                 for i in range(0, len(fake_projects))]
    fake_path = map(lambda x: os.path.realpath(
        os.path.join(self.tempdir, 'platform', x)), fake_path)
    self.assertEquals(subdir, fake_path)


class StubEBuild(portage_utilities.EBuild):
  def __init__(self, path):
    super(StubEBuild, self).__init__(path)
    self.is_workon = True
    self.is_stable = True

  def _ReadEBuild(self, path):
    pass

  def GetCommitId(self, srcpath):
    id_map = {
      'p1_path' : 'my_id',
      'p1_path1' : 'my_id1',
      'p1_path2' : 'my_id2'
    }
    if srcpath in id_map:
      return id_map[srcpath]
    else:
      return 'you_lose'


class EBuildRevWorkonTest(cros_test_lib.MoxTempDirTestCase):
  # Lines that we will feed as fake ebuild contents to
  # EBuild.MarAsStable().  This is the minimum content needed
  # to test the various branches in the function's main processing
  # loop.
  _mock_ebuild = ['EAPI=2\n',
                  'CROS_WORKON_COMMIT=old_id\n',
                  'KEYWORDS=\"~x86 ~arm ~amd64\"\n',
                  'src_unpack(){}\n']
  _mock_ebuild_multi = ['EAPI=2\n',
                        'CROS_WORKON_COMMIT=("old_id1","old_id2")\n',
                        'KEYWORDS=\"~x86 ~arm ~amd64\"\n',
                        'src_unpack(){}\n']

  def setUp(self):
    self.overlay = '/sources/overlay'
    package_name = os.path.join(self.overlay,
                                'category/test_package/test_package-0.0.1')
    ebuild_path = package_name + '-r1.ebuild'
    self.m_ebuild = StubEBuild(ebuild_path)
    self.revved_ebuild_path = package_name + '-r2.ebuild'

  def createRevWorkOnMocks(self, ebuild_content, rev, multi=False):
    self.mox.StubOutWithMock(os.path, 'exists')
    self.mox.StubOutWithMock(cros_build_lib, 'Die')
    self.mox.StubOutWithMock(portage_utilities.shutil, 'copyfile')
    self.mox.StubOutWithMock(os, 'unlink')
    self.mox.StubOutWithMock(portage_utilities.EBuild, '_RunCommand')
    self.mox.StubOutWithMock(cros_build_lib, 'RunCommand')
    self.mox.StubOutWithMock(portage_utilities.filecmp, 'cmp')
    self.mox.StubOutWithMock(portage_utilities.fileinput, 'input')
    self.mox.StubOutWithMock(portage_utilities.EBuild, 'GetVersion')
    self.mox.StubOutWithMock(portage_utilities.EBuild, 'GetSourcePath')
    self.mox.StubOutWithMock(portage_utilities.EBuild, 'GetTreeId')

    if multi:
      portage_utilities.EBuild.GetSourcePath('/sources').AndReturn(
          (['fake_project1','fake_project2'], ['p1_path1','p1_path2']))
    else:
      portage_utilities.EBuild.GetSourcePath('/sources').AndReturn(
          (['fake_project1'], ['p1_path']))
    portage_utilities.EBuild.GetVersion('/sources', '0.0.1').AndReturn('0.0.1')
    if multi:
      portage_utilities.EBuild.GetTreeId('p1_path1').AndReturn('treehash1')
      portage_utilities.EBuild.GetTreeId('p1_path2').AndReturn('treehash2')
    else:
      portage_utilities.EBuild.GetTreeId('p1_path').AndReturn('treehash')

    ebuild_9999 = self.m_ebuild._unstable_ebuild_path
    os.path.exists(ebuild_9999).AndReturn(True)

    # These calls come from MarkAsStable()
    portage_utilities.shutil.copyfile(ebuild_9999, self.revved_ebuild_path)

    m_file = self.mox.CreateMock(file)
    portage_utilities.fileinput.input(self.revved_ebuild_path,
                                      inplace=1).AndReturn(ebuild_content)
    m_file.write('EAPI=2\n')

    if multi:
      m_file.write('CROS_WORKON_COMMIT=("my_id1" "my_id2")\n')
      m_file.write('CROS_WORKON_TREE=("treehash1" "treehash2")\n')
    else:
      m_file.write('CROS_WORKON_COMMIT="my_id"\n')
      m_file.write('CROS_WORKON_TREE="treehash"\n')

    m_file.write('KEYWORDS=\"x86 arm amd64\"\n')
    m_file.write('src_unpack(){}\n')
    # MarkAsStable() returns here

    portage_utilities.filecmp.cmp(self.m_ebuild.ebuild_path,
                                  self.revved_ebuild_path,
                                  shallow=False).AndReturn(not rev)
    if rev:
      portage_utilities.EBuild._RunCommand(
          ['git', 'add', self.revved_ebuild_path],
          cwd=self.overlay)
      if self.m_ebuild.is_stable:
        portage_utilities.EBuild._RunCommand(
            ['git', 'rm', self.m_ebuild.ebuild_path],
            cwd=self.overlay)
      if multi:
        message = portage_utilities._GIT_COMMIT_MESSAGE % (
          self.m_ebuild.package, 'my_id1,my_id2')
      else:
        message = portage_utilities._GIT_COMMIT_MESSAGE % (
          self.m_ebuild.package, 'my_id')
      cros_build_lib.RunCommand(
          ['git', 'commit', '-a', '-m', message], cwd=self.overlay,
          print_cmd=False)
    else:
      os.unlink(self.revved_ebuild_path)

    return m_file


  def testRevWorkOnEBuild(self):
    """Test Uprev of a single project ebuild."""
    m_file = self.createRevWorkOnMocks(self._mock_ebuild, rev=True)
    self.mox.ReplayAll()
    result = self.m_ebuild.RevWorkOnEBuild('/sources', redirect_file=m_file)
    self.mox.VerifyAll()
    self.assertEqual(result, 'category/test_package-0.0.1-r2')

  def testRevWorkOnMultiEBuild(self):
    """Test Uprev of a multi-project (array) ebuild."""
    m_file = self.createRevWorkOnMocks(self._mock_ebuild_multi, rev=True,
                                       multi=True)
    self.mox.ReplayAll()
    result = self.m_ebuild.RevWorkOnEBuild('/sources', redirect_file=m_file)
    self.mox.VerifyAll()
    self.assertEqual(result, 'category/test_package-0.0.1-r2')

  def testRevUnchangedEBuild(self):
    m_file = self.createRevWorkOnMocks(self._mock_ebuild, rev=False)

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
      self._mock_ebuild[0:1] + self._mock_ebuild[2:], rev=True)

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
    self.m_ebuild.CommitChange(mock_message, '.')
    self.mox.VerifyAll()

  def testUpdateCommitHashesForChanges(self):
    """Tests that we can update the commit hashes for changes correctly."""
    cls = portage_utilities.EBuild
    ebuild1 = self.mox.CreateMock(cls)
    ebuild1.ebuild_path = 'public_overlay/ebuild.ebuild'
    ebuild1.package = 'test/project'

    self.mox.StubOutWithMock(portage_utilities, 'FindOverlays')
    self.mox.StubOutWithMock(cls, '_GetEBuildProjects')
    self.mox.StubOutWithMock(cls, '_GetSHA1ForProject')
    self.mox.StubOutWithMock(cls, 'UpdateEBuild')
    self.mox.StubOutWithMock(cls, 'CommitChange')
    self.mox.StubOutWithMock(cls, 'GitRepoHasChanges')

    build_root = 'fakebuildroot'
    overlays = ['public_overlay']
    changes = ['fake change']
    projects = ['fake_project1', 'fake_project2']
    project_ebuilds = {ebuild1: projects}
    portage_utilities.FindOverlays(
        constants.BOTH_OVERLAYS, buildroot=build_root).AndReturn(overlays)
    cls._GetEBuildProjects(
        build_root, overlays, changes).AndReturn(project_ebuilds)
    for i, p in enumerate(projects):
      cls._GetSHA1ForProject(mox.IgnoreArg(), p).InAnyOrder().AndReturn(str(i))
    cls.UpdateEBuild(ebuild1.ebuild_path, dict(CROS_WORKON_COMMIT='("0" "1")'))
    cls.GitRepoHasChanges('public_overlay').AndReturn(True)
    cls.CommitChange(mox.IgnoreArg(), overlay='public_overlay')
    self.mox.ReplayAll()
    cls.UpdateCommitHashesForChanges(changes, build_root)
    self.mox.VerifyAll()

  def testGitRepoHasChanges(self):
    """Tests that GitRepoHasChanges works correctly."""
    cros_build_lib.RunCommand(
        ['git', 'clone', '--depth=1',
         'file://' + os.path.join(constants.SOURCE_ROOT, 'chromite'),
         self.tempdir])
    # No changes yet as we just cloned the repo.
    self.assertFalse(portage_utilities.EBuild.GitRepoHasChanges(self.tempdir))
    # Update metadata but no real changes.
    osutils.Touch(os.path.join(self.tempdir, 'LICENSE'))
    self.assertFalse(portage_utilities.EBuild.GitRepoHasChanges(self.tempdir))
    # A real change.
    osutils.WriteFile(os.path.join(self.tempdir, 'LICENSE'), 'hi')
    self.assertTrue(portage_utilities.EBuild.GitRepoHasChanges(self.tempdir))


class FindOverlaysTest(cros_test_lib.MoxTestCase):
  FAKE, MARIO = 'fake-board', 'x86-mario'
  PRIVATE = constants.PRIVATE_OVERLAYS
  PUBLIC = constants.PUBLIC_OVERLAYS
  BOTH = constants.BOTH_OVERLAYS

  def setUp(self):
    """Fetch all overlays."""
    self.overlays = {}
    for b in (None, self.FAKE, self.MARIO):
      self.overlays[b] = d = {}
      for o in (self.PRIVATE, self.PUBLIC, self.BOTH, None):
        d[o] = portage_utilities.FindOverlays(o, b, constants.SOURCE_ROOT)

  def testMissingPrimaryOverlay(self):
    """Test what happens when a primary overlay is missing.

    If the overlay doesn't exist, FindOverlays should throw a
    MissingOverlayException.
    """
    self.assertRaises(portage_utilities.MissingOverlayException,
                      portage_utilities.FindPrimaryOverlay, self.BOTH,
                      self.FAKE, constants.SOURCE_ROOT)

  def testDuplicates(self):
    """Verify that no duplicate overlays are returned."""
    for d in self.overlays.itervalues():
      for overlays in d.itervalues():
        self.assertEqual(len(overlays), len(set(overlays)))

  def testOverlaysExist(self):
    """Verify that all overlays returned actually exist on disk."""
    for d in self.overlays.itervalues():
      for overlays in d.itervalues():
        self.assertTrue(all(os.path.isdir(x) for x in overlays))

  def testPrivatePublicOverlayTypes(self):
    """Verify public/private filtering.

    If we ask for results from 'both overlays', we should
    find all public and all private overlays.

    There should always be at least one public overlay. (Note:
    there may not be any private overlays, e.g. if the user has
    a public checkout.)
    """
    for d in self.overlays.itervalues():
      self.assertTrue(set(d[self.BOTH]) >= set(d[self.PUBLIC]))
      self.assertTrue(set(d[self.BOTH]) > set(d[self.PRIVATE]))
      self.assertTrue(set(d[self.PUBLIC]).isdisjoint(d[self.PRIVATE]))

  def testNoOverlayType(self):
    """If we specify overlay_type=None, no results should be returned."""
    self.assertTrue(all(d[None] == [] for d in self.overlays.itervalues()))

  def testNonExistentBoard(self):
    """Test what happens when a non-existent board is supplied.

    If we specify a non-existent board to FindOverlays, only generic
    overlays should be returned.
    """
    for o in (self.PUBLIC, self.BOTH):
      self.assertTrue(set(self.overlays[self.FAKE][o]) <
                      set(self.overlays[self.MARIO][o]))

  def testAllBoards(self):
    """If we specify board=None, all overlays should be returned."""
    for o in (self.PUBLIC, self.BOTH):
      for b in (self.FAKE, self.MARIO):
        self.assertTrue(set(self.overlays[b][o]) < set(self.overlays[None][o]))

  def testMarioPrimaryOverlay(self):
    """Verify that mario has a primary overlay.

    Further, the only difference between the public overlays for mario and a
    fake board is the primary overlay, which is listed last.
    """
    mario_primary = portage_utilities.FindPrimaryOverlay(self.BOTH, self.MARIO,
                                                         constants.SOURCE_ROOT)
    self.assertTrue(mario_primary in self.overlays[self.MARIO][self.BOTH])
    self.assertTrue(mario_primary not in self.overlays[self.FAKE][self.BOTH])
    self.assertEqual(mario_primary, self.overlays[self.MARIO][self.PUBLIC][-1])
    self.assertEqual(self.overlays[self.MARIO][self.PUBLIC][:-1],
                     self.overlays[self.FAKE][self.PUBLIC])


class BuildEBuildDictionaryTest(cros_test_lib.MoxTestCase):

  def setUp(self):
    self.mox.StubOutWithMock(os, 'walk')
    self.mox.StubOutWithMock(cros_build_lib, 'RunCommand')
    self.package = 'chromeos-base/test_package'
    self.root = '/overlay/chromeos-base/test_package'
    self.package_path = self.root + '/test_package-0.0.1.ebuild'
    paths = [[self.root, [], []]]
    os.walk("/overlay").AndReturn(paths)
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


class BlacklistManagerTest(cros_test_lib.MoxTestCase):
  """Class that tests the blacklist manager."""
  FAKE_BLACKLIST = """
    # A Fake blacklist file.

    chromeos-base/fake-package
  """

  @osutils.TempDirDecorator
  def testInitializeFromFile(self):
    """Tests whether we can correctly initialize from a fake blacklist file."""
    file_path = tempfile.mktemp()
    osutils.WriteFile(file_path, self.FAKE_BLACKLIST)
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


class ProjectMappingTest(cros_test_lib.TestCase):

  def testSplitEbuildPath(self):
    """Test if we can split an ebuild path into its components."""
    ebuild_path = 'chromeos-base/power_manager/power_manager-9999.ebuild'
    components = ['chromeos-base', 'power_manager', 'power_manager-9999']
    for path in (ebuild_path, './' + ebuild_path, 'foo.bar/' + ebuild_path):
      self.assertEquals(components, portage_utilities.SplitEbuildPath(path))

  def testSplitPV(self):
    """Test splitting PVs into package and version components."""
    pv = 'bar-1.2.3_rc1-r5'
    package, version_no_rev, rev = tuple(pv.split('-'))
    split_pv = portage_utilities.SplitPV(pv)
    self.assertEquals(split_pv.pv, pv)
    self.assertEquals(split_pv.package, package)
    self.assertEquals(split_pv.version_no_rev, version_no_rev)
    self.assertEquals(split_pv.rev, rev)
    self.assertEquals(split_pv.version, '%s-%s' % (version_no_rev, rev))

  def testSplitCPV(self):
    """Test splitting CPV into components."""
    cpv = 'foo/bar-4.5.6_alpha-r6'
    cat, pv = cpv.split('/', 1)
    split_pv = portage_utilities.SplitPV(pv)
    split_cpv = portage_utilities.SplitCPV(cpv)
    self.assertEquals(split_cpv.category, cat)
    for k, v in split_pv._asdict().iteritems():
      self.assertEquals(getattr(split_cpv, k), v)

  def testFindWorkonProjects(self):
    """Test if we can find the list of workon projects."""
    power_manager = 'chromeos-base/power_manager'
    power_manager_project = 'chromiumos/platform/power_manager'
    kernel = 'sys-kernel/chromeos-kernel'
    kernel_project = 'chromiumos/third_party/kernel'
    matches = [
      ([power_manager], set([power_manager_project])),
      ([kernel], set([kernel_project])),
      ([power_manager, kernel], set([power_manager_project, kernel_project]))
    ]
    for packages, projects in matches:
      self.assertEquals(projects,
                        portage_utilities.FindWorkonProjects(packages))

class PackageDBTest(cros_test_lib.MoxTempDirTestCase):
  fake_pkgdb = { 'category1' : [ 'package-1', 'package-2' ],
                 'category2' : [ 'package-3', 'package-4' ],
                 'category3' : [ 'invalid', 'semi-invalid' ],
                 'invalid' : [], }
  fake_packages = []
  build_root = None
  fake_chroot = None

  def setUp(self):
    self.build_root = self.tempdir
    # Prepare a fake chroot.
    self.fake_chroot = os.path.join(self.build_root, 'chroot/build/amd64-host')
    fake_pkgdb_path = os.path.join(self.fake_chroot, 'var/db/pkg')
    os.makedirs(fake_pkgdb_path)
    for cat, pkgs in self.fake_pkgdb.iteritems():
      catpath = os.path.join(fake_pkgdb_path, cat)
      if cat == 'invalid':
        # Invalid category is a file. Should not be delved into.
        osutils.Touch(catpath)
        continue
      os.makedirs(catpath)
      for pkg in pkgs:
        pkgpath = os.path.join(catpath, pkg)
        if pkg == 'invalid':
          # Invalid package is a file instead of a directory/
          osutils.Touch(pkgpath)
          continue
        os.makedirs(pkgpath)
        if pkg.endswith('-invalid'):
          # Invalid package does not meet existence of "%s/%s.ebuild" file.
          osutils.Touch(os.path.join(pkgpath, 'whatever'))
          continue
        # Correct pkg.
        osutils.Touch(os.path.join(pkgpath, pkg + '.ebuild'))
        pv = portage_utilities.SplitPV(pkg)
        key = '%s/%s' % (cat, pv.package)
        self.fake_packages.append((key, pv.version))

  def testListInstalledPackages(self):
    """Test if listing packages installed into a root works."""
    packages = portage_utilities.ListInstalledPackages(self.fake_chroot)
    # Sort the lists, because the filesystem might reorder the entries for us.
    packages.sort()
    self.fake_packages.sort()
    self.assertEquals(self.fake_packages, packages)


if __name__ == '__main__':
  cros_test_lib.main()
