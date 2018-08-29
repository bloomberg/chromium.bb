# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for chromite.lib.repo and helpers for testing that module."""

from __future__ import print_function

import os

from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import cros_test_lib
from chromite.lib import git
from chromite.lib import osutils
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
    """Test Repository.__init__."""
    repo = repo_util.Repository(self.repo_root)
    self.assertTrue(os.path.samefile(repo.root, self.repo_root))

  def testInitNoRepoDir(self):
    """Test Repository.__init__ fails if not in repo."""
    with self.assertRaises(repo_util.NotInRepoError):
      repo_util.Repository(self.empty_root)

  def testInitializeSimple(self):
    """Test Repository.Initialize simple call."""
    expected_cmd = ['repo', 'init', '--manifest-url', self.MANIFEST_URL]
    self.rc.AddCmdResult(expected_cmd, side_effect=RepoInitSideEffects)
    repo = repo_util.Repository.Initialize(self.empty_root, self.MANIFEST_URL)
    self.assertCommandCalled(expected_cmd, cwd=self.empty_root)
    self.assertTrue(os.path.samefile(repo.root, self.empty_root))

  def testInitializeComplex(self):
    """Test Repository.Initialize complex call."""
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
    """Test Repository.Initialize fails in existing repo dir."""
    with self.assertRaisesRegexp(repo_util.Error, 'cannot init in existing'):
      repo_util.Repository.Initialize(self.repo_root, self.MANIFEST_URL)

  def testInitializeExistingRepoSubdir(self):
    """Test Repository.Initialize fails in existing repo subdir."""
    with self.assertRaisesRegexp(repo_util.Error, 'cannot init in existing'):
      repo_util.Repository.Initialize(self.repo_root_subdir, self.MANIFEST_URL)

  def testInitializeFailCleanup(self):
    """Test Repository.Initialize failure deletes .repo."""
    expected_cmd = ['repo', 'init', '--manifest-url', self.MANIFEST_URL]
    self.rc.AddCmdResult(expected_cmd, returncode=99,
                         side_effect=RepoInitSideEffects)
    rmdir = self.PatchObject(osutils, 'RmDir')
    with self.assertRaises(cros_build_lib.RunCommandError):
      repo_util.Repository.Initialize(self.empty_root, self.MANIFEST_URL)
    repo_dir = os.path.join(self.empty_root, '.repo')
    rmdir.assert_any_call(repo_dir, ignore_missing=True)

  def testFind(self):
    """Test Repository.Find finds repo from subdir."""
    repo = repo_util.Repository.Find(self.repo_root_subdir)
    self.assertEqual(repo.root, self.repo_root)

  def testFindNothing(self):
    """Test Repository.Find finds nothing from non-repo dir."""
    self.assertIsNone(repo_util.Repository.Find(self.empty_root_subdir))

  def testMustFind(self):
    """Test Repository.MustFind finds repo from subdir."""
    repo = repo_util.Repository.MustFind(self.repo_root_subdir)
    self.assertEqual(repo.root, self.repo_root)

  def testMustFindNothing(self):
    """Test Repository.MustFind fails from non-repo dir."""
    with self.assertRaises(repo_util.NotInRepoError):
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

  def AssertRepoCalled(self, repo_args, **kwargs):
    kwargs.setdefault('cwd', self.root)
    kwargs.setdefault('capture_output', False)
    kwargs.setdefault('debug_level', logging.DEBUG)
    self.assertCommandCalled([RepoCmdPath(self.root)] + repo_args, **kwargs)

  def AddRepoResult(self, repo_args, **kwargs):
    self.rc.AddCmdResult([RepoCmdPath(self.root)] + repo_args, **kwargs)

  def testAssertRepoCalled(self):
    """Test that AddRepoResult test helper works."""
    self.repo._Run(['subcmd', 'arg1'])
    with self.assertRaises(AssertionError):
      self.AssertRepoCalled(['subcmd', 'arg2'])
    with self.assertRaises(AssertionError):
      self.AssertRepoCalled(['subcmd'])
    with self.assertRaises(AssertionError):
      self.AssertRepoCalled(['subcmd', 'arg1'], cwd='other_dir')
    self.AssertRepoCalled(['subcmd', 'arg1'])

  def testRun(self):
    """Test Repository._Run repo_cmd."""
    self.repo._Run(['subcmd', 'arg'])
    self.AssertRepoCalled(['subcmd', 'arg'])

  def testRunSubDirCwd(self):
    """Test Repository._Run cwd."""
    self.repo._Run(['subcmd'], cwd=self.subdir)
    self.AssertRepoCalled(['subcmd'], cwd=self.subdir)

  def testRunBadCwd(self):
    """Test Repository._Run fails on cwd outside of repo."""
    with self.assertRaises(repo_util.NotInRepoError):
      self.repo._Run(['subcmd'], cwd=self.tempdir)

  def testSyncSimple(self):
    """Test Repository.Sync simple call."""
    self.repo.Sync()
    self.AssertRepoCalled(['sync'])

  def testSyncComplex(self):
    """Test Repository.Sync complex call."""
    self.repo.Sync(
        projects=['p1', 'p2'], local_only=True, current_branch=True, jobs=9,
        cwd=self.subdir)
    self.AssertRepoCalled(
        ['sync', 'p1', 'p2', '--local-only', '--current-branch', '--jobs', '9'],
        cwd=self.subdir)

  def testStartBranchSimple(self):
    """Test Repository.StartBranch simple call."""
    self.repo.StartBranch('my-branch')
    self.AssertRepoCalled(['start', 'my-branch', '--all'])

  def testStartBranchComplex(self):
    """Test Repository.StartBranch complex call."""
    self.repo.StartBranch(
        'my-branch', projects=['foo', 'bar'], cwd=self.subdir)
    self.AssertRepoCalled(
        ['start', 'my-branch', 'foo', 'bar'], cwd=self.subdir)

  def testListSimple(self):
    """Test Repository.List simple call."""
    output = 'src/project : my/project\nsrc/ugly : path : other/project\n'
    self.AddRepoResult(['list'], output=output)
    projects = self.repo.List()
    self.AssertRepoCalled(['list'], capture_output=True)
    self.assertListEqual(projects, [
        repo_util.ProjectInfo(name='my/project', path='src/project'),
        repo_util.ProjectInfo(name='other/project', path='src/ugly : path'),
    ])

  def testListComplex(self):
    """Test Repository.List complex call."""
    self.repo.List(['project1', 'project2'], cwd=self.subdir)
    self.AssertRepoCalled(['list', 'project1', 'project2'],
                          cwd=self.subdir, capture_output=True)

  def testListProjectNotFound(self):
    """Test Repository.List fails when given a nonexistant project."""
    self.AddRepoResult(['list', 'foobar'], returncode=1,
                       error='error: project foobar not found')
    with self.assertRaises(repo_util.ProjectNotFoundError):
      self.repo.List(['foobar'])

  def testManifest(self):
    """Test Repository.Manifest."""
    output = '<manifest></manifest>'
    self.AddRepoResult(['manifest'], output=output)
    manifest = self.repo.Manifest()
    self.assertIsNotNone(manifest)

  def testCopy(self):
    """Test Repository.Copy."""
    copy_root = os.path.join(self.tempdir, 'copy')
    os.mkdir(copy_root)
    def mkdirDestRepo(*_args, **_kwargs):
      os.mkdir(os.path.join(copy_root, '.repo'))

    output = 'src/p1 : p1\nother : other/project\n'
    self.AddRepoResult(['list'], output=output, side_effect=mkdirDestRepo)
    copy = self.repo.Copy(copy_root)
    self.assertEqual(copy.root, copy_root)
    self.assertCommandCalled([
        'cp', '--archive', '--link', '--parents',
        '.repo/project-objects/p1.git/objects', copy_root,
    ], cwd=self.root, extra_env={'LC_MESSAGES': 'C'})
    self.assertCommandCalled([
        'cp', '--archive', '--link', '--parents',
        '.repo/project-objects/other/project.git/objects', copy_root,
    ], cwd=self.root, extra_env={'LC_MESSAGES': 'C'})
    self.assertCommandCalled([
        'cp', '--archive', '--no-clobber', '.repo', copy_root,
    ], cwd=self.root, extra_env={'LC_MESSAGES': 'C'})


@cros_test_lib.NetworkTest()
class RepositoryIntegrationTest(cros_test_lib.TempDirTestCase):
  """Tests for repo_util.Repository that actually call `repo`.

  Note that these test methods are *not* independent: they must run in
  definition order.
  """

  MANIFEST_URL = constants.EXTERNAL_GOB_URL + '/chromiumos/manifest.git'
  REPO_URL = constants.EXTERNAL_GOB_URL + '/external/repo.git'
  PROJECT = 'chromiumos/chromite'
  PROJECT_DIR = 'chromite'

  root = None
  repo = None
  copy = None
  project_path = None

  tests = []

  def runTest(self):
    for test in self.tests:
      test(self)

  @tests.append
  def testInitialize(self):
    """Test Repository.Initialize creates a .repo dir."""
    self.root = os.path.join(self.tempdir, 'root')
    os.mkdir(self.root)
    # Try to use the current repo as a --reference.
    reference = repo_util.Repository.Find(constants.SOURCE_ROOT)
    self.repo = repo_util.Repository.Initialize(
        self.root,
        manifest_url=self.MANIFEST_URL,
        reference=reference,
        depth=1,
        repo_url=self.REPO_URL)
    self.assertExists(os.path.join(self.root, '.repo'))

  @tests.append
  def testSync(self):
    """Test Repository.Sync creates a project checkout dir."""
    self.repo.Sync([self.PROJECT], current_branch=True)
    self.project_path = os.path.join(self.root, self.PROJECT_DIR)
    self.assertExists(self.project_path)

  @tests.append
  def testMustFind(self):
    """Test Repository.MustFind finds the Repository from a subdir."""
    repo = repo_util.Repository.MustFind(self.project_path)
    self.assertEqual(repo.root, self.root)

  @tests.append
  def testList(self):
    """Test Repository.List returns correct ProjectInfo."""
    projects = self.repo.List([self.PROJECT])
    self.assertEqual(projects, [
        repo_util.ProjectInfo(name=self.PROJECT, path=self.PROJECT_DIR)])

  @tests.append
  def testStartBranch(self):
    """Test Repository.StartBranch creates a git branch."""
    self.repo.StartBranch('my-branch')
    project_branch = git.GetCurrentBranch(self.project_path)
    self.assertEqual(project_branch, 'my-branch')

  @tests.append
  def testManifest(self):
    """Test Repository.Manifest includes project."""
    manifest = self.repo.Manifest()
    project = manifest.GetUniqueProject(self.PROJECT)
    self.assertEqual(project.path, self.PROJECT_DIR)

  @tests.append
  def testCopy(self):
    """Test Repository.Copy creates a working copy."""
    copy_root = os.path.join(self.tempdir, 'copy')
    os.mkdir(copy_root)
    copy = self.repo.Copy(copy_root)
    self.assertEqual(copy.root, copy_root)
    copy.Sync([self.PROJECT], local_only=True)
    self.assertExists(os.path.join(copy_root, self.PROJECT_DIR))
    # Check for hardlinking; any object file will do.
    objects_path = repo_util.PROJECT_OBJECTS_PATH_FORMAT % self.PROJECT
    for dirname, _, files in os.walk(os.path.join(self.root, objects_path)):
      if files:
        object_file = os.path.join(dirname, files[0])
        break
    else:
      self.fail('no object file found')
    self.assertTrue(os.path.samefile(
        os.path.join(self.root, object_file),
        os.path.join(copy_root, object_file)))
