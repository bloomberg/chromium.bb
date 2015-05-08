# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test the path_util module."""

from __future__ import print_function

import os
import tempfile

from chromite.cbuildbot import constants
from chromite.lib import bootstrap_lib
from chromite.lib import cros_build_lib_unittest
from chromite.lib import cros_test_lib
from chromite.lib import partial_mock
from chromite.lib import path_util


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

  def testGitSubmodule(self):
    """Recognizes a chrome git submodule checkout."""
    self.rc_mock.AddCmdResult(
        partial_mock.In('config'), output=constants.CHROMIUM_GOB_URL)
    dir_struct = [
        'a/.gclient',
        'a/.repo',
        'a/b/.git/',
    ]
    self.RunTest(dir_struct, 'a/b', 'a/b',
                 path_util.CHECKOUT_TYPE_SUBMODULE,
                 'a/b')

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
        'submodule/.git/',
    ]
    cros_test_lib.CreateOnDiskHierarchy(self.tempdir, dir_struct)
    self.repo_root = os.path.join(self.tempdir, 'repo')
    self.gclient_root = os.path.join(self.tempdir, 'gclient')
    self.submodule_root = os.path.join(self.tempdir, 'submodule')
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

  def testSubmoduleRoot(self):
    """Test when we are inside a git submodule Chrome checkout."""
    self.cwd_mock.return_value = self.submodule_root
    self.rc_mock.AddCmdResult(
        partial_mock.In('config'), output=constants.CHROMIUM_GOB_URL)
    self.assertEquals(
        path_util.FindCacheDir(),
        os.path.join(self.submodule_root, path_util.CHROME_CACHE_DIR))

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
