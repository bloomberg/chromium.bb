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

  def _MockedRepository(self):
    r = repo_util.Repository(self.repo_root)
    r.RunRepo = mock.MagicMock()
    return r

  def testInit(self):
    r = repo_util.Repository(self.repo_root)
    self.assertTrue(os.path.samefile(r.root, self.repo_root))

  def testInitNoRepoDir(self):
    with self.assertRaises(repo_util.Error):
      repo_util.Repository(self.empty_root)

  def testInitializeSimple(self):
    expected_cmd = ['repo', 'init', '--manifest-url', self.MANIFEST_URL]
    self.rc.AddCmdResult(expected_cmd, side_effect=RepoInitSideEffects)
    r = repo_util.Repository.Initialize(self.empty_root, self.MANIFEST_URL)
    self.assertCommandCalled(expected_cmd, cwd=self.empty_root)
    self.assertTrue(os.path.samefile(r.root, self.empty_root))

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
    r = repo_util.Repository.Initialize(
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
    self.assertTrue(os.path.samefile(r.root, self.empty_root))

  def testInitializeExistingRepoDir(self):
    with self.assertRaises(repo_util.Error):
      repo_util.Repository.Initialize(self.repo_root, self.MANIFEST_URL)

  def testInitializeExistingRepoSubdir(self):
    with self.assertRaises(repo_util.Error):
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
    r = repo_util.Repository.Find(self.repo_root_subdir)
    self.assertEqual(r.root, self.repo_root)

  def testFindNothing(self):
    self.assertIsNone(repo_util.Repository.Find(self.empty_root_subdir))

  def testRunRepo(self):
    r = repo_util.Repository(self.repo_root)
    r.RunRepo('subcmd', '--arg', 'val', int_timeout=7, cwd='fake')
    self.assertCommandCalled(
        [RepoCmdPath(self.repo_root), 'subcmd', '--arg', 'val'],
        int_timeout=7, cwd=self.repo_root)

  def testSyncSimple(self):
    r = self._MockedRepository()
    r.Sync()
    r.RunRepo.assert_called_with('sync')

  def testSyncComplex(self):
    r = self._MockedRepository()
    r.Sync(projects=['p1', 'p2'], jobs=9)
    r.RunRepo.assert_called_with('sync', 'p1', 'p2', '--jobs', '9')
