# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for chromite.lib.repo and helpers for testing that module."""

from __future__ import print_function

import mock
import os

from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import repo_util


def RepoInitSideEffects(*_args, **kwargs):
  """Mimic side effects of `repo init` by creating .repo dir."""
  os.mkdir(os.path.join(kwargs['cwd'], '.repo'))


def RepoCmdPath(repo_root):
  """Return the path to the repo command to use for the given repo root."""
  return os.path.join(repo_root, '.repo', 'repo', 'repo')


class RepositoryTest(cros_test_lib.RunCommandTempDirTestCase):
  """Tests for repo_util.Repository."""

  MANIFEST_URL = 'https://example.com/manifest.xml'

  def setUp(self):
    self.empty_root = os.path.join(self.tempdir, 'empty')
    self.empty_root_subdir = os.path.join(self.empty_root, 'sub', 'dir')
    os.makedirs(self.empty_root_subdir)
    self.repo_root = os.path.join(self.tempdir, 'root')
    self.repo_root_subdir = os.path.join(self.repo_root, 'sub', 'dir')
    os.makedirs(self.repo_root_subdir)
    self.repo_dir = os.path.join(self.repo_root, '.repo')
    os.makedirs(self.repo_dir)

  def testInit(self):
    repo = repo_util.Repository(self.repo_root)
    self.assertTrue(os.path.samefile(repo.root, self.repo_root))

  def testInitNoRepoDir(self):
    with self.assertRaisesRegexp(repo_util.Error, r'no \.repo dir'):
      repo_util.Repository(self.empty_root)

  def testInitializeSimple(self):
    expected_cmd = ['repo', 'init', '--manifest-url', self.MANIFEST_URL]
    self.rc.AddCmdResult(expected_cmd, side_effect=RepoInitSideEffects)
    repo = repo_util.Repository.Initialize(self.empty_root, self.MANIFEST_URL)
    self.assertCommandCalled(expected_cmd, cwd=self.empty_root)
    self.assertTrue(os.path.samefile(repo.root, self.empty_root))

  def testInitializeComplex(self):
    expected_cmd = [
        'repo', 'init',
        '--manifest-url', 'http://manifest.xyz/manifest',
        '--manifest-branch', 'test-branch',
        '--manifest-name', 'other.xml',
        '--mirror',
        '--reference', '/repo/reference',
        '--depth', '99',
        '--groups', 'abba,queen',
        '--repo-url', 'https://repo.xyz/repo',
    ]
    self.rc.AddCmdResult(expected_cmd, side_effect=RepoInitSideEffects)
    repo = repo_util.Repository.Initialize(
        self.empty_root, 'http://manifest.xyz/manifest',
        manifest_branch='test-branch',
        manifest_name='other.xml',
        mirror=True,
        reference='/repo/reference',
        depth=99,
        groups='abba,queen',
        repo_url='https://repo.xyz/repo',
    )
    self.assertCommandCalled(expected_cmd, cwd=self.empty_root)
    self.assertTrue(os.path.samefile(repo.root, self.empty_root))

  def testInitializeExistingRepoDir(self):
    with self.assertRaisesRegexp(repo_util.Error, 'cannot init in existing'):
      repo_util.Repository.Initialize(self.repo_root, self.MANIFEST_URL)

  def testInitializeExistingRepoSubdir(self):
    with self.assertRaisesRegexp(repo_util.Error, 'cannot init in existing'):
      repo_util.Repository.Initialize(self.repo_root_subdir, self.MANIFEST_URL)

  def testInitializeFailCleanup(self):
    expected_cmd = ['repo', 'init', '--manifest-url', self.MANIFEST_URL]
    self.rc.AddCmdResult(expected_cmd, returncode=99,
                         side_effect=RepoInitSideEffects)
    with self.assertRaises(cros_build_lib.RunCommandError):
      repo_util.Repository.Initialize(self.empty_root, self.MANIFEST_URL)
    repo_dir = os.path.join(self.empty_root, '.repo')
    self.assertCommandCalled(['rm', '-rf', repo_dir])

  def testFind(self):
    repo = repo_util.Repository.Find(self.repo_root_subdir)
    self.assertEqual(repo.root, self.repo_root)

  def testFindNothing(self):
    self.assertIsNone(repo_util.Repository.Find(self.empty_root_subdir))

  def testMustFind(self):
    repo = repo_util.Repository.MustFind(self.repo_root_subdir)
    self.assertEqual(repo.root, self.repo_root)

  def testMustFindNothing(self):
    with self.assertRaisesRegexp(repo_util.Error, 'no repo found'):
      repo_util.Repository.MustFind(self.empty_root_subdir)


class RepositoryCommandMethodTest(cros_test_lib.RunCommandTempDirTestCase):
  """Tests for repo_util.Repository command methods."""

  # Testing _Run: pylint: disable=protected-access

  def setUp(self):
    self.root = os.path.join(self.tempdir, 'root')
    self.repo_dir = os.path.join(self.root, '.repo')
    self.subdir = os.path.join(self.root, 'sub', 'dir')
    os.makedirs(self.repo_dir)
    os.makedirs(self.subdir)
    self.repo = repo_util.Repository(self.root)

  def MockRun(self):
    self.repo._Run = mock.MagicMock()
    return self.repo._Run

  def testRun(self):
    self.repo._Run(['subcmd', 'arg'])
    self.assertCommandCalled([RepoCmdPath(self.root), 'subcmd', 'arg'],
                             cwd=self.root)

  def testRunSubDirCwd(self):
    self.repo._Run(['subcmd'], cwd=self.subdir)
    self.assertCommandCalled([RepoCmdPath(self.root), 'subcmd'],
                             cwd=self.subdir)

  def testRunBadCwd(self):
    with self.assertRaisesRegexp(repo_util.Error, 'cannot run `repo` outside'):
      self.repo._Run(['subcmd'], cwd=self.tempdir)

  def testSyncSimple(self):
    run = self.MockRun()
    self.repo.Sync()
    run.assert_called_with(['sync'], cwd=None)

  def testSyncComplex(self):
    run = self.MockRun()
    self.repo.Sync(
        projects=['p1', 'p2'], jobs=9, cwd=self.subdir)
    run.assert_called_with(
        ['sync', 'p1', 'p2', '--jobs', '9'], cwd=self.subdir)

  def testStartBranchSimple(self):
    run = self.MockRun()
    self.repo.StartBranch('my-branch')
    run.assert_called_with(['start', 'my-branch', '--all'], cwd=None)

  def testStartBranchComplex(self):
    run = self.MockRun()
    self.repo.StartBranch(
        'my-branch', projects=['foo', 'bar'], cwd=self.subdir)
    run.assert_called_with(
        ['start', 'my-branch', 'foo', 'bar'], cwd=self.subdir)
