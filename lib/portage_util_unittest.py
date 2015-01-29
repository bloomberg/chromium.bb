# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for portage_util.py."""

from __future__ import print_function

import fileinput
import mox
import os

from chromite.cbuildbot import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import git
from chromite.lib import osutils
from chromite.lib import portage_util


MANIFEST = git.ManifestCheckout.Cached(constants.SOURCE_ROOT)


# pylint: disable=protected-access


class _Package(object):
  """Package helper class."""

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
  """Ebuild related tests."""

  def _makeFakeEbuild(self, fake_ebuild_path, fake_ebuild_content=''):
    self.mox.StubOutWithMock(fileinput, 'input')
    fileinput.input(fake_ebuild_path).AndReturn(fake_ebuild_content)
    self.mox.ReplayAll()
    fake_ebuild = portage_util.EBuild(fake_ebuild_path)
    self.mox.VerifyAll()
    return fake_ebuild

  def testParseEBuildPath(self):
    # Test with ebuild with revision number.
    fake_ebuild_path = '/path/to/test_package/test_package-0.0.1-r1.ebuild'
    fake_ebuild = self._makeFakeEbuild(fake_ebuild_path)

    self.assertEquals(fake_ebuild.category, 'to')
    self.assertEquals(fake_ebuild.pkgname, 'test_package')
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

    self.assertEquals(fake_ebuild.category, 'to')
    self.assertEquals(fake_ebuild.pkgname, 'test_package')
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
    self.mox.StubOutWithMock(cros_build_lib, 'RunCommand')
    result = _DummyCommandResult(fake_hash)
    cros_build_lib.RunCommand(
        mox.IgnoreArg(),
        cwd=mox.IgnoreArg(),
        print_cmd=portage_util.EBuild.VERBOSE,
        capture_output=True).AndReturn(result)

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
  """Project and Path related tests."""

  def _MockParseWorkonVariables(self, fake_projects, _fake_localname,
                                _fake_subdir, fake_ebuild_contents):
    """Mock the necessary calls, start Replay mode, call GetSourcePath()."""
    self.mox.StubOutWithMock(os.path, 'isdir')
    self.mox.StubOutWithMock(MANIFEST, 'FindCheckoutFromPath')

    # We need 'chromeos-base' here because it controls default _SUBDIR values.
    ebuild_path = os.path.join(self.tempdir, 'chromeos-base', 'package',
                               'package-9999.ebuild')
    osutils.WriteFile(ebuild_path, fake_ebuild_contents, makedirs=True)
    for p in fake_projects:
      os.path.isdir(mox.IgnoreArg()).AndReturn(False)
    for p in fake_projects:
      os.path.isdir(mox.IgnoreArg()).AndReturn(True)
      MANIFEST.FindCheckoutFromPath(mox.IgnoreArg()).AndReturn({'name': p})
    self.mox.ReplayAll()

    ebuild = portage_util.EBuild(ebuild_path)
    result = ebuild.GetSourcePath(self.tempdir, MANIFEST)
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
    """ % (portage_util.EBuild.FormatBashArray(fake_projects),
           portage_util.EBuild.FormatBashArray(fake_localname),
           portage_util.EBuild.FormatBashArray(fake_subdir))
    project, subdir = self._MockParseWorkonVariables(
        fake_projects, fake_localname, fake_subdir, fake_ebuild_contents)
    self.assertEquals(project, fake_projects)
    fake_path = ['%s/%s' % (fake_localname[i], fake_subdir[i])
                 for i in range(0, len(fake_projects))]
    fake_path = [os.path.realpath(os.path.join(self.tempdir, 'platform', x))
                 for x in fake_path]
    self.assertEquals(subdir, fake_path)


class StubEBuild(portage_util.EBuild):
  """Test helper to StubEBuild."""

  def __init__(self, path):
    super(StubEBuild, self).__init__(path)
    self.is_workon = True
    self.is_stable = True

  def _ReadEBuild(self, path):
    pass

  def GetCommitId(self, srcpath):
    id_map = {
        'p1_path': 'my_id',
        'p1_path1': 'my_id1',
        'p1_path2': 'my_id2'
    }
    if srcpath in id_map:
      return id_map[srcpath]
    else:
      return 'you_lose'


class EBuildRevWorkonTest(cros_test_lib.MoxTempDirTestCase):
  """Tests for EBuildRevWorkon."""
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
    self.mox.StubOutWithMock(portage_util.shutil, 'copyfile')
    self.mox.StubOutWithMock(os, 'unlink')
    self.mox.StubOutWithMock(portage_util.EBuild, '_RunGit')
    self.mox.StubOutWithMock(cros_build_lib, 'RunCommand')
    self.mox.StubOutWithMock(portage_util.filecmp, 'cmp')
    self.mox.StubOutWithMock(portage_util.fileinput, 'input')
    self.mox.StubOutWithMock(portage_util.EBuild, 'GetVersion')
    self.mox.StubOutWithMock(portage_util.EBuild, 'GetSourcePath')
    self.mox.StubOutWithMock(portage_util.EBuild, 'GetTreeId')

    # pylint: disable=no-value-for-parameter
    if multi:
      portage_util.EBuild.GetSourcePath('/sources', MANIFEST).AndReturn(
          (['fake_project1', 'fake_project2'], ['p1_path1', 'p1_path2']))
    else:
      portage_util.EBuild.GetSourcePath('/sources', MANIFEST).AndReturn(
          (['fake_project1'], ['p1_path']))
    portage_util.EBuild.GetVersion('/sources', MANIFEST, '0.0.1').AndReturn(
        '0.0.1')
    if multi:
      portage_util.EBuild.GetTreeId('p1_path1').AndReturn('treehash1')
      portage_util.EBuild.GetTreeId('p1_path2').AndReturn('treehash2')
    else:
      portage_util.EBuild.GetTreeId('p1_path').AndReturn('treehash')

    ebuild_9999 = self.m_ebuild._unstable_ebuild_path
    os.path.exists(ebuild_9999).AndReturn(True)

    # These calls come from MarkAsStable()
    portage_util.shutil.copyfile(ebuild_9999, self.revved_ebuild_path)

    m_file = self.mox.CreateMock(file)
    portage_util.fileinput.input(self.revved_ebuild_path,
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

    portage_util.filecmp.cmp(self.m_ebuild.ebuild_path,
                             self.revved_ebuild_path,
                             shallow=False).AndReturn(not rev)
    if rev:
      cmd = ['add', self.revved_ebuild_path]
      portage_util.EBuild._RunGit(self.overlay, cmd)
      if self.m_ebuild.is_stable:
        cmd = ['rm', self.m_ebuild.ebuild_path]
        portage_util.EBuild._RunGit(self.overlay, cmd)
    else:
      os.unlink(self.revved_ebuild_path)

    return m_file

  def testRevWorkOnEBuild(self):
    """Test Uprev of a single project ebuild."""
    m_file = self.createRevWorkOnMocks(self._mock_ebuild, rev=True)
    self.mox.ReplayAll()
    result = self.m_ebuild.RevWorkOnEBuild('/sources', MANIFEST,
                                           redirect_file=m_file)
    self.mox.VerifyAll()
    self.assertEqual(result, 'category/test_package-0.0.1-r2')

  def testRevWorkOnMultiEBuild(self):
    """Test Uprev of a multi-project (array) ebuild."""
    m_file = self.createRevWorkOnMocks(self._mock_ebuild_multi, rev=True,
                                       multi=True)
    self.mox.ReplayAll()
    result = self.m_ebuild.RevWorkOnEBuild('/sources', MANIFEST,
                                           redirect_file=m_file)
    self.mox.VerifyAll()
    self.assertEqual(result, 'category/test_package-0.0.1-r2')

  def testRevUnchangedEBuild(self):
    m_file = self.createRevWorkOnMocks(self._mock_ebuild, rev=False)

    self.mox.ReplayAll()
    result = self.m_ebuild.RevWorkOnEBuild('/sources', MANIFEST,
                                           redirect_file=m_file)
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
    result = self.m_ebuild.RevWorkOnEBuild('/sources', MANIFEST,
                                           redirect_file=m_file)
    self.mox.VerifyAll()
    self.assertEqual(result, 'category/test_package-0.0.1-r1')

  def testCommitChange(self):
    self.mox.StubOutWithMock(cros_build_lib, 'RunCommand')
    mock_message = 'Commitme'
    git.RunGit('.', ['commit', '-a', '-m', mock_message])
    self.mox.ReplayAll()
    self.m_ebuild.CommitChange(mock_message, '.')
    self.mox.VerifyAll()

  def testUpdateCommitHashesForChanges(self):
    """Tests that we can update the commit hashes for changes correctly."""
    cls = portage_util.EBuild
    ebuild1 = self.mox.CreateMock(cls)
    ebuild1.ebuild_path = 'public_overlay/ebuild.ebuild'
    ebuild1.package = 'test/project'

    self.mox.StubOutWithMock(portage_util, 'FindOverlays')
    self.mox.StubOutWithMock(cls, '_GetEBuildPaths')
    self.mox.StubOutWithMock(cls, '_GetSHA1ForPath')
    self.mox.StubOutWithMock(cls, 'UpdateEBuild')
    self.mox.StubOutWithMock(cls, 'CommitChange')
    self.mox.StubOutWithMock(cls, 'GitRepoHasChanges')

    build_root = 'fakebuildroot'
    overlays = ['public_overlay']
    changes = ['fake change']
    paths = ['fake_path1', 'fake_path2']
    path_ebuilds = {ebuild1: paths}
    portage_util.FindOverlays(
        constants.BOTH_OVERLAYS, buildroot=build_root).AndReturn(overlays)
    cls._GetEBuildPaths(build_root, mox.IgnoreArg(), overlays,
                        changes).AndReturn(path_ebuilds)
    for i, p in enumerate(paths):
      cls._GetSHA1ForPath(mox.IgnoreArg(), p).InAnyOrder().AndReturn(str(i))
    cls.UpdateEBuild(ebuild1.ebuild_path, dict(CROS_WORKON_COMMIT='("0" "1")'))
    cls.GitRepoHasChanges('public_overlay').AndReturn(True)
    cls.CommitChange(mox.IgnoreArg(), overlay='public_overlay')
    self.mox.ReplayAll()
    cls.UpdateCommitHashesForChanges(changes, build_root, MANIFEST)
    self.mox.VerifyAll()

  def testGitRepoHasChanges(self):
    """Tests that GitRepoHasChanges works correctly."""
    url = 'file://' + os.path.join(constants.SOURCE_ROOT, 'chromite')
    git.RunGit(self.tempdir, ['clone', '--depth=1', url, self.tempdir])
    # No changes yet as we just cloned the repo.
    self.assertFalse(portage_util.EBuild.GitRepoHasChanges(self.tempdir))
    # Update metadata but no real changes.
    osutils.Touch(os.path.join(self.tempdir, 'LICENSE'))
    self.assertFalse(portage_util.EBuild.GitRepoHasChanges(self.tempdir))
    # A real change.
    osutils.WriteFile(os.path.join(self.tempdir, 'LICENSE'), 'hi')
    self.assertTrue(portage_util.EBuild.GitRepoHasChanges(self.tempdir))


class ListOverlaysTest(cros_test_lib.MockTempDirTestCase):
  """Tests related to listing overlays."""

  def testMissingOverlays(self):
    """Tests that exceptions are raised when an overlay is missing."""
    self.assertRaises(portage_util.MissingOverlayException,
                      portage_util._ListOverlays,
                      board='foo', buildroot=self.tempdir)


class FindOverlaysTest(cros_test_lib.MockTempDirTestCase):
  """Tests related to finding overlays."""

  FAKE, PUB_PRIV, PUB_PRIV_VARIANT, PUB_ONLY, PUB2_ONLY, PRIV_ONLY = (
      'fake!board', 'pub-priv-board', 'pub-priv-board_variant',
      'pub-only-board', 'pub2-only-board', 'priv-only-board',
  )
  PRIVATE = constants.PRIVATE_OVERLAYS
  PUBLIC = constants.PUBLIC_OVERLAYS
  BOTH = constants.BOTH_OVERLAYS

  def setUp(self):
    # Create an overlay tree to run tests against and isolate ourselves from
    # changes in the main tree.
    D = cros_test_lib.Directory
    overlay_files = (D('metadata', ('layout.conf',)),)
    board_overlay_files = overlay_files + (
        'make.conf',
        'toolchain.conf',
    )
    file_layout = (
        D('src', (
            D('overlays', (
                D('overlay-%s' % self.PUB_ONLY, board_overlay_files),
                D('overlay-%s' % self.PUB2_ONLY, board_overlay_files),
                D('overlay-%s' % self.PUB_PRIV, board_overlay_files),
                D('overlay-%s' % self.PUB_PRIV_VARIANT, board_overlay_files),
            )),
            D('private-overlays', (
                D('overlay-%s' % self.PUB_PRIV, board_overlay_files),
                D('overlay-%s' % self.PUB_PRIV_VARIANT, board_overlay_files),
                D('overlay-%s' % self.PRIV_ONLY, board_overlay_files),
            )),
            D('third_party', (
                D('chromiumos-overlay', overlay_files),
                D('portage-stable', overlay_files),
            )),
        )),
    )
    cros_test_lib.CreateOnDiskHierarchy(self.tempdir, file_layout)

    # Seed the board overlays.
    conf_data = 'repo-name = %(repo-name)s\nmasters = %(masters)s'
    conf_path = os.path.join(self.tempdir, 'src', '%(private)soverlays',
                             'overlay-%(board)s', 'metadata', 'layout.conf')

    for board in (self.PUB_PRIV, self.PUB_PRIV_VARIANT, self.PUB_ONLY,
                  self.PUB2_ONLY):
      settings = {
          'board': board,
          'masters': '',
          'private': '',
          'repo-name': board,
      }
      if '_' in board:
        settings['masters'] = board.split('_')[0]
      osutils.WriteFile(conf_path % settings,
                        conf_data % settings)

    for board in (self.PUB_PRIV, self.PUB_PRIV_VARIANT, self.PRIV_ONLY):
      settings = {
          'board': board,
          'masters': '',
          'private': 'private-',
          'repo-name': '%s-private' % board,
      }
      if '_' in board:
        settings['masters'] = board.split('_')[0]
      osutils.WriteFile(conf_path % settings,
                        conf_data % settings)

    # Seed the common overlays.
    conf_path = os.path.join(self.tempdir, 'src', 'third_party', '%(overlay)s',
                             'metadata', 'layout.conf')
    osutils.WriteFile(conf_path % {'overlay': 'chromiumos-overlay'},
                      conf_data % {'repo-name': 'chromiumos', 'masters': ''})
    osutils.WriteFile(conf_path % {'overlay': 'portage-stable'},
                      conf_data % {'repo-name': 'portage-stable',
                                   'masters': ''})

    # Now build up the list of overlays that we'll use in tests below.
    self.overlays = {}
    for b in (None, self.FAKE, self.PUB_PRIV, self.PUB_PRIV_VARIANT,
              self.PUB_ONLY, self.PUB2_ONLY, self.PRIV_ONLY):
      self.overlays[b] = d = {}
      for o in (self.PRIVATE, self.PUBLIC, self.BOTH, None):
        try:
          d[o] = portage_util.FindOverlays(o, b, self.tempdir)
        except portage_util.MissingOverlayException:
          d[o] = []
    self._no_overlays = not bool(any(d.values()))

  def testMissingPrimaryOverlay(self):
    """Test what happens when a primary overlay is missing.

    If the overlay doesn't exist, FindOverlays should throw a
    MissingOverlayException.
    """
    self.assertRaises(portage_util.MissingOverlayException,
                      portage_util.FindPrimaryOverlay, self.BOTH,
                      self.FAKE, self.tempdir)

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
    """
    for b, d in self.overlays.items():
      if b == self.FAKE:
        continue
      self.assertGreaterEqual(set(d[self.BOTH]), set(d[self.PUBLIC]))
      self.assertGreater(set(d[self.BOTH]), set(d[self.PRIVATE]))
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
      self.assertLess(set(self.overlays[self.FAKE][o]),
                      set(self.overlays[self.PUB_PRIV][o]))

  def testAllBoards(self):
    """If we specify board=None, all overlays should be returned."""
    for o in (self.PUBLIC, self.BOTH):
      for b in (self.FAKE, self.PUB_PRIV):
        self.assertLess(set(self.overlays[b][o]), set(self.overlays[None][o]))

  def testPrimaryOverlays(self):
    """Verify that boards have a primary overlay.

    Further, the only difference between public boards are the primary overlay
    which should be listed last.
    """
    primary = portage_util.FindPrimaryOverlay(
        self.BOTH, self.PUB_ONLY, self.tempdir)
    self.assertIn(primary, self.overlays[self.PUB_ONLY][self.BOTH])
    self.assertNotIn(primary, self.overlays[self.PUB2_ONLY][self.BOTH])
    self.assertEqual(primary, self.overlays[self.PUB_ONLY][self.PUBLIC][-1])
    self.assertEqual(self.overlays[self.PUB_ONLY][self.PUBLIC][:-1],
                     self.overlays[self.PUB2_ONLY][self.PUBLIC][:-1])
    self.assertNotEqual(self.overlays[self.PUB_ONLY][self.PUBLIC][-1],
                        self.overlays[self.PUB2_ONLY][self.PUBLIC][-1])

  def testReadOverlayFileOrder(self):
    """Verify that the boards are examined in the right order."""
    m = self.PatchObject(os.path, 'isfile', return_value=False)
    portage_util.ReadOverlayFile('test', self.PUBLIC, self.PUB_PRIV,
                                 self.tempdir)
    read_overlays = [x[0][0][:-5] for x in m.call_args_list]
    overlays = [x for x in reversed(self.overlays[self.PUB_PRIV][self.PUBLIC])]
    self.assertEqual(read_overlays, overlays)

  def testFindOverlayFile(self):
    """Verify that the first file found is returned."""
    file_to_find = 'something_special'
    full_path = os.path.join(self.tempdir,
                             'src', 'private-overlays',
                             'overlay-%s' % self.PUB_PRIV,
                             file_to_find)
    osutils.Touch(full_path)
    self.assertEqual(full_path,
                     portage_util.FindOverlayFile(file_to_find, self.BOTH,
                                                  self.PUB_PRIV_VARIANT,
                                                  self.tempdir))

  def testFoundPrivateOverlays(self):
    """Verify that private boards had their overlays located."""
    for b in (self.PUB_PRIV, self.PUB_PRIV_VARIANT, self.PRIV_ONLY):
      self.assertNotEqual(self.overlays[b][self.PRIVATE], [])
    self.assertNotEqual(self.overlays[self.PUB_PRIV][self.BOTH],
                        self.overlays[self.PUB_PRIV][self.PRIVATE])
    self.assertNotEqual(self.overlays[self.PUB_PRIV_VARIANT][self.BOTH],
                        self.overlays[self.PUB_PRIV_VARIANT][self.PRIVATE])

  def testFoundPublicOverlays(self):
    """Verify that public boards had their overlays located."""
    for b in (self.PUB_PRIV, self.PUB_PRIV_VARIANT, self.PUB_ONLY,
              self.PUB2_ONLY):
      self.assertNotEqual(self.overlays[b][self.PUBLIC], [])
    self.assertNotEqual(self.overlays[self.PUB_PRIV][self.BOTH],
                        self.overlays[self.PUB_PRIV][self.PUBLIC])
    self.assertNotEqual(self.overlays[self.PUB_PRIV_VARIANT][self.BOTH],
                        self.overlays[self.PUB_PRIV_VARIANT][self.PUBLIC])

  def testFoundParentOverlays(self):
    """Verify that the overlays for a parent board are found."""
    for d in self.PUBLIC, self.PRIVATE:
      self.assertLess(set(self.overlays[self.PUB_PRIV][d]),
                      set(self.overlays[self.PUB_PRIV_VARIANT][d]))


class UtilFuncsTest(cros_test_lib.TempDirTestCase):
  """Basic tests for utility functions"""

  def _CreateProfilesRepoName(self, name):
    """Write |name| to profiles/repo_name"""
    profiles = os.path.join(self.tempdir, 'profiles')
    osutils.SafeMakedirs(profiles)
    repo_name = os.path.join(profiles, 'repo_name')
    osutils.WriteFile(repo_name, name)

  def testGetOverlayNameNone(self):
    """If the overlay has no name, it should be fine"""
    self.assertEqual(portage_util.GetOverlayName(self.tempdir), None)

  def testGetOverlayNameProfilesRepoName(self):
    """Verify profiles/repo_name can be read"""
    self._CreateProfilesRepoName('hi!')
    self.assertEqual(portage_util.GetOverlayName(self.tempdir), 'hi!')

  def testGetOverlayNameProfilesLayoutConf(self):
    """Verify metadata/layout.conf is read before profiles/repo_name"""
    self._CreateProfilesRepoName('hi!')
    metadata = os.path.join(self.tempdir, 'metadata')
    osutils.SafeMakedirs(metadata)
    layout_conf = os.path.join(metadata, 'layout.conf')
    osutils.WriteFile(layout_conf, 'repo-name = bye')
    self.assertEqual(portage_util.GetOverlayName(self.tempdir), 'bye')

  def testGetOverlayNameProfilesLayoutConfNoRepoName(self):
    """Verify metadata/layout.conf w/out repo-name is ignored"""
    self._CreateProfilesRepoName('hi!')
    metadata = os.path.join(self.tempdir, 'metadata')
    osutils.SafeMakedirs(metadata)
    layout_conf = os.path.join(metadata, 'layout.conf')
    osutils.WriteFile(layout_conf, 'here = we go')
    self.assertEqual(portage_util.GetOverlayName(self.tempdir), 'hi!')


class BuildEBuildDictionaryTest(cros_test_lib.MoxTestCase):
  """Tests of the EBuild Dictionary."""

  def setUp(self):
    self.mox.StubOutWithMock(os, 'walk')
    self.mox.StubOutWithMock(cros_build_lib, 'RunCommand')
    self.package = 'chromeos-base/test_package'
    self.root = '/overlay/chromeos-base/test_package'
    self.package_path = self.root + '/test_package-0.0.1.ebuild'
    paths = [[self.root, [], []]]
    os.walk('/overlay').AndReturn(paths)
    self.mox.StubOutWithMock(portage_util, '_FindUprevCandidates')

  def testWantedPackage(self):
    overlays = {'/overlay': []}
    package = _Package(self.package)
    portage_util._FindUprevCandidates([]).AndReturn(package)
    self.mox.ReplayAll()
    portage_util.BuildEBuildDictionary(overlays, False, [self.package])
    self.mox.VerifyAll()
    self.assertEquals(len(overlays), 1)
    self.assertEquals(overlays['/overlay'], [package])

  def testUnwantedPackage(self):
    overlays = {'/overlay': []}
    package = _Package(self.package)
    portage_util._FindUprevCandidates([]).AndReturn(package)
    self.mox.ReplayAll()
    portage_util.BuildEBuildDictionary(overlays, False, [])
    self.assertEquals(len(overlays), 1)
    self.assertEquals(overlays['/overlay'], [])
    self.mox.VerifyAll()


class ProjectMappingTest(cros_test_lib.TestCase):
  """Tests related to Proejct Mapping."""

  def testSplitEbuildPath(self):
    """Test if we can split an ebuild path into its components."""
    ebuild_path = 'chromeos-base/platform2/platform2-9999.ebuild'
    components = ['chromeos-base', 'platform2', 'platform2-9999']
    for path in (ebuild_path, './' + ebuild_path, 'foo.bar/' + ebuild_path):
      self.assertEquals(components, portage_util.SplitEbuildPath(path))

  def testSplitPV(self):
    """Test splitting PVs into package and version components."""
    pv = 'bar-1.2.3_rc1-r5'
    package, version_no_rev, rev = tuple(pv.split('-'))
    split_pv = portage_util.SplitPV(pv)
    self.assertEquals(split_pv.pv, pv)
    self.assertEquals(split_pv.package, package)
    self.assertEquals(split_pv.version_no_rev, version_no_rev)
    self.assertEquals(split_pv.rev, rev)
    self.assertEquals(split_pv.version, '%s-%s' % (version_no_rev, rev))

  def testSplitCPV(self):
    """Test splitting CPV into components."""
    cpv = 'foo/bar-4.5.6_alpha-r6'
    cat, pv = cpv.split('/', 1)
    split_pv = portage_util.SplitPV(pv)
    split_cpv = portage_util.SplitCPV(cpv)
    self.assertEquals(split_cpv.category, cat)
    for k, v in split_pv._asdict().iteritems():
      self.assertEquals(getattr(split_cpv, k), v)

  def testFindWorkonProjects(self):
    """Test if we can find the list of workon projects."""
    ply_image = 'media-gfx/ply-image'
    ply_image_project = 'chromiumos/third_party/ply-image'
    kernel = 'sys-kernel/chromeos-kernel'
    kernel_project = 'chromiumos/third_party/kernel'
    matches = [
        ([ply_image], set([ply_image_project])),
        ([kernel], set([kernel_project])),
        ([ply_image, kernel], set([ply_image_project, kernel_project]))
    ]
    if portage_util.FindOverlays(constants.BOTH_OVERLAYS):
      for packages, projects in matches:
        self.assertEquals(projects,
                          portage_util.FindWorkonProjects(packages))


class PortageDBTest(cros_test_lib.MoxTempDirTestCase):
  """Portage package Database related tests."""

  fake_pkgdb = {'category1': ['package-1', 'package-2'],
                'category2': ['package-3', 'package-4'],
                'category3': ['invalid', 'semi-invalid'],
                'with': ['files-1'],
                'dash-category': ['package-5'],
                '-invalid': ['package-6'],
                'invalid': []}
  fake_packages = []
  build_root = None
  fake_chroot = None

  fake_files = [
      ('dir', '/lib64'),
      ('obj', '/lib64/libext2fs.so.2.4', 'a6723f44cf82f1979e9731043f820d8c',
       '1390848093'),
      ('dir', '/dir with spaces'),
      ('obj', '/dir with spaces/file with spaces',
       'cd4865bbf122da11fca97a04dfcac258', '1390848093'),
      ('sym', '/lib64/libe2p.so.2', '->', 'libe2p.so.2.3', '1390850489'),
      ('foo'),
  ]

  def setUp(self):
    self.build_root = self.tempdir
    self.fake_packages = []
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
        # Create the package.
        osutils.Touch(os.path.join(pkgpath, pkg + '.ebuild'))
        if cat.startswith('-'):
          # Invalid category.
          continue
        # Correct pkg.
        pv = portage_util.SplitPV(pkg)
        key = '%s/%s' % (cat, pv.package)
        self.fake_packages.append((key, pv.version))
    # Add contents to with/files-1.
    osutils.WriteFile(
        os.path.join(fake_pkgdb_path, 'with', 'files-1', 'CONTENTS'),
        ''.join(' '.join(entry) + '\n' for entry in self.fake_files))

  def testListInstalledPackages(self):
    """Test if listing packages installed into a root works."""
    packages = portage_util.ListInstalledPackages(self.fake_chroot)
    # Sort the lists, because the filesystem might reorder the entries for us.
    packages.sort()
    self.fake_packages.sort()
    self.assertEquals(self.fake_packages, packages)

  def testIsPackageInstalled(self):
    """Test if checking the existence of an installed package works."""
    self.assertTrue(portage_util.IsPackageInstalled(
        'category1/package',
        sysroot=self.fake_chroot))
    self.assertFalse(portage_util.IsPackageInstalled(
        'category1/foo',
        sysroot=self.fake_chroot))

  def testListContents(self):
    """Test if the list of installed files is properly parsed."""
    pdb = portage_util.PortageDB(self.fake_chroot)
    pkg = pdb.GetInstalledPackage('with', 'files-1')
    self.assertTrue(pkg)
    lst = pkg.ListContents()

    # Check ListContents filters out the garbage we added to the list of files.
    fake_files = [f for f in self.fake_files if f[0] in ('sym', 'obj', 'dir')]
    self.assertEquals(len(fake_files), len(lst))

    # Check the paths are all relative.
    self.assertTrue(all(not f[1].startswith('/') for f in lst))

    # Check all the files are present. We only consider file type and path, and
    # convert the path to a relative path.
    fake_files = [(f[0], f[1].lstrip('/')) for f in fake_files]
    self.assertEquals(fake_files, lst)


class InstalledPackageTest(cros_test_lib.MoxTempDirTestCase):
  """InstalledPackage class tests outside a PortageDB."""

  def setUp(self):
    osutils.WriteFile(os.path.join(self.tempdir, 'package-1.ebuild'), 'EAPI=1')
    osutils.WriteFile(os.path.join(self.tempdir, 'PF'), 'package-1')
    osutils.WriteFile(os.path.join(self.tempdir, 'CATEGORY'), 'category-1')

  def testOutOfDBPackage(self):
    """Tests an InstalledPackage instance can be created without a PortageDB."""
    pkg = portage_util.InstalledPackage(None, self.tempdir)
    self.assertEquals('package-1', pkg.pf)
    self.assertEquals('category-1', pkg.category)

  def testIncompletePackage(self):
    """Tests an incomplete or otherwise invalid package raises an exception."""
    # No package name is provided.
    os.unlink(os.path.join(self.tempdir, 'PF'))
    self.assertRaises(portage_util.PortageDBException,
                      portage_util.InstalledPackage, None, self.tempdir)

    # Check that doesn't fail when the package name is provided.
    pkg = portage_util.InstalledPackage(None, self.tempdir, pf='package-1')
    self.assertEquals('package-1', pkg.pf)
