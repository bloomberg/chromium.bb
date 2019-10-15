# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for portage_util.py."""

from __future__ import print_function

import os

from six.moves import StringIO

from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import failures_lib
from chromite.lib import git
from chromite.lib import osutils
from chromite.lib import portage_util


MANIFEST = git.ManifestCheckout.Cached(constants.SOURCE_ROOT)


# pylint: disable=protected-access


class _Package(object):
  """Package helper class."""

  def __init__(self, package):
    self.package = package


class EBuildTest(cros_test_lib.MockTempDirTestCase):
  """Ebuild related tests."""
  _MULTILINE_WITH_TEST = """
hello
src_test() {
}"""

  _MULTILINE_NO_TEST = """
hello
src_compile() {
}"""

  _MULTILINE_COMMENTED = """
#src_test() {
# notactive
# }"""

  _MULTILINE_PLATFORM = """
platform_pkg_test() {
}"""

  _AUTOTEST_NORMAL = (
      '\n\t+tests_fake_Test1\n\t+tests_fake_Test2\n',
      ('fake_Test1', 'fake_Test2',))

  _AUTOTEST_EXTRA_PLUS = (
      '\n\t++++tests_fake_Test1\n', ('fake_Test1',))

  _AUTOTEST_EXTRA_TAB = (
      '\t\t\n\t\n\t+tests_fake_Test1\tfoo\n', ('fake_Test1',))

  _SINGLE_LINE_TEST = 'src_test() { echo "foo" }'

  _INHERIT_CROS_GO = 'inherit cros-workon cros-go'

  _INHERIT_TAST_BUNDLE = 'inherit tast-bundle'

  _INHERIT_CROS_DEBUG = 'inherit cros-debug'

  _EBUILD_BASE = """
CROS_WORKON_COMMIT=commit1
CROS_WORKON_TREE=("tree1" "tree2")
inherit cros-workon
"""

  _EBUILD_DIFFERENT_COMMIT = """
CROS_WORKON_COMMIT=commit9
CROS_WORKON_TREE=("tree1" "tree2")
inherit cros-workon
"""

  _EBUILD_DIFFERENT_TREE = """
CROS_WORKON_COMMIT=commit1
CROS_WORKON_TREE=("tree9" "tree2")
inherit cros-workon
"""

  _EBUILD_DIFFERENT_CONTENT = """
CROS_WORKON_COMMIT=commit1
CROS_WORKON_TREE=("tree1" "tree2")
inherit cros-workon superpower
"""

  def _MakeFakeEbuild(self, fake_ebuild_path, fake_ebuild_content=''):
    osutils.WriteFile(fake_ebuild_path, fake_ebuild_content, makedirs=True)
    fake_ebuild = portage_util.EBuild(fake_ebuild_path, False)
    return fake_ebuild

  def testParseEBuildPath(self):
    """Test with ebuild with revision number."""
    basedir = os.path.join(self.tempdir, 'cat', 'test_package')
    fake_ebuild_path = os.path.join(basedir, 'test_package-0.0.1-r1.ebuild')
    fake_ebuild = self._MakeFakeEbuild(fake_ebuild_path)

    self.assertEqual(fake_ebuild.category, 'cat')
    self.assertEqual(fake_ebuild.pkgname, 'test_package')
    self.assertEqual(fake_ebuild.version_no_rev, '0.0.1')
    self.assertEqual(fake_ebuild.current_revision, 1)
    self.assertEqual(fake_ebuild.version, '0.0.1-r1')
    self.assertEqual(fake_ebuild.package, 'cat/test_package')
    self.assertEqual(fake_ebuild._ebuild_path_no_version,
                     os.path.join(basedir, 'test_package'))
    self.assertEqual(fake_ebuild.ebuild_path_no_revision,
                     os.path.join(basedir, 'test_package-0.0.1'))
    self.assertEqual(fake_ebuild._unstable_ebuild_path,
                     os.path.join(basedir, 'test_package-9999.ebuild'))
    self.assertEqual(fake_ebuild.ebuild_path, fake_ebuild_path)

  def testParseEBuildPathNoRevisionNumber(self):
    """Test with ebuild without revision number."""
    basedir = os.path.join(self.tempdir, 'cat', 'test_package')
    fake_ebuild_path = os.path.join(basedir, 'test_package-9999.ebuild')
    fake_ebuild = self._MakeFakeEbuild(fake_ebuild_path)

    self.assertEqual(fake_ebuild.category, 'cat')
    self.assertEqual(fake_ebuild.pkgname, 'test_package')
    self.assertEqual(fake_ebuild.version_no_rev, '9999')
    self.assertEqual(fake_ebuild.current_revision, 0)
    self.assertEqual(fake_ebuild.version, '9999')
    self.assertEqual(fake_ebuild.package, 'cat/test_package')
    self.assertEqual(fake_ebuild._ebuild_path_no_version,
                     os.path.join(basedir, 'test_package'))
    self.assertEqual(fake_ebuild.ebuild_path_no_revision,
                     os.path.join(basedir, 'test_package-9999'))
    self.assertEqual(fake_ebuild._unstable_ebuild_path,
                     os.path.join(basedir, 'test_package-9999.ebuild'))
    self.assertEqual(fake_ebuild.ebuild_path, fake_ebuild_path)

  def testGetCommitId(self):
    fake_hash = '24ab3c9f6d6b5c744382dba2ca8fb444b9808e9f'
    basedir = os.path.join(self.tempdir, 'cat', 'test_package')
    fake_ebuild_path = os.path.join(basedir, 'test_package-9999.ebuild')
    fake_ebuild = self._MakeFakeEbuild(fake_ebuild_path)

    # git rev-parse HEAD
    self.PatchObject(
        git, 'RunGit',
        return_value=cros_build_lib.CommandResult(stdout=fake_hash + '\n'))
    test_hash = fake_ebuild.GetCommitId(self.tempdir)
    self.assertEqual(test_hash, fake_hash)

  def testEBuildStable(self):
    """Test ebuild w/keyword variations"""
    basedir = os.path.join(self.tempdir, 'cat', 'test_package')
    fake_ebuild_path = os.path.join(basedir, 'test_package-9999.ebuild')

    datasets = (
        ('~amd64', False),
        ('amd64', True),
        ('~amd64 ~arm ~x86', False),
        ('~amd64 arm ~x86', True),
        ('-* ~arm', False),
        ('-* x86', True),
    )
    for keywords, stable in datasets:
      fake_ebuild = self._MakeFakeEbuild(
          fake_ebuild_path, fake_ebuild_content=['KEYWORDS="%s"\n' % keywords])
      self.assertEqual(fake_ebuild.is_stable, stable)

  def testEBuildBlacklisted(self):
    """Test blacklisted ebuild"""
    basedir = os.path.join(self.tempdir, 'cat', 'test_package')
    fake_ebuild_path = os.path.join(basedir, 'test_package-9999.ebuild')

    fake_ebuild = self._MakeFakeEbuild(fake_ebuild_path)
    self.assertEqual(fake_ebuild.is_blacklisted, False)

    fake_ebuild = self._MakeFakeEbuild(
        fake_ebuild_path, fake_ebuild_content=['CROS_WORKON_BLACKLIST="1"\n'])
    self.assertEqual(fake_ebuild.is_blacklisted, True)

  def testHasTest(self):
    """Tests that we detect test stanzas correctly."""
    def run_case(content, expected):
      with osutils.TempDir() as temp:
        ebuild = os.path.join(temp, 'overlay', 'app-misc',
                              'foo-0.0.1-r1.ebuild')
        osutils.WriteFile(ebuild, content, makedirs=True)
        self.assertEqual(expected, portage_util.EBuild(ebuild, False).has_test)

    run_case(self._MULTILINE_WITH_TEST, True)
    run_case(self._MULTILINE_NO_TEST, False)
    run_case(self._MULTILINE_COMMENTED, False)
    run_case(self._MULTILINE_PLATFORM, True)
    run_case(self._SINGLE_LINE_TEST, True)
    run_case(self._INHERIT_CROS_GO, True)
    run_case(self._INHERIT_TAST_BUNDLE, True)
    run_case(self._INHERIT_CROS_DEBUG, False)

  def testCheckHasTestWithoutEbuild(self):
    """Test CheckHasTest on a package without ebuild config file"""
    package_name = 'chromeos-base/temp_mypackage'
    package_path = os.path.join(self.tempdir, package_name)
    os.makedirs(package_path)
    with self.assertRaises(failures_lib.PackageBuildFailure):
      portage_util._CheckHasTest(self.tempdir, package_name)

  def testEBuildGetAutotestTests(self):
    """Test extraction of test names from IUSE_TESTS variable.

    Used for autotest ebuilds.
    """

    def run_case(tests_str, results):
      settings = {'IUSE_TESTS' : tests_str}
      self.assertEqual(
          portage_util.EBuild._GetAutotestTestsFromSettings(settings), results)

    run_case(self._AUTOTEST_NORMAL[0], list(self._AUTOTEST_NORMAL[1]))
    run_case(self._AUTOTEST_EXTRA_PLUS[0], list(self._AUTOTEST_EXTRA_PLUS[1]))
    run_case(self._AUTOTEST_EXTRA_TAB[0], list(self._AUTOTEST_EXTRA_TAB[1]))

  def testAlmostSameEBuilds(self):
    """Test _AlmostSameEBuilds()."""
    def AlmostSameEBuilds(ebuild1_contents, ebuild2_contents):
      ebuild1_path = os.path.join(self.tempdir, 'a.ebuild')
      ebuild2_path = os.path.join(self.tempdir, 'b.ebuild')
      osutils.WriteFile(ebuild1_path, ebuild1_contents)
      osutils.WriteFile(ebuild2_path, ebuild2_contents)
      return portage_util.EBuild._AlmostSameEBuilds(ebuild1_path, ebuild2_path)

    self.assertTrue(
        AlmostSameEBuilds(self._EBUILD_BASE, self._EBUILD_BASE))
    self.assertTrue(
        AlmostSameEBuilds(self._EBUILD_BASE, self._EBUILD_DIFFERENT_COMMIT))
    self.assertFalse(
        AlmostSameEBuilds(self._EBUILD_BASE, self._EBUILD_DIFFERENT_TREE))
    self.assertFalse(
        AlmostSameEBuilds(self._EBUILD_BASE, self._EBUILD_DIFFERENT_CONTENT))


class ProjectAndPathTest(cros_test_lib.MockTempDirTestCase):
  """Project and Path related tests."""

  def _MockParseWorkonVariables(self, fake_projects, fake_srcpaths,
                                fake_localnames, fake_ebuild_contents):
    """Mock the necessary calls, call GetSourceInfo()."""

    def _isdir(path):
      """Mock function for os.path.isdir"""
      if any(fake_srcpaths):
        if path == os.path.join(self.tempdir, 'src'):
          return True

      for srcpath in fake_srcpaths:
        if srcpath:
          if path == os.path.join(self.tempdir, 'src', srcpath):
            return True
        else:
          for localname in fake_localnames:
            if path == os.path.join(self.tempdir, localname):
              return False
            elif path == os.path.join(self.tempdir, 'platform', localname):
              return True

      raise Exception('unhandled path: %s' % path)

    def _FindCheckoutFromPath(path):
      """Mock function for manifest.FindCheckoutFromPath"""
      for project, localname in zip(fake_projects, fake_localnames):
        if path == os.path.join(self.tempdir, 'platform', localname):
          return {'name': project}
      return {}

    self.PatchObject(os.path, 'isdir', side_effect=_isdir)
    self.PatchObject(MANIFEST, 'FindCheckoutFromPath',
                     side_effect=_FindCheckoutFromPath)

    if not fake_srcpaths:
      fake_srcpaths = [''] * len(fake_projects)
    if not fake_projects:
      fake_projects = [''] * len(fake_srcpaths)

    # We need 'chromeos-base' here because it controls default _SUBDIR values.
    ebuild_path = os.path.join(self.tempdir, 'packages', 'chromeos-base',
                               'package', 'package-9999.ebuild')
    osutils.WriteFile(ebuild_path, fake_ebuild_contents, makedirs=True)

    ebuild = portage_util.EBuild(ebuild_path, False)
    return ebuild.GetSourceInfo(self.tempdir, MANIFEST)

  def testParseLegacyWorkonVariables(self):
    """Tests if ebuilds in a single item format are correctly parsed."""
    fake_project = 'my_project1'
    fake_localname = 'foo'
    fake_ebuild_contents = """
CROS_WORKON_PROJECT=%s
CROS_WORKON_LOCALNAME=%s
    """ % (fake_project, fake_localname)
    info = self._MockParseWorkonVariables(
        [fake_project], [], [fake_localname], fake_ebuild_contents)
    self.assertEqual(info.projects, [fake_project])
    self.assertEqual(info.srcdirs, [os.path.join(
        self.tempdir, 'platform', fake_localname)])
    self.assertEqual(info.subtrees, [os.path.join(
        self.tempdir, 'platform', fake_localname)])

  def testParseAlwaysLive(self):
    """Tests if an ebuild which is always live is correctly handled."""
    fake_project = 'my_project1'
    fake_localname = 'foo'
    fake_ebuild_contents = """
CROS_WORKON_PROJECT=%s
CROS_WORKON_LOCALNAME=%s
CROS_WORKON_ALWAYS_LIVE=1
    """ % (fake_project, fake_localname)
    info = self._MockParseWorkonVariables(
        [fake_project], [], [fake_localname], fake_ebuild_contents)
    self.assertEqual(info.projects, [])
    self.assertEqual(info.srcdirs, [])
    self.assertEqual(info.srcdirs, [])
    self.assertEqual(info.subtrees, [])

  def testParseArrayWorkonVariables(self):
    """Tests if ebuilds in an array format are correctly parsed."""
    fake_projects = ['my_project1', 'my_project2', 'my_project3']
    fake_localnames = ['foo', 'bar', 'bas']
    # The test content is formatted using the same function that
    # formats ebuild output, ensuring that we can parse our own
    # products.
    fake_ebuild_contents = """
CROS_WORKON_PROJECT=%s
CROS_WORKON_LOCALNAME=%s
    """ % (portage_util.EBuild.FormatBashArray(fake_projects),
           portage_util.EBuild.FormatBashArray(fake_localnames))
    info = self._MockParseWorkonVariables(
        fake_projects, [], fake_localnames, fake_ebuild_contents)
    self.assertEqual(info.projects, fake_projects)
    fake_paths = [
        os.path.realpath(os.path.join(self.tempdir, 'platform', x))
        for x in fake_localnames
    ]
    self.assertEqual(info.srcdirs, fake_paths)
    self.assertEqual(info.subtrees, fake_paths)

  def testParseArrayWorkonVariablesWithSrcpaths(self):
    """Tests if ebuilds with CROS_WORKON_SRCPATH are handled correctly."""
    fake_projects = ['my_project1', '', '']
    fake_srcpaths = ['', 'path/to/src', 'path/to/other/src']
    fake_localnames = ['foo', 'bar', 'bas']
    # The test content is formatted using the same function that
    # formats ebuild output, ensuring that we can parse our own
    # products.
    fake_ebuild_contents = """
CROS_WORKON_PROJECT=%s
CROS_WORKON_SRCPATH=%s
CROS_WORKON_LOCALNAME=%s
    """ % (portage_util.EBuild.FormatBashArray(fake_projects),
           portage_util.EBuild.FormatBashArray(fake_srcpaths),
           portage_util.EBuild.FormatBashArray(fake_localnames))
    info = self._MockParseWorkonVariables(
        fake_projects, fake_srcpaths, fake_localnames, fake_ebuild_contents)
    self.assertEqual(info.projects, fake_projects)
    fake_paths = []
    for srcpath, localname in zip(fake_srcpaths, fake_localnames):
      if srcpath:
        path = os.path.realpath(os.path.join(self.tempdir, 'src', srcpath))
      else:
        path = os.path.realpath(os.path.join(
            self.tempdir, 'platform', localname))
      fake_paths.append(path)

    self.assertEqual(info.srcdirs, fake_paths)
    self.assertEqual(info.subtrees, fake_paths)

  def testParseArrayWorkonVariablesWithSubtrees(self):
    """Tests if ebuilds with CROS_WORKON_SUBTREE are handled correctly."""
    fake_project = 'my_project1'
    fake_localname = 'foo/bar'
    fake_subtrees = 'test baz/quz'
    # The test content is formatted using the same function that
    # formats ebuild output, ensuring that we can parse our own
    # products.
    fake_ebuild_contents = """
CROS_WORKON_PROJECT=%s
CROS_WORKON_LOCALNAME=%s
CROS_WORKON_SUBTREE="%s"
    """ % (fake_project, fake_localname, fake_subtrees)
    info = self._MockParseWorkonVariables(
        [fake_project], [], [fake_localname], fake_ebuild_contents)
    self.assertEqual(info.projects, [fake_project])
    self.assertEqual(info.srcdirs, [os.path.join(
        self.tempdir, 'platform', fake_localname)])
    self.assertEqual(info.subtrees, [
        os.path.join(self.tempdir, 'platform', 'foo/bar/test'),
        os.path.join(self.tempdir, 'platform', 'foo/bar/baz/quz')])


class StubEBuild(portage_util.EBuild):
  """Test helper to StubEBuild."""

  def __init__(self, path, subdir_support):
    super(StubEBuild, self).__init__(path, subdir_support)
    self.is_workon = True
    self.is_stable = True

  def _ReadEBuild(self, path):
    pass

  def GetCommitId(self, srcdir):
    id_map = {
        'p1_path1': 'my_id1',
        'p1_path2': 'my_id2'
    }
    if srcdir in id_map:
      return id_map[srcdir]
    else:
      return 'you_lose'


class EBuildRevWorkonTest(cros_test_lib.MockTempDirTestCase):
  """Tests for EBuildRevWorkon."""

  # Lines that we will feed as fake ebuild contents to
  # EBuild.MarAsStable().  This is the minimum content needed
  # to test the various branches in the function's main processing
  # loop.
  _mock_ebuild = ['EAPI=2\n',
                  'CROS_WORKON_COMMIT=old_id\n',
                  'CROS_WORKON_PROJECT=test_package\n',
                  'KEYWORDS="~x86 ~arm ~amd64"\n',
                  'src_unpack(){}\n']
  _mock_ebuild_multi = ['EAPI=2\n',
                        'CROS_WORKON_COMMIT=("old_id1","old_id2")\n',
                        'KEYWORDS="~x86 ~arm ~amd64"\n',
                        'src_unpack(){}\n']
  _mock_ebuild_subdir = ['EAPI=5\n',
                         'CROS_WORKON_COMMIT=old_id\n',
                         'CROS_WORKON_PROJECT=test_package\n',
                         'CROS_WORKON_SUBDIRS_TO_REV=( foo )\n'
                         'KEYWORDS="~x86 ~arm ~amd64"\n',
                         'src_unpack(){}\n']
  _revved_ebuild = ('EAPI=2\n'
                    'CROS_WORKON_COMMIT="my_id1"\n'
                    'CROS_WORKON_TREE=("treehash1a" "treehash1b")\n'
                    'CROS_WORKON_PROJECT=test_package\n'
                    'KEYWORDS="x86 arm amd64"\n'
                    'src_unpack(){}\n')
  _revved_ebuild_multi = (
      'EAPI=2\n'
      'CROS_WORKON_COMMIT=("my_id1" "my_id2")\n'
      'CROS_WORKON_TREE=("treehash1a" "treehash1b" "treehash2")\n'
      'KEYWORDS="x86 arm amd64"\n'
      'src_unpack(){}\n')
  _revved_ebuild_subdir = ('EAPI=5\n'
                           'CROS_WORKON_COMMIT="my_id1"\n'
                           'CROS_WORKON_TREE=("treehash1a" "treehash1b")\n'
                           'CROS_WORKON_PROJECT=test_package\n'
                           'CROS_WORKON_SUBDIRS_TO_REV=( foo )\n'
                           'KEYWORDS="x86 arm amd64"\n'
                           'src_unpack(){}\n')

  unstable_ebuild_changed = False

  def setUp(self):
    self.overlay = os.path.join(self.tempdir, 'overlay')
    package_name = os.path.join(self.overlay,
                                'category/test_package/test_package-0.0.1')
    ebuild_path = package_name + '-r1.ebuild'
    self.m_ebuild = StubEBuild(ebuild_path, False)
    self.revved_ebuild_path = package_name + '-r2.ebuild'
    self._m_file = StringIO()
    self.git_files_changed = []

  def createRevWorkOnMocks(self, ebuild_content, rev, multi=False):
    """Creates a mock environment to run RevWorkOnEBuild.

    Args:
      ebuild_content: The content of the ebuild that will be revved.
      rev: Tell _RunGit whether this is attempt an attempt to rev an ebuild.
      multi: Whether there are multiple projects to uprev.
    """
    def _GetTreeId(path):
      """Mock function for portage_util.EBuild.GetTreeId"""
      return {
          'p1_path1/a': 'treehash1a',
          'p1_path1/b': 'treehash1b',
          'p1_path2': 'treehash2',
      }.get(path)

    def _RunGit(cwd, cmd):
      """Mock function for portage_util.EBuild._RunGit"""
      if cmd[0] == 'log':
        # special case for git log in the overlay:
        # report if -9999.ebuild supposedly changed
        if cwd == self.overlay and self.unstable_ebuild_changed:
          return 'someebuild-9999.ebuild'

        file_list = cmd[cmd.index('--') + 1:]
        # Just get the last path component so we can specify the file_list
        # without concerning outselves with tempdir
        file_list = [os.path.split(f)[1] for f in file_list]
        # Return a dummy file if we have changes in any of the listed files.
        if set(self.git_files_changed).intersection(file_list):
          return 'somefile'
        return ''
      self.assertEqual(cwd, self.overlay)
      self.assertTrue(rev, msg='git should not be run when not revving')
      if cmd[0] == 'add':
        self.assertEqual(cmd, ['add', self.revved_ebuild_path])
      else:
        self.assertTrue(self.m_ebuild.is_stable)
        self.assertEqual(cmd, ['rm', '-f', self.m_ebuild.ebuild_path])

    source_mock = self.PatchObject(portage_util.EBuild, 'GetSourceInfo')
    if multi:
      source_mock.return_value = portage_util.SourceInfo(
          projects=['fake_project1', 'fake_project2'],
          srcdirs=['p1_path1', 'p1_path2'],
          subdirs=['files', None],
          subtrees=['p1_path1/a', 'p1_path1/b', 'p1_path2'])
    else:
      source_mock.return_value = portage_util.SourceInfo(
          projects=['fake_project1'],
          srcdirs=['p1_path1'],
          subdirs=['files', None],
          subtrees=['p1_path1/a', 'p1_path1/b'])

    self.PatchObject(portage_util.EBuild, 'GetTreeId', side_effect=_GetTreeId)
    self.PatchObject(portage_util.EBuild, '_RunGit', side_effect=_RunGit)

    osutils.WriteFile(self.m_ebuild._unstable_ebuild_path, ebuild_content,
                      makedirs=True)
    osutils.WriteFile(self.m_ebuild.ebuild_path, ebuild_content, makedirs=True)

  def RevWorkOnEBuild(self, *args, **kwargs):
    """Thin helper wrapper to call the function under test.

    Returns:
      (result, revved_ebuild) where result is the result from the called
      function, and revved_ebuild is the content of the revved ebuild.
    """
    m_file = StringIO()
    kwargs['redirect_file'] = m_file
    result = self.m_ebuild.RevWorkOnEBuild(*args, **kwargs)
    return result, m_file.getvalue()

  def testRevWorkOnEBuild(self):
    """Test Uprev of a single project ebuild."""
    self.createRevWorkOnMocks(self._mock_ebuild, rev=True)
    result, revved_ebuild = self.RevWorkOnEBuild(self.tempdir, MANIFEST)
    self.assertEqual(result[0], 'category/test_package-0.0.1-r2')
    self.assertEqual(self._revved_ebuild, revved_ebuild)
    self.assertExists(self.revved_ebuild_path)

  def testRevUnchangedEBuildSubdirsNoChange(self):
    """Test Uprev of a single-project ebuild with CROS_WORKON_SUBDIRS_TO_REV.

    No files changed in git, so this should not uprev.
    """
    self.createRevWorkOnMocks(self._mock_ebuild_subdir, rev=False)
    self.m_ebuild.cros_workon_vars = portage_util.EBuild.GetCrosWorkonVars(
        self.m_ebuild.ebuild_path, 'test-package')
    result, revved_ebuild = self.RevWorkOnEBuild(self.tempdir, MANIFEST)
    self.assertIsNone(result)
    self.assertEqual('', revved_ebuild)
    self.assertNotExists(self.revved_ebuild_path)

  def testRevUnchangedEBuildSubdirsChange(self):
    """Test Uprev of a single-project ebuild with CROS_WORKON_SUBDIRS_TO_REV.

    The 'foo' directory is changed in git, and this directory is mentioned in
    CROS_WORKON_SUBDIRS_TO_REV, so this should uprev.
    """
    self.git_files_changed = ['foo']
    self.createRevWorkOnMocks(self._mock_ebuild_subdir, rev=True)
    self.m_ebuild.cros_workon_vars = portage_util.EBuild.GetCrosWorkonVars(
        self.m_ebuild.ebuild_path, 'test-package')
    result, revved_ebuild = self.RevWorkOnEBuild(self.tempdir, MANIFEST)
    self.assertEqual(result[0], 'category/test_package-0.0.1-r2')
    self.assertEqual(self._revved_ebuild_subdir, revved_ebuild)
    self.assertExists(self.revved_ebuild_path)

  def testRevChangedEBuildFilesChanged(self):
    """Test Uprev of a single-project ebuild whose files/ content has changed.

    The 'files' directory is changed in git and some other directory is
    mentioned in CROS_WORKON_SUBDIRS_TO_REV. files/ should always force uprev.
    """
    self.git_files_changed = ['files']
    self.createRevWorkOnMocks(self._mock_ebuild_subdir, rev=True)
    self.m_ebuild.cros_workon_vars = portage_util.EBuild.GetCrosWorkonVars(
        self.m_ebuild.ebuild_path, 'test-package')
    result, revved_ebuild = self.RevWorkOnEBuild(self.tempdir, MANIFEST)
    self.assertEqual(result[0], 'category/test_package-0.0.1-r2')
    self.assertEqual(self._revved_ebuild_subdir, revved_ebuild)
    self.assertExists(self.revved_ebuild_path)

  def testRevUnchangedEBuildFilesChanged(self):
    """Test Uprev of a single-project ebuild whose files/ content has changed.

    The 'files' directory is changed in git and some other directory is
    mentioned in CROS_WORKON_SUBDIRS_TO_REV. files/ should always force uprev.
    """
    self.git_files_changed = ['files']
    self.createRevWorkOnMocks(self._mock_ebuild_subdir, rev=False)
    self.m_ebuild.cros_workon_vars = portage_util.EBuild.GetCrosWorkonVars(
        self.m_ebuild.ebuild_path, 'test-package')
    result, revved_ebuild = self.RevWorkOnEBuild(self.tempdir, MANIFEST)
    self.assertEqual(result[0], 'category/test_package-0.0.1-r2')
    self.assertEqual(self._revved_ebuild_subdir, revved_ebuild)
    self.assertExists(self.revved_ebuild_path)

  def testRevUnchangedEBuildOtherSubdirChange(self):
    """Uprev an other subdir with no CROS_WORKON_SUBDIRS_TO_REV.

    The 'other' directory is changed in git, but there is no
    CROS_WORKON_SUBDIRS_TO_REV in the build, so any change causes an uprev.
    """
    self.git_files_changed = ['other']
    self.createRevWorkOnMocks(self._mock_ebuild, rev=True)
    self.m_ebuild.cros_workon_vars = portage_util.EBuild.GetCrosWorkonVars(
        self.m_ebuild.ebuild_path, 'test-package')
    result, revved_ebuild = self.RevWorkOnEBuild(self.tempdir, MANIFEST)
    self.assertEqual(result[0], 'category/test_package-0.0.1-r2')
    self.assertEqual(self._revved_ebuild, revved_ebuild)
    self.assertExists(self.revved_ebuild_path)

  def testNoRevUnchangedEBuildOtherSubdirChange(self):
    """Uprev an other subdir with no CROS_WORKON_SUBDIRS_TO_REV.

    The 'other' directory is changed in git, but CROS_WORKON_SUBDIRS_TO_REV
    is empty , so this should not uprev.
    """
    self.git_files_changed = ['other']
    self.createRevWorkOnMocks(self._mock_ebuild_subdir, rev=False)
    self.m_ebuild.cros_workon_vars = portage_util.EBuild.GetCrosWorkonVars(
        self.m_ebuild.ebuild_path, 'test-package')
    result, revved_ebuild = self.RevWorkOnEBuild(self.tempdir, MANIFEST)
    self.assertEqual(result, None)
    self.assertEqual('', revved_ebuild)
    self.assertNotExists(self.revved_ebuild_path)

  def testRevChangedEBuildNoSubdirChange(self):
    """Uprev a changed ebuild with CROS_WORKON_SUBDIRS_TO_REV.

    Any change to the 9999 ebuild should cause an uprev, even if
    CROS_WORKON_SUBDIRS_TO_REV is set and no files in that list are changed in
    git.
    """
    self.unstable_ebuild_changed = True
    self.createRevWorkOnMocks(self._mock_ebuild_subdir, rev=True)
    self.m_ebuild.cros_workon_vars = portage_util.EBuild.GetCrosWorkonVars(
        self.m_ebuild.ebuild_path, 'test-package')
    result, revved_ebuild = self.RevWorkOnEBuild(self.tempdir, MANIFEST)
    self.assertEqual(result[0], 'category/test_package-0.0.1-r2')
    self.assertEqual(self._revved_ebuild_subdir, revved_ebuild)
    self.assertExists(self.revved_ebuild_path)

  def testRevWorkOnMultiEBuild(self):
    """Test Uprev of a multi-project (array) ebuild."""
    self.createRevWorkOnMocks(self._mock_ebuild_multi, rev=True, multi=True)
    result, revved_ebuild = self.RevWorkOnEBuild(self.tempdir, MANIFEST)
    self.assertEqual(result[0], 'category/test_package-0.0.1-r2')
    self.assertEqual(self._revved_ebuild_multi, revved_ebuild)
    self.assertExists(self.revved_ebuild_path)

  def testRevUnchangedEBuild(self):
    self.createRevWorkOnMocks(self._mock_ebuild, rev=False)

    self.PatchObject(
        portage_util.EBuild, '_AlmostSameEBuilds', return_value=True)
    result, revved_ebuild = self.RevWorkOnEBuild(self.tempdir, MANIFEST)
    self.assertEqual(result, None)
    self.assertEqual(self._revved_ebuild, revved_ebuild)
    self.assertNotExists(self.revved_ebuild_path)

  def testRevMissingEBuild(self):
    self.revved_ebuild_path = self.m_ebuild.ebuild_path
    self.m_ebuild.ebuild_path = self.m_ebuild._unstable_ebuild_path
    self.m_ebuild.current_revision = 0
    self.m_ebuild.is_stable = False

    self.createRevWorkOnMocks(self._mock_ebuild[0:1] + self._mock_ebuild[2:],
                              rev=True)
    result, revved_ebuild = self.RevWorkOnEBuild(self.tempdir, MANIFEST)

    self.assertEqual(result[0], 'category/test_package-0.0.1-r1')
    self.assertEqual(self._revved_ebuild, revved_ebuild)
    self.assertExists(self.revved_ebuild_path)

  def testCommitChange(self):
    m = self.PatchObject(portage_util.EBuild, '_RunGit', return_value='')
    mock_message = 'Commitme'
    self.m_ebuild.CommitChange(mock_message, '.')
    m.assert_called_once_with('.', ['commit', '-a', '-m', 'Commitme'])

  def testGitRepoHasChanges(self):
    """Tests that GitRepoHasChanges works correctly."""
    git.RunGit(self.tempdir,
               ['clone', '--depth=1', constants.CHROMITE_DIR, self.tempdir])
    # No changes yet as we just cloned the repo.
    self.assertFalse(portage_util.EBuild.GitRepoHasChanges(self.tempdir))
    # Update metadata but no real changes.
    osutils.Touch(os.path.join(self.tempdir, 'LICENSE'))
    self.assertFalse(portage_util.EBuild.GitRepoHasChanges(self.tempdir))
    # A real change.
    osutils.WriteFile(os.path.join(self.tempdir, 'LICENSE'), 'hi')
    self.assertTrue(portage_util.EBuild.GitRepoHasChanges(self.tempdir))

  def testNoVersionScript(self):
    """Verify default behavior with no chromeos-version.sh script."""
    self.assertEqual('1234', self.m_ebuild.GetVersion(None, None, '1234'))

  def testValidVersionScript(self):
    """Verify normal behavior with a chromeos-version.sh script."""
    exists = self.PatchObject(os.path, 'exists', return_value=True)
    self.PatchObject(portage_util.EBuild, 'GetSourceInfo',
                     return_value=portage_util.SourceInfo(
                         projects=None, srcdirs=[], subdirs=[], subtrees=[]))
    self.PatchObject(cros_build_lib, 'run',
                     return_value=cros_build_lib.CommandResult(
                         returncode=0, output='1122', error='STDERR'))
    self.assertEqual('1122', self.m_ebuild.GetVersion(None, None, '1234'))
    # Sanity check.
    self.assertEqual(exists.call_count, 1)

  def testVersionScriptNoOutput(self):
    """Reject scripts that output nothing."""
    exists = self.PatchObject(os.path, 'exists', return_value=True)
    self.PatchObject(portage_util.EBuild, 'GetSourceInfo',
                     return_value=portage_util.SourceInfo(
                         projects=None, srcdirs=[], subdirs=[], subtrees=[]))
    run = self.PatchObject(cros_build_lib, 'run')

    # Reject no output.
    run.return_value = cros_build_lib.CommandResult(
        returncode=0, output='', error='STDERR')
    self.assertRaises(portage_util.Error,
                      self.m_ebuild.GetVersion, None, None, '1234')
    # Sanity check.
    self.assertEqual(exists.call_count, 1)
    exists.reset_mock()

    # Reject simple output.
    run.return_value = cros_build_lib.CommandResult(
        returncode=0, output='\n', error='STDERR')
    self.assertRaises(portage_util.Error,
                      self.m_ebuild.GetVersion, None, None, '1234')
    # Sanity check.
    self.assertEqual(exists.call_count, 1)
    exists.reset_mock()

    # Reject error.
    run.return_value = cros_build_lib.CommandResult(
        returncode=1, output='FAIL\n', error='STDERR')
    self.assertRaises(portage_util.Error,
                      self.m_ebuild.GetVersion, None, None, '1234')
    # Sanity check.
    self.assertEqual(exists.call_count, 1)

  def testVersionScriptTooHighVersion(self):
    """Reject scripts that output high version numbers."""
    exists = self.PatchObject(os.path, 'exists', return_value=True)
    self.PatchObject(portage_util.EBuild, 'GetSourceInfo',
                     return_value=portage_util.SourceInfo(
                         projects=None, srcdirs=[], subdirs=[], subtrees=[]))
    self.PatchObject(cros_build_lib, 'run',
                     return_value=cros_build_lib.CommandResult(
                         returncode=0, output='999999', error='STDERR'))
    self.assertRaises(ValueError, self.m_ebuild.GetVersion, None, None, '1234')
    # Sanity check.
    self.assertEqual(exists.call_count, 1)

  def testVersionScriptInvalidVersion(self):
    """Reject scripts that output bad version numbers."""
    exists = self.PatchObject(os.path, 'exists', return_value=True)
    self.PatchObject(portage_util.EBuild, 'GetSourceInfo',
                     return_value=portage_util.SourceInfo(
                         projects=None, srcdirs=[], subdirs=[], subtrees=[]))
    self.PatchObject(cros_build_lib, 'run',
                     return_value=cros_build_lib.CommandResult(
                         returncode=0, output='abcd', error='STDERR'))
    self.assertRaises(ValueError, self.m_ebuild.GetVersion, None, None, '1234')
    # Sanity check.
    self.assertEqual(exists.call_count, 1)

  def testVersionScriptInvalidVersionPostfix(self):
    """Reject scripts that output bad version numbers."""
    exists = self.PatchObject(os.path, 'exists', return_value=True)
    self.PatchObject(
        portage_util.EBuild,
        'GetSourceInfo',
        return_value=portage_util.SourceInfo(
            projects=None, srcdirs=[], subdirs=[], subtrees=[]))
    self.PatchObject(
        cros_build_lib,
        'run',
        return_value=cros_build_lib.CommandResult(
            returncode=0, output='4.4.21_baseline', error='STDERR'))
    with self.assertRaises(portage_util.EbuildVersionError):
      self.m_ebuild.GetVersion(None, None, '1234')
    # Sanity check.
    self.assertEqual(exists.call_count, 1)

  def testUpdateEBuildRecovery(self):
    """Make sure UpdateEBuild can be called more than once even w/failures."""
    ebuild = os.path.join(self.tempdir, 'test.ebuild')
    content = '# Some data\nVAR=val\n'
    osutils.WriteFile(ebuild, content)

    # First run: pass in an invalid redirect file to trigger an exception.
    try:
      portage_util.EBuild.UpdateEBuild(ebuild, {'VAR': 'a'}, redirect_file=1234)
      assert False, 'this should have thrown an exception ...'
    except Exception:
      pass

    # Second run: it should pass normally.
    portage_util.EBuild.UpdateEBuild(ebuild, {'VAR': 'b'})


class ListOverlaysTest(cros_test_lib.TempDirTestCase):
  """Tests related to listing overlays."""

  def testMissingOverlays(self):
    """Tests that exceptions are raised when an overlay is missing."""
    self.assertRaises(portage_util.MissingOverlayError,
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
          'masters': 'portage-stable ',
          'private': '',
          'repo-name': board,
      }
      if '_' in board:
        settings['masters'] += board.split('_')[0]
      osutils.WriteFile(conf_path % settings,
                        conf_data % settings)

    for board in (self.PUB_PRIV, self.PUB_PRIV_VARIANT, self.PRIV_ONLY):
      settings = {
          'board': board,
          'masters': 'portage-stable ',
          'private': 'private-',
          'repo-name': '%s-private' % board,
      }
      if '_' in board:
        settings['masters'] += board.split('_')[0]
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
        except portage_util.MissingOverlayError:
          d[o] = []
    self._no_overlays = not bool(any(d.values()))

  def testDuplicates(self):
    """Verify that no duplicate overlays are returned."""
    for d in self.overlays.values():
      for overlays in d.values():
        self.assertEqual(len(overlays), len(set(overlays)))

  def testOverlaysExist(self):
    """Verify that all overlays returned actually exist on disk."""
    for d in self.overlays.values():
      for overlays in d.values():
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
    self.assertTrue(all(d[None] == [] for d in self.overlays.values()))

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

  def testGetRepositoryFromEbuildInfo(self):
    """Verify GetRepositoryFromEbuildInfo handles data from ebuild info."""

    def _runTestGetRepositoryFromEbuildInfo(fake_projects, fake_srcdirs):
      """Generate the output from ebuild info"""

      # ebuild info always put () around the result, even for single element
      # array.
      fake_ebuild_contents = """
CROS_WORKON_PROJECT=("%s")
CROS_WORKON_SRCDIR=("%s")
      """ % ('" "'.join(fake_projects),
             '" "'.join(fake_srcdirs))
      result = portage_util.GetRepositoryFromEbuildInfo(fake_ebuild_contents)
      result_srcdirs, result_projects = zip(*result)
      self.assertEqual(fake_projects, list(result_projects))
      self.assertEqual(fake_srcdirs, list(result_srcdirs))

    _runTestGetRepositoryFromEbuildInfo(['a', 'b'], ['src_a', 'src_b'])
    _runTestGetRepositoryFromEbuildInfo(['a'], ['src_a'])


class GetOverlayEBuildsTest(cros_test_lib.MockTempDirTestCase):
  """Tests for GetOverlayEBuilds."""

  def setUp(self):
    self.overlay = self.tempdir
    self.uprev_candidate_mock = self.PatchObject(
        portage_util, '_FindUprevCandidates',
        side_effect=GetOverlayEBuildsTest._FindUprevCandidateMock)

  def _CreatePackage(self, name, blacklisted=False):
    """Helper that creates an ebuild."""
    package_path = os.path.join(self.overlay, name,
                                'test_package-0.0.1.ebuild')
    content = 'CROS_WORKON_BLACKLIST=1' if blacklisted else ''
    osutils.WriteFile(package_path, content, makedirs=True)

  @staticmethod
  def _FindUprevCandidateMock(files, allow_blacklisted, _subdir_support):
    """Mock for the FindUprevCandidateMock function.

    Simplified implementation of FindUprevCandidate: consider an ebuild worthy
    of uprev if |allow_blacklisted| is set or the ebuild is not blacklisted.
    """
    for f in files:
      if (f.endswith('.ebuild') and
          (not 'CROS_WORKON_BLACKLIST=1' in osutils.ReadFile(f) or
           allow_blacklisted)):
        pkgdir = os.path.dirname(f)
        return _Package(os.path.join(os.path.basename(os.path.dirname(pkgdir)),
                                     os.path.basename(pkgdir)))
    return None

  def _assertFoundPackages(self, ebuilds, packages):
    """Succeeds iff the packages discovered were packages."""
    self.assertEqual([e.package for e in ebuilds],
                     packages)

  def testWantedPackage(self):
    """Test that we can find a specific package."""
    package_name = 'chromeos-base/mypackage'
    self._CreatePackage(package_name)
    ebuilds = portage_util.GetOverlayEBuilds(
        self.overlay, False, [package_name])
    self._assertFoundPackages(ebuilds, [package_name])

  def testUnwantedPackage(self):
    """Test that we find only the packages we want."""
    ebuilds = portage_util.GetOverlayEBuilds(self.overlay, False, [])
    self._assertFoundPackages(ebuilds, [])

  def testAnyPackage(self):
    """Test that we return all packages available if use_all is set."""
    package_name = 'chromeos-base/package_name'
    self._CreatePackage(package_name)
    ebuilds = portage_util.GetOverlayEBuilds(self.overlay, True, [])
    self._assertFoundPackages(ebuilds, [package_name])

  def testUnknownPackage(self):
    """Test that _FindUprevCandidates is only called if the CP matches."""
    self._CreatePackage('chromeos-base/package_name')
    ebuilds = portage_util.GetOverlayEBuilds(
        self.overlay, False, ['chromeos-base/other_package'])
    self.assertFalse(self.uprev_candidate_mock.called)
    self._assertFoundPackages(ebuilds, [])

  def testBlacklistedPackagesIgnoredByDefault(self):
    """Test that blacklisted packages are ignored by default."""
    package_name = 'chromeos-base/blacklisted_package'
    self._CreatePackage(package_name, blacklisted=True)
    ebuilds = portage_util.GetOverlayEBuilds(
        self.overlay, False, [package_name])
    self._assertFoundPackages(ebuilds, [])

  def testBlacklistedPackagesAllowed(self):
    """Test that we can find blacklisted packages with |allow_blacklisted|."""
    package_name = 'chromeos-base/blacklisted_package'
    self._CreatePackage(package_name, blacklisted=True)
    ebuilds = portage_util.GetOverlayEBuilds(
        self.overlay, False, [package_name], allow_blacklisted=True)
    self._assertFoundPackages(ebuilds, [package_name])


class ProjectMappingTest(cros_test_lib.TestCase):
  """Tests related to Proejct Mapping."""

  def testSplitEbuildPath(self):
    """Test if we can split an ebuild path into its components."""
    ebuild_path = 'chromeos-base/platform2/platform2-9999.ebuild'
    components = ['chromeos-base', 'platform2', 'platform2-9999']
    for path in (ebuild_path, './' + ebuild_path, 'foo.bar/' + ebuild_path):
      self.assertEqual(components, portage_util.SplitEbuildPath(path))

  def testSplitPV(self):
    """Test splitting PVs into package and version components."""
    pv = 'bar-1.2.3_rc1-r5'
    package, version_no_rev, rev = tuple(pv.split('-'))
    split_pv = portage_util.SplitPV(pv)
    self.assertEqual(split_pv.pv, pv)
    self.assertEqual(split_pv.package, package)
    self.assertEqual(split_pv.version_no_rev, version_no_rev)
    self.assertEqual(split_pv.rev, rev)
    self.assertEqual(split_pv.version, '%s-%s' % (version_no_rev, rev))

  def testSplitCPV(self):
    """Test splitting CPV into components."""
    # Test 1: Valid input.
    cpv = 'foo/bar-4.5.6_alpha-r6'
    cp, ver, rev = cpv.split('-')
    cat, pv = cpv.split('/', 1)

    split_pv = portage_util.SplitPV(pv)
    split_cpv = portage_util.SplitCPV(cpv)

    self.assertEqual(split_cpv.category, cat)
    self.assertEqual(cp, split_cpv.cp)
    self.assertEqual('%s-%s' % (cp, ver), split_cpv.cpv)
    self.assertEqual('%s-%s-%s' % (cp, ver, rev), split_cpv.cpf)
    for k, v in split_pv._asdict().items():
      self.assertEqual(getattr(split_cpv, k), v)

    # Test 2: No category, valid pvr.
    cpv = 'bar-pkg-1.2.3_rc4-r5'
    split_cpv_ns = portage_util.SplitCPV(cpv, strict=False)

    # Make sure strict parses correctly.
    self.assertIsNone(portage_util.SplitCPV(cpv, strict=True))
    self.assertIsNotNone(portage_util.SplitPV(cpv, strict=True))
    # All category references should be None when not strict.
    self.assertIsNotNone(split_cpv_ns)
    self.assertIsNone(split_cpv_ns.category)
    self.assertIsNone(split_cpv_ns.cp)
    self.assertIsNone(split_cpv_ns.cpv)
    self.assertIsNone(split_cpv_ns.cpf)

    # Test 3: No version or revision, valid cp.
    cpv = 'cat-foo/bar-pkg'
    split_cpv_ns = portage_util.SplitCPV(cpv, strict=False)

    # Make sure strict parses correctly.
    self.assertIsNone(portage_util.SplitCPV(cpv, strict=True))
    self.assertIsNone(portage_util.SplitPV(cpv, strict=True))
    # Category should get parsed out, and the rest assigned to package.
    self.assertIsNotNone(split_cpv_ns)
    self.assertEqual('cat-foo', split_cpv_ns.category)
    self.assertEqual('bar-pkg', split_cpv_ns.package)
    # The cp should just have category/package. In this case it's valid.
    self.assertEqual('cat-foo/bar-pkg', split_cpv_ns.cp)
    self.assertIsNone(split_cpv_ns.cpv)
    self.assertIsNone(split_cpv_ns.cpf)

    # Test 4: No version - skipped to valid revision.
    cpv = 'cat-foo/bar-pkg-r5'
    split_cpv_ns = portage_util.SplitCPV(cpv, strict=False)

    # Make sure strict parses correctly.
    self.assertIsNone(portage_util.SplitCPV(cpv, strict=True))
    self.assertIsNone(portage_util.SplitPV(cpv, strict=True))
    # Category should get parsed out, and the rest assigned to package.
    self.assertIsNotNone(split_cpv_ns)
    self.assertEqual('cat-foo', split_cpv_ns.category)
    self.assertEqual('bar-pkg-r5', split_cpv_ns.package)
    # The cp should just have category/package. Invalid in this case, but
    # meeting expectations.
    self.assertEqual('cat-foo/bar-pkg-r5', split_cpv_ns.cp)
    self.assertIsNone(split_cpv_ns.cpv)
    self.assertIsNone(split_cpv_ns.cpf)

    cpv = 'invalid/format/package'
    with self.assertRaises(ValueError):
      portage_util.SplitCPV(cpv)

  def testFindWorkonProjects(self):
    """Test if we can find the list of workon projects."""
    frecon = 'sys-apps/frecon'
    frecon_project = 'chromiumos/platform/frecon'
    this = 'chromeos-base/chromite'
    this_project = 'chromiumos/chromite'
    matches = [
        ([frecon], set([frecon_project])),
        ([this], set([this_project])),
        ([frecon, this], set([frecon_project, this_project]))
    ]
    if portage_util.FindOverlays(constants.BOTH_OVERLAYS):
      for packages, projects in matches:
        self.assertEqual(projects,
                         portage_util.FindWorkonProjects(packages))


class PortageDBTest(cros_test_lib.TempDirTestCase):
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
    for cat, pkgs in self.fake_pkgdb.items():
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
    self.assertEqual(self.fake_packages, packages)

  def testGeneratePackageSizes(self):
    """Test if calculating installed package sizes works."""
    fake_data = 'FAKE DATA'
    expected_size = 0
    for fake_file in self.fake_files:
      if fake_file[0] == 'obj':
        fake_filename = os.path.join(self.fake_chroot,
                                     os.path.relpath(fake_file[1], '/'))
        osutils.WriteFile(fake_filename, fake_data, makedirs=True)
        expected_size += len(fake_data)

    portage_db = portage_util.PortageDB(self.fake_chroot)
    installed_packages = portage_db.InstalledPackages()
    package_size_pairs = portage_util.GeneratePackageSizes(portage_db,
                                                           'fake_chroot',
                                                           installed_packages)
    total_size = sum(x for _, x in package_size_pairs)
    self.assertEqual(total_size, expected_size)

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
    self.assertEqual(len(fake_files), len(lst))

    # Check the paths are all relative.
    self.assertTrue(all(not f[1].startswith('/') for f in lst))

    # Check all the files are present. We only consider file type and path, and
    # convert the path to a relative path.
    fake_files = [(f[0], f[1].lstrip('/')) for f in fake_files]
    self.assertEqual(fake_files, lst)


class InstalledPackageTest(cros_test_lib.TempDirTestCase):
  """InstalledPackage class tests outside a PortageDB."""

  def setUp(self):
    osutils.WriteFile(os.path.join(self.tempdir, 'package-1.ebuild'), 'EAPI=1')
    osutils.WriteFile(os.path.join(self.tempdir, 'PF'), 'package-1')
    osutils.WriteFile(os.path.join(self.tempdir, 'CATEGORY'), 'category-1')

  def testOutOfDBPackage(self):
    """Tests an InstalledPackage instance can be created without a PortageDB."""
    pkg = portage_util.InstalledPackage(None, self.tempdir)
    self.assertEqual('package-1', pkg.pf)
    self.assertEqual('category-1', pkg.category)

  def testIncompletePackage(self):
    """Tests an incomplete or otherwise invalid package raises an exception."""
    # No package name is provided.
    os.unlink(os.path.join(self.tempdir, 'PF'))
    self.assertRaises(portage_util.PortageDBError,
                      portage_util.InstalledPackage, None, self.tempdir)

    # Check that doesn't fail when the package name is provided.
    pkg = portage_util.InstalledPackage(None, self.tempdir, pf='package-1')
    self.assertEqual('package-1', pkg.pf)


class HasPrebuiltTest(cros_test_lib.RunCommandTestCase):
  """HasPrebuilt tests."""

  def setUp(self):
    self.atom = 'chromeos-base/chromeos-chrome'
    self.r1_cpf = 'chromeos-base/chromeos-chrome-78.0.3900.0_rc-r1'
    self.r2_cpf = 'chromeos-base/chromeos-chrome-78.0.3900.0_rc-r2'
    self.r1_cpv = portage_util.SplitCPV(self.r1_cpf)
    self.r2_cpv = portage_util.SplitCPV(self.r2_cpf)

    def _check_pkg(cmd, *_args, **_kwargs):
      rc = 0 if '=%s' % self.r1_cpf in cmd else 1
      return cros_build_lib.CommandResult(cmd=cmd, returncode=rc)
    self.rc.SetDefaultCmdResult(side_effect=_check_pkg)

  def testHasPrebuilt(self):
    """Test a package with a matching prebuilt."""
    self.PatchObject(portage_util, 'PortageqBestVisible',
                     return_value=self.r1_cpv)
    self.rc.SetDefaultCmdResult(returncode=0)

    self.assertTrue(portage_util.HasPrebuilt(self.atom))
    self.rc.assertCommandContains(('=%s' % self.r1_cpf,))

  def testNoPrebuilt(self):
    """Test a package without a matching prebuilt."""
    self.PatchObject(portage_util, 'PortageqBestVisible',
                     return_value=self.r2_cpv)
    self.rc.SetDefaultCmdResult(returncode=1)

    self.assertFalse(portage_util.HasPrebuilt(self.atom))
    self.rc.assertCommandContains(('=%s' % self.r2_cpf,))


class PortageqBestVisibleTest(cros_test_lib.MockTestCase):
  """PortageqBestVisible tests."""

  def testValidPackage(self):
    """Test valid outputs."""
    result = cros_build_lib.CommandResult(output='cat/pkg-1.0', returncode=0)
    self.PatchObject(portage_util, '_Portageq', return_value=result)

    self.assertIsInstance(portage_util.PortageqBestVisible('cat/pkg'),
                          portage_util.CPV)


class PortageqEnvvarTest(cros_test_lib.MockTestCase):
  """PortageqEnvvar[s] tests."""

  def testValidEnvvar(self):
    """Test valid variables."""
    result = cros_build_lib.CommandResult(output='TEST=value\n', returncode=0)
    self.PatchObject(portage_util, '_Portageq', return_value=result)

    envvar1 = portage_util.PortageqEnvvar('TEST')
    envvars1 = portage_util.PortageqEnvvars(['TEST'])

    self.assertEqual('value', envvar1)
    self.assertEqual(envvar1, envvars1['TEST'])

  def testUndefinedEnvvars(self):
    """Test undefined variable handling."""
    # The variable exists in the command output even when not actually defined.
    result = cros_build_lib.CommandResult(output='DOES_NOT_EXIST=\n',
                                          returncode=1)
    success_error = cros_build_lib.RunCommandError('', result)
    self.PatchObject(portage_util, '_Portageq', side_effect=success_error)

    # Test ignoring error when just from undefined variable.
    envv = portage_util.PortageqEnvvar('DOES_NOT_EXIST', allow_undefined=True)
    envvs = portage_util.PortageqEnvvars(['DOES_NOT_EXIST'],
                                         allow_undefined=True)
    self.assertEqual('', envv)
    self.assertEqual(envv, envvs['DOES_NOT_EXIST'])

    # Test raising the error when just from undefined variable.
    with self.assertRaises(portage_util.PortageqError):
      portage_util.PortageqEnvvar('DOES_NOT_EXIST')
    with self.assertRaises(portage_util.PortageqError):
      portage_util.PortageqEnvvars(['DOES_NOT_EXIST'])

    run_error = cros_build_lib.CommandResult(output='\n', returncode=2)
    failure_error = cros_build_lib.RunCommandError('', run_error)
    self.PatchObject(portage_util, '_Portageq', side_effect=failure_error)

    # Test re-raising the error when the command did not run successfully.
    with self.assertRaises(cros_build_lib.RunCommandError):
      portage_util.PortageqEnvvar('DOES_NOT_EXIST')
    with self.assertRaises(cros_build_lib.RunCommandError):
      portage_util.PortageqEnvvars(['DOES_NOT_EXIST'])

  def testInvalidEnvvars(self):
    """Test invalid variables handling."""
    # Envvar tests.
    with self.assertRaises(TypeError):
      portage_util.PortageqEnvvar([])
    with self.assertRaises(ValueError):
      portage_util.PortageqEnvvar('')

    # Envvars tests.
    self.assertEqual({}, portage_util.PortageqEnvvars([]))
    with self.assertRaises(TypeError):
      portage_util.PortageqEnvvars('')

    # Raised when extending the command list. This is currently expected, and
    # ints should not be accepted, but more formal handling can be added.
    with self.assertRaises(TypeError):
      portage_util.PortageqEnvvars(1)


class PortageqHasVersionTest(cros_test_lib.MockTestCase):
  """PortageqHasVersion tests."""

  def testPortageqHasVersion(self):
    """Test HasVersion."""
    result_true = cros_build_lib.CommandResult(returncode=0)
    result_false = cros_build_lib.CommandResult(returncode=1)
    result_error = cros_build_lib.CommandResult(returncode=2)

    # Test has version.
    self.PatchObject(portage_util, '_Portageq', return_value=result_true)
    self.assertTrue(portage_util.PortageqHasVersion('cat/pkg'))

    # Test not has version.
    self.PatchObject(portage_util, '_Portageq', return_value=result_false)
    self.assertFalse(portage_util.PortageqHasVersion('cat/pkg'))

    # Test run error.
    self.PatchObject(portage_util, '_Portageq', return_value=result_error)
    self.assertFalse(portage_util.PortageqHasVersion('cat/pkg'))


class PortageqMatchTest(cros_test_lib.MockTestCase):
  """PortageqMatch tests."""

  def testMultiError(self):
    """Test unspecific query results in error.

    The method currently isn't setup to support multiple values in the output.
    It is instead interpreted as a cpv format error by SplitCPV. This isn't
    a hard requirement, just the current expected behavior.
    """
    output_str = 'cat-1/pkg-one-1.0\ncat-2/pkg-two-2.1.3-r45\n'
    result = cros_build_lib.CommandResult(returncode=0,
                                          output=output_str)
    self.PatchObject(portage_util, '_Portageq', return_value=result)

    with self.assertRaises(ValueError):
      portage_util.PortageqMatch('*/*')

  def testValidPackage(self):
    """Test valid package produces a CPV."""
    result = cros_build_lib.CommandResult(returncode=0, output='cat/pkg-1.0-r1')
    self.PatchObject(portage_util, '_Portageq', return_value=result)

    self.assertIsInstance(portage_util.PortageqMatch('cat/pkg'),
                          portage_util.CPV)


class FindEbuildTest(cros_test_lib.RunCommandTestCase):
  """Tests for FindEbuildsForPackages and FindEbuildsForPackages."""

  def testFindEbuildsForPackagesReturnResultsSimple(self):
    equery_output = ('/chromeos-overlay/misc/foo/foo.ebuild\n'
                     '/chromeos-overlay/misc/bar/bar.ebuild\n')
    self.rc.AddCmdResult(
        ['/build/nami/build/bin/equery', 'which', 'misc/foo', 'misc/bar'],
        output=equery_output)
    self.assertEqual(
        portage_util.FindEbuildsForPackages(['misc/foo', 'misc/bar'],
                                            sysroot='/build/nami'),
        {'misc/bar': '/chromeos-overlay/misc/bar/bar.ebuild',
         'misc/foo': '/chromeos-overlay/misc/foo/foo.ebuild'})

  def testFindEbuildsForPackagesWithoutCategoryReturnResults(self):
    equery_output = ('/chromeos-overlay/misc/foo/foo.ebuild\n'
                     '/chromeos-overlay/misc/bar/bar.ebuild\n')
    self.rc.AddCmdResult(
        ['/build/nami/build/bin/equery', 'which', 'misc/foo', 'bar'],
        output=equery_output)
    self.assertEqual(
        portage_util.FindEbuildsForPackages(['misc/foo', 'bar'],
                                            sysroot='/build/nami'),
        {'bar': '/chromeos-overlay/misc/bar/bar.ebuild',
         'misc/foo': '/chromeos-overlay/misc/foo/foo.ebuild'})

  def testFindEbuildsForPackagesReturnResultsComplexPackages(self):
    ebuild_path = (
        '/portage-stable/sys-libs/timezone-data/timezone-data-2018i.ebuild')
    equery_output = '\n'.join([ebuild_path] * 4)
    packages = [
        # CATEGORY/PN
        'sys-libs/timezone-data',
        # CATEGORY/P
        'sys-libs/timezone-data-2018i',
        # CATEGORY/PN:SLOT
        'sys-libs/timezone-data:0',
        # CATEGORY/P:SLOT
        'sys-libs/timezone-data-2018i:0',
    ]
    self.rc.AddCmdResult(
        ['/build/nami/build/bin/equery', 'which'] + packages,
        output=equery_output)
    self.assertEqual(
        portage_util.FindEbuildsForPackages(packages,
                                            sysroot='/build/nami'),
        {'sys-libs/timezone-data': ebuild_path,
         'sys-libs/timezone-data-2018i:0': ebuild_path,
         'sys-libs/timezone-data:0': ebuild_path,
         'sys-libs/timezone-data-2018i': ebuild_path,
        })

  def testFindEbuildsForPackagesReturnNone(self):
    # Result for package 'bar' is missing.
    equery_output = ('/chromeos-overlay/bar/bar.ebuild\n')
    self.rc.AddCmdResult(
        ['/build/nami/build/bin/equery', 'which', 'foo', 'bar'],
        output=equery_output, returncode=1)
    self.assertEqual(portage_util.FindEbuildsForPackages(
        ['foo', 'bar'], sysroot='/build/nami'), {})

  def testFindEbuildsForPackagesInvalidEbuildsOrder(self):
    equery_output = ('/chromeos-overlay/bar/bar.ebuild\n'
                     '/chromeos-overlay/foo/foo.ebuild\n')
    self.rc.AddCmdResult(
        ['/build/nami/build/bin/equery', 'which', 'foo', 'bar'],
        output=equery_output)
    with self.assertRaises(AssertionError):
      portage_util.FindEbuildsForPackages(
          ['foo', 'bar'], sysroot='/build/nami')

  def testFindEbuildForPackageReturnResults(self):
    equery_output = '/chromeos-overlay/misc/foo/foo-9999.ebuild\n'
    self.rc.AddCmdResult(
        ['/build/nami/build/bin/equery', 'which', 'misc/foo'],
        output=equery_output)
    self.assertEqual(
        portage_util.FindEbuildForPackage('misc/foo', sysroot='/build/nami'),
        '/chromeos-overlay/misc/foo/foo-9999.ebuild')

  def testFindEbuildForPackageReturnNone(self):
    equery_output = "Cannot find ebuild for package 'foo'\n"
    self.rc.AddCmdResult(
        ['/build/nami/build/bin/equery', 'which', 'foo'],
        output=equery_output, returncode=1)
    self.assertEqual(portage_util.FindEbuildForPackage(
        'foo', sysroot='/build/nami'), None)
