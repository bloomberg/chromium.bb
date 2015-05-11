# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test the path_util module."""

from __future__ import print_function

import itertools
import mock
import os
import tempfile

from chromite.cbuildbot import constants
from chromite.lib import bootstrap_lib
from chromite.lib import cros_build_lib_unittest
from chromite.lib import cros_test_lib
from chromite.lib import git
from chromite.lib import partial_mock
from chromite.lib import path_util
from chromite.lib import workspace_lib


FAKE_SOURCE_PATH = '/path/to/source/tree'
FAKE_REPO_PATH = '/path/to/repo'
FAKE_WORKSPACE_NESTED_CHROOT_DIR = '.fake_chroot'
FAKE_WORKSPACE_STANDALONE_CHROOT_PATH = '/path/to/chroot'
CUSTOM_SOURCE_PATH = '/custom/source/path'


class DetermineCheckoutTest(cros_test_lib.MockTempDirTestCase):
  """Verify functionality for figuring out what checkout we're in."""

  def setUp(self):
    self.rc_mock = cros_build_lib_unittest.RunCommandMock()
    self.StartPatcher(self.rc_mock)
    self.rc_mock.SetDefaultCmdResult()

  def RunTest(self, dir_struct, cwd, expected_root, expected_type,
              expected_src):
    """Run a test with specific parameters and expected results."""
    cros_test_lib.CreateOnDiskHierarchy(self.tempdir, dir_struct)
    cwd = os.path.join(self.tempdir, cwd)
    checkout_info = path_util.DetermineCheckout(cwd)
    full_root = expected_root
    if expected_root is not None:
      full_root = os.path.join(self.tempdir, expected_root)
    full_src = expected_src
    if expected_src is not None:
      full_src = os.path.join(self.tempdir, expected_src)

    self.assertEquals(checkout_info.root, full_root)
    self.assertEquals(checkout_info.type, expected_type)
    self.assertEquals(checkout_info.chrome_src_dir, full_src)

  def testGclientRepo(self):
    """Recognizes a GClient repo checkout."""
    dir_struct = [
        'a/.gclient',
        'a/b/.repo/',
        'a/b/c/.gclient',
        'a/b/c/d/somefile',
    ]
    self.RunTest(dir_struct, 'a/b/c', 'a/b/c',
                 path_util.CHECKOUT_TYPE_GCLIENT,
                 'a/b/c/src')
    self.RunTest(dir_struct, 'a/b/c/d', 'a/b/c',
                 path_util.CHECKOUT_TYPE_GCLIENT,
                 'a/b/c/src')
    self.RunTest(dir_struct, 'a/b', 'a/b',
                 path_util.CHECKOUT_TYPE_REPO,
                 None)
    self.RunTest(dir_struct, 'a', 'a',
                 path_util.CHECKOUT_TYPE_GCLIENT,
                 'a/src')

  def testGitUnderGclient(self):
    """Recognizes a chrome git checkout by gclient."""
    self.rc_mock.AddCmdResult(
        partial_mock.In('config'), output=constants.CHROMIUM_GOB_URL)
    dir_struct = [
        'a/.gclient',
        'a/src/.git/',
    ]
    self.RunTest(dir_struct, 'a/src', 'a',
                 path_util.CHECKOUT_TYPE_GCLIENT,
                 'a/src')

  def testGitUnderRepo(self):
    """Recognizes a chrome git checkout by repo."""
    self.rc_mock.AddCmdResult(
        partial_mock.In('config'), output=constants.CHROMIUM_GOB_URL)
    dir_struct = [
        'a/.repo/',
        'a/b/.git/',
    ]
    self.RunTest(dir_struct, 'a/b', 'a',
                 path_util.CHECKOUT_TYPE_REPO,
                 None)

  def testBadGit1(self):
    """.git is not a directory."""
    self.RunTest(['a/.git'], 'a', None,
                 path_util.CHECKOUT_TYPE_UNKNOWN, None)

  def testBadGit2(self):
    """'git config' returns nothing."""
    self.RunTest(['a/.repo/', 'a/b/.git/'], 'a/b', 'a',
                 path_util.CHECKOUT_TYPE_REPO, None)

  def testBadGit3(self):
    """'git config' returns error."""
    self.rc_mock.AddCmdResult(partial_mock.In('config'), returncode=5)
    self.RunTest(['a/.git/'], 'a', None,
                 path_util.CHECKOUT_TYPE_UNKNOWN, None)

  def testSdkBootstrap(self):
    """Recognizes an SDK bootstrap case."""
    self.rc_mock.AddCmdResult(
        partial_mock.In('config'), output=constants.CHROMITE_URL)
    dir_struct = [
        'a/.git/',
        'a/sdk_checkouts/1.0.0/.repo',
        'a/sdk_checkouts/1.0.0/chromite/.git',
    ]
    self.RunTest(dir_struct, 'a', 'a',
                 path_util.CHECKOUT_TYPE_SDK_BOOTSTRAP, None)
    self.RunTest(dir_struct, 'a/b', 'a',
                 path_util.CHECKOUT_TYPE_SDK_BOOTSTRAP, None)
    self.RunTest(dir_struct, 'a/sdk_checkouts', 'a',
                 path_util.CHECKOUT_TYPE_SDK_BOOTSTRAP, None)
    self.RunTest(dir_struct, 'a/sdk_checkouts/1.0.0', 'a',
                 path_util.CHECKOUT_TYPE_SDK_BOOTSTRAP, None)
    self.RunTest(dir_struct, 'a/sdk_checkouts/1.0.0/chromite', 'a',
                 path_util.CHECKOUT_TYPE_SDK_BOOTSTRAP, None)


class FindCacheDirTest(cros_test_lib.WorkspaceTestCase):
  """Test cache dir specification and finding functionality."""

  def setUp(self):
    dir_struct = [
        'repo/.repo/',
        'repo/manifest/',
        'gclient/.gclient',
    ]
    cros_test_lib.CreateOnDiskHierarchy(self.tempdir, dir_struct)
    self.repo_root = os.path.join(self.tempdir, 'repo')
    self.gclient_root = os.path.join(self.tempdir, 'gclient')
    self.nocheckout_root = os.path.join(self.tempdir, 'nothing')
    self.CreateBootstrap('1.0.0')
    self.CreateWorkspace()
    self.bootstrap_cache = os.path.join(
        self.bootstrap_path, bootstrap_lib.SDK_CHECKOUTS,
        path_util.GENERAL_CACHE_DIR)

    self.rc_mock = self.StartPatcher(cros_build_lib_unittest.RunCommandMock())
    self.cwd_mock = self.PatchObject(os, 'getcwd')

  def testRepoRoot(self):
    """Test when we are inside a repo checkout."""
    self.cwd_mock.return_value = self.repo_root
    self.assertEquals(
        path_util.FindCacheDir(),
        os.path.join(self.repo_root, path_util.GENERAL_CACHE_DIR))

  def testGclientRoot(self):
    """Test when we are inside a gclient checkout."""
    self.cwd_mock.return_value = self.gclient_root
    self.assertEquals(
        path_util.FindCacheDir(),
        os.path.join(self.gclient_root, path_util.CHROME_CACHE_DIR))

  def testTempdir(self):
    """Test when we are not in any checkout."""
    self.cwd_mock.return_value = self.nocheckout_root
    self.assertStartsWith(
        path_util.FindCacheDir(),
        os.path.join(tempfile.gettempdir(), ''))

  def testBootstrap(self):
    """Test when running from bootstrap."""
    self.cwd_mock.return_value = self.bootstrap_path
    self.rc_mock.AddCmdResult(
        partial_mock.In('config'), output=constants.CHROMITE_URL)
    self.assertEquals(
        path_util.FindCacheDir(),
        self.bootstrap_cache)

  def testSdkCheckoutsInsideBootstrap(self):
    """Test when in the bootstrap SDK checkout location."""
    self.cwd_mock.return_value = os.path.join(
        self.bootstrap_path, bootstrap_lib.SDK_CHECKOUTS)
    self.rc_mock.AddCmdResult(
        partial_mock.In('config'), output=constants.CHROMITE_URL)
    self.assertEquals(
        path_util.FindCacheDir(),
        self.bootstrap_cache)

  def testSdkInsideBootstrap(self):
    """Test when in an SDK checkout inside the bootstrap."""
    self.cwd_mock.return_value = os.path.join(
        self.bootstrap_path, bootstrap_lib.SDK_CHECKOUTS, '1.0.0', 'chromite')
    self.rc_mock.AddCmdResult(
        partial_mock.In('config'), output=constants.CHROMITE_URL)
    self.assertEquals(
        path_util.FindCacheDir(),
        self.bootstrap_cache)

  def testWorkspaceWithBootstrap(self):
    """In a workspace, with boostrap path."""
    self.cwd_mock.return_value = self.workspace_path
    self.assertEquals(
        path_util.FindCacheDir(),
        self.bootstrap_cache)

  def testWorkspaceWithoutBootstrap(self):
    """In a workspace, no bootstrap path."""
    self.cwd_mock.return_value = self.workspace_path
    self.mock_bootstrap_path.return_value = None
    self.assertStartsWith(
        path_util.FindCacheDir(),
        os.path.join(tempfile.gettempdir(), ''))


class TestPathResolver(cros_test_lib.MockTestCase):
  """Tests of ChrootPathResolver class."""

  def setUp(self):
    self.PatchObject(constants, 'SOURCE_ROOT', new=FAKE_SOURCE_PATH)
    self.PatchObject(path_util, 'GetCacheDir', return_value='/path/to/cache')
    self.PatchObject(git, 'FindRepoDir',
                     return_value=os.path.join(FAKE_REPO_PATH, '.fake_repo'))
    self.chroot_path = None

  def FakeCwd(self, base_path):
    return os.path.join(base_path, 'somewhere/in/there')

  def SetChrootPath(self, workspace_path, source_path, standalone_chroot):
    """Set and fake the chroot path."""
    if workspace_path is None:
      self.chroot_path = os.path.join(source_path,
                                      constants.DEFAULT_CHROOT_DIR)
    else:
      if standalone_chroot:
        self.chroot_path = FAKE_WORKSPACE_STANDALONE_CHROOT_PATH
      else:
        self.chroot_path = os.path.join(workspace_path,
                                        FAKE_WORKSPACE_NESTED_CHROOT_DIR)

      self.PatchObject(workspace_lib, 'ChrootPath',
                       return_value=self.chroot_path)

  def RunInsideChrootTests(self, workspace_path, standalone_chroot=False):
    """Tests {To,From}Chroot() calls from inside the chroot."""
    self.SetChrootPath(workspace_path, constants.SOURCE_ROOT,
                       standalone_chroot)
    resolver = path_util.ChrootPathResolver(workspace_path=workspace_path)

    # Paths remain the same, only expanded/canonicalized (both directions).
    self.assertEqual(os.path.realpath('some/path'),
                     resolver.ToChroot('some/path'))
    self.assertEqual(os.path.realpath('/some/path'),
                     resolver.ToChroot('/some/path'))
    self.assertEqual(os.path.realpath('some/path'),
                     resolver.FromChroot('some/path'))
    self.assertEqual(os.path.realpath('/some/path'),
                     resolver.FromChroot('/some/path'))

  @mock.patch('chromite.lib.cros_build_lib.IsInsideChroot', return_value=True)
  def testInsideChrootNoWorkspace(self, _):
    """Test conversion to chroot: inside, no workspace."""
    self.RunInsideChrootTests(None)

  @mock.patch('chromite.lib.cros_build_lib.IsInsideChroot', return_value=True)
  def testInsideChrootWithWorkspaceNestedChroot(self, _):
    """Test conversion to chroot: inside, workspace, nested chroot."""
    self.RunInsideChrootTests('path/to/workspace')

  @mock.patch('chromite.lib.cros_build_lib.IsInsideChroot', return_value=True)
  def testInsideChrootWithWorkspaceStandaloneChroot(self, _):
    """Test convertion to chroot: inside, workspace, standalone chroot."""
    self.RunInsideChrootTests('path/to/workspace', True)

  def RunOutsideToChrootTests(self, workspace_path, standalone_chroot):
    """Tests ToChroot() calls from outside the chroot."""
    for source_path, source_from_path_repo in itertools.product(
        (None, CUSTOM_SOURCE_PATH), (False, True)):
      if source_from_path_repo:
        actual_source_path = FAKE_REPO_PATH
      else:
        actual_source_path = source_path or constants.SOURCE_ROOT

      fake_cwd = self.FakeCwd(actual_source_path)
      self.PatchObject(os, 'getcwd', return_value=fake_cwd)
      self.SetChrootPath(workspace_path, actual_source_path, standalone_chroot)
      resolver = path_util.ChrootPathResolver(
          workspace_path=workspace_path,
          source_path=source_path,
          source_from_path_repo=source_from_path_repo)
      source_rel_cwd = os.path.relpath(fake_cwd, actual_source_path)

      # Case: path inside the chroot space.
      self.assertEqual(
          '/some/path',
          resolver.ToChroot(os.path.join(self.chroot_path, 'some/path')))

      # Case: path inside the workspace.
      if workspace_path is not None:
        self.assertEqual(
            os.path.join(constants.CHROOT_WORKSPACE_ROOT, 'some/path'),
            resolver.ToChroot(os.path.join(workspace_path, 'some/path')))

      # Case: path inside the cache directory.
      self.assertEqual(
          os.path.join(constants.CHROOT_CACHE_ROOT, 'some/path'),
          resolver.ToChroot(os.path.join(path_util.GetCacheDir(),
                                         'some/path')))

      # Case: absolute path inside the source tree.
      if source_from_path_repo:
        self.assertEqual(
            os.path.join(constants.CHROOT_SOURCE_ROOT, 'some/path'),
            resolver.ToChroot(os.path.join(FAKE_REPO_PATH, 'some/path')))
      else:
        self.assertEqual(
            os.path.join(constants.CHROOT_SOURCE_ROOT, 'some/path'),
            resolver.ToChroot(os.path.join(actual_source_path, 'some/path')))

      # Case: relative path inside the source tree.
      if source_from_path_repo:
        self.assertEqual(
            os.path.join(constants.CHROOT_SOURCE_ROOT, source_rel_cwd,
                         'some/path'),
            resolver.ToChroot('some/path'))
      else:
        self.assertEqual(
            os.path.join(constants.CHROOT_SOURCE_ROOT, source_rel_cwd,
                         'some/path'),
            resolver.ToChroot('some/path'))

      # Case: unreachable, path with improper source root prefix.
      with self.assertRaises(ValueError):
        resolver.ToChroot(os.path.join(actual_source_path + '-foo',
                                       'some/path'))

      # Case: unreachable (random).
      with self.assertRaises(ValueError):
        resolver.ToChroot('/some/path')

  @mock.patch('chromite.lib.cros_build_lib.IsInsideChroot', return_value=False)
  def testOutsideToChrootNoWorkspace(self, _):
    """Test inbound translation w/o workspace."""
    self.RunOutsideToChrootTests(None, False)

  @mock.patch('chromite.lib.cros_build_lib.IsInsideChroot', return_value=False)
  def testOutsideToChrootStandaloneWorkspaceNestedChroot(self, _):
    """Test inbound translation w/ standalone workspace + nested chroot."""
    self.RunOutsideToChrootTests('/path/to/workspace', False)

  @mock.patch('chromite.lib.cros_build_lib.IsInsideChroot', return_value=False)
  def testOutsideToChrootStandaloneWorkspaceStandaloneChroot(self, _):
    """Test inbound translation w/ standalone workspace + standalone chroot."""
    self.RunOutsideToChrootTests('/path/to/workspace', True)

  @mock.patch('chromite.lib.cros_build_lib.IsInsideChroot', return_value=False)
  def testOutsideToChrootStandaloneRelativeWorkspaceNestedChroot(self, _):
    """Test inbound w/ standalone realtive workspace + nested chroot."""
    self.RunOutsideToChrootTests(
        os.path.join(constants.SOURCE_ROOT, '../path/to/workspace'),
        False)

  @mock.patch('chromite.lib.cros_build_lib.IsInsideChroot', return_value=False)
  def testOutsideToChrootStandaloneRelativeWorkspaceStandaloneChroot(self, _):
    """Test inbound w/ standalone realtive workspace + standalone chroot."""
    self.RunOutsideToChrootTests(
        os.path.join(constants.SOURCE_ROOT, '../path/to/workspace'),
        True)

  @mock.patch('chromite.lib.cros_build_lib.IsInsideChroot', return_value=False)
  def testOutsideToChrootNestedWorkspaceNestedChroot(self, _):
    """Test inbound translation w/ nested workspace + nested chroot."""
    self.RunOutsideToChrootTests(
        os.path.join(constants.SOURCE_ROOT, 'path/to/workspace'),
        False)

  @mock.patch('chromite.lib.cros_build_lib.IsInsideChroot', return_value=False)
  def testOutsideToChrootNestedWorkspaceStandaloneChroot(self, _):
    """Test inbound translation w/ nested workspace + standalone chroot."""
    self.RunOutsideToChrootTests(
        os.path.join(constants.SOURCE_ROOT, 'path/to/workspace'),
        True)

  @mock.patch('chromite.lib.cros_build_lib.IsInsideChroot', return_value=False)
  def testOutsideToChrootNestedRelativeWorkspaceNestedChroot(self, _):
    """Test inbound translation w/ nested realtive workspace + nested chroot."""
    self.RunOutsideToChrootTests('path/to/workspace', False)

  @mock.patch('chromite.lib.cros_build_lib.IsInsideChroot', return_value=False)
  def testOutsideToChrootNestedRelativeWorkspaceStandaloneChroot(self, _):
    """Test inbound w/ nested realtive workspace + standalone chroot."""
    self.RunOutsideToChrootTests('path/to/workspace', True)

  def RunOutsideFromChrootTests(self, workspace_path, standalone_chroot):
    """Tests FromChroot() calls from outside the chroot."""
    self.PatchObject(os, 'getcwd', return_value=self.FakeCwd(FAKE_SOURCE_PATH))
    self.SetChrootPath(workspace_path, constants.SOURCE_ROOT, standalone_chroot)
    resolver = path_util.ChrootPathResolver(workspace_path=workspace_path)

    # Case: source root path.
    self.assertEqual(
        os.path.join(constants.SOURCE_ROOT, 'some/path'),
        resolver.FromChroot(os.path.join(constants.CHROOT_SOURCE_ROOT,
                                         'some/path')))

    if workspace_path is None:
      # Case: no workspace path, workspace root resolution fails.
      with self.assertRaises(ValueError):
        resolver.FromChroot(os.path.join(constants.CHROOT_WORKSPACE_ROOT,
                                         'some/path'))

      # Case: cyclic source/chroot sub-path elimination.
      self.assertEqual(
          os.path.join(constants.SOURCE_ROOT, 'some/path'),
          resolver.FromChroot(os.path.join(
              constants.CHROOT_SOURCE_ROOT,
              constants.DEFAULT_CHROOT_DIR,
              constants.CHROOT_SOURCE_ROOT.lstrip(os.path.sep),
              constants.DEFAULT_CHROOT_DIR,
              constants.CHROOT_SOURCE_ROOT.lstrip(os.path.sep),
              'some/path')))
    else:
      # Case: workspace root path.
      self.assertEqual(
          os.path.join(workspace_path, 'some/path'),
          resolver.FromChroot(os.path.join(constants.CHROOT_WORKSPACE_ROOT,
                                           'some/path')))

      if not standalone_chroot:
        # Case: cyclic workspace/chroot sub-path elimination.
        self.assertEqual(
            os.path.join(workspace_path, 'some/path'),
            resolver.FromChroot(os.path.join(
                constants.CHROOT_WORKSPACE_ROOT,
                FAKE_WORKSPACE_NESTED_CHROOT_DIR,
                constants.CHROOT_WORKSPACE_ROOT.lstrip(os.path.sep),
                FAKE_WORKSPACE_NESTED_CHROOT_DIR,
                constants.CHROOT_WORKSPACE_ROOT.lstrip(os.path.sep),
                'some/path')))

    # Case: path inside the cache directory.
    self.assertEqual(
        os.path.join(path_util.GetCacheDir(), 'some/path'),
        resolver.FromChroot(os.path.join(constants.CHROOT_CACHE_ROOT,
                                         'some/path')))

    # Case: non-rooted chroot paths.
    self.assertEqual(
        os.path.join(self.chroot_path, 'some/path'),
        resolver.FromChroot('/some/path'))

  @mock.patch('chromite.lib.cros_build_lib.IsInsideChroot', return_value=False)
  def testOutsideFromChrootNoWorkspace(self, _):
    """Test inbound translation w/o workspace."""
    self.RunOutsideFromChrootTests(None, False)

  @mock.patch('chromite.lib.cros_build_lib.IsInsideChroot', return_value=False)
  def testOutsideFromChrootStandaloneWorkspaceNestedChroot(self, _):
    """Test inbound translation w/ standalone workspace + nested chroot."""
    self.RunOutsideFromChrootTests('/path/to/workspace', False)

  @mock.patch('chromite.lib.cros_build_lib.IsInsideChroot', return_value=False)
  def testOutsideFromChrootStandaloneWorkspaceStandaloneChroot(self, _):
    """Test inbound translation w/ standalone workspace + standalone chroot."""
    self.RunOutsideFromChrootTests('/path/to/workspace', True)

  @mock.patch('chromite.lib.cros_build_lib.IsInsideChroot', return_value=False)
  def testOutsideFromChrootNestedWorkspaceNestedChroot(self, _):
    """Test inbound translation w/ nested workspace + nested chroot."""
    self.RunOutsideFromChrootTests(
        os.path.join(constants.SOURCE_ROOT, 'path/to/workspace'),
        False)

  @mock.patch('chromite.lib.cros_build_lib.IsInsideChroot', return_value=False)
  def testOutsideFromChrootNestedWorkspaceStandaloneChroot(self, _):
    """Test inbound translation w/ nested workspace + standalone chroot."""
    self.RunOutsideFromChrootTests(
        os.path.join(constants.SOURCE_ROOT, 'path/to/workspace'),
        True)
