# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module tests the `cros branch` command."""

from __future__ import print_function

import os

from chromite.cbuildbot.manifest_version import VersionInfo
from chromite.cli import command_unittest
from chromite.cli.cros.cros_branch import Branch
from chromite.cli.cros.cros_branch import BranchCommand
from chromite.cli.cros.cros_branch import CanBranchProject
from chromite.cli.cros.cros_branch import CanPinProject
from chromite.cli.cros.cros_branch import FactoryBranch
from chromite.cli.cros.cros_branch import FirmwareBranch
from chromite.cli.cros.cros_branch import ManifestRepository
from chromite.cli.cros.cros_branch import ReleaseBranch
from chromite.cli.cros.cros_branch import StabilizeBranch
from chromite.lib import cros_test_lib
from chromite.lib import git
from chromite.lib import partial_mock
from chromite.lib import repo_manifest
from chromite.lib import repo_util


class ManifestTestCase(cros_test_lib.MockTestCase):
  """Test case requiring manifest test data."""

  PATH = 'path/to'
  MANIFEST_OUTER_XML = '''\
<?xml version="1.0" encoding="UTF-8"?>
<manifest>%s</manifest>
  '''
  MANIFEST_BASE_FILE = 'official.xml'
  MANIFEST_BASE_PATH = os.path.join(PATH, MANIFEST_BASE_FILE)
  MANIFEST_BASE_XML = '''
  <remote fetch="https://chromium.googlesource.com"
          name="cros"
          review="chromium-review.googlesource.com"/>

  <default remote="cros" revision="refs/heads/master" sync-j="8"/>

  <!-- Test legacy heristic to determine branching strategy for projects. -->
  <project name="chromiumos/chromite"
           path="path/to/chromite"
           revision="refs/heads/chromite"/>
  <project name="chromiumos/special"
           path="path/to/special-new"
           revision="refs/heads/special-new"/>
  <project name="chromiumos/special"
           path="path/to/special-old"
           revision="refs/heads/special-old"/>
  <project name="faraway/external"
           path="path/to/external"
           revision="refs/heads/pinned"/>
  '''
  MANIFEST_INCLUDED_FILE = 'included.xml'
  MANIFEST_INCLUDED_PATH = os.path.join(PATH, MANIFEST_INCLUDED_FILE)
  MANIFEST_INCLUDED_XML = '''
  <!-- Test the explicitly specified branching strategy for projects. -->
  <project name="chromiumos/explicit-branch"
           path="path/to/explicit-branch"
           revision="refs/heads/explicit-branch">
    <annotation name="branch-mode" value="create"/>
  </project>
  <project name="chromiumos/external-explicitly-pinned"
           path="path/to/explicit-external"
           revision="refs/heads/pinned">
    <annotation name="branch-mode" value="pin"/>
  </project>
  <project name="faraway/external-explicitly-unpinned"
           path="path/to/explicit-unpinned">
    <annotation name="branch-mode" value="tot"/>
  </project>
  '''
  INCLUDES_XML = '''<include name="included.xml"/>'''
  MANIFEST_BASE_FULL_XML = MANIFEST_OUTER_XML % (
      INCLUDES_XML + MANIFEST_BASE_XML)
  MANIFEST_INCLUDED_FULL_XML = MANIFEST_OUTER_XML % MANIFEST_INCLUDED_XML
  MANIFEST_FULL_FILE = 'manifest.xml'
  MANIFEST_FULL_PATH = os.path.join(PATH, MANIFEST_FULL_FILE)
  MANIFEST_FULL_XML = MANIFEST_OUTER_XML % (
      MANIFEST_BASE_XML + MANIFEST_INCLUDED_XML)
  MANIFEST_TABLE = {
      MANIFEST_BASE_FILE: MANIFEST_BASE_FULL_XML,
      MANIFEST_INCLUDED_FILE: MANIFEST_INCLUDED_FULL_XML,
      MANIFEST_FULL_FILE: MANIFEST_FULL_XML,
  }
  PINNED_PROJECTS = ('explicit-external', 'external')
  TOT_PROJECTS = ('explicit-unpinned',)
  MULTI_CHECKOUT_PROJECTS = ('special-old', 'special-new')
  SINGLE_CHECKOUT_PROJECTS = ('chromite', 'explicit-branch')
  BRANCHED_PROJECTS = SINGLE_CHECKOUT_PROJECTS + MULTI_CHECKOUT_PROJECTS
  PINNED_BRANCH = git.NormalizeRef('pinned')
  TOT_BRANCH = git.NormalizeRef('master')

  def PathFor(self, pid):
    """Return the test project's path.

    Args:
      pid: The test project ID (e.g. 'chromite').

    Returns:
      Path to the project, always of the form '<test path>/<project ID>'.
    """
    return os.path.join(self.PATH, pid)

  def RevisionFor(self, pid):
    """Return the test project's revision.

    Args:
      pid: The test project ID (e.g. 'chromite')

    Returns:
      Reivision for the project, always of form 'refs/heads/<project ID>'.
    """
    return git.NormalizeRef(pid)

  def FromFileMock(self, source, allow_unsupported_features=False):
    """Forward repo_manifest.FromFile to repo_manifest.FromString.

    Args:
      source: Path to the test manifest. Used to look up XML in a table.
      allow_unsupported_features: See repo_manifest.Manifest.

    Returns:
      repo_manifest.Manifest created from test data.
    """
    return repo_manifest.Manifest.FromString(
        self.MANIFEST_TABLE[os.path.basename(source)],
        allow_unsupported_features=allow_unsupported_features)

  def setUp(self):
    self.full_manifest = self.FromFileMock(self.MANIFEST_FULL_PATH)
    self.PatchObject(repo_manifest.Manifest, 'FromFile', self.FromFileMock)
    self.PatchObject(
        repo_util.Repository,
        'Manifest',
        return_value=repo_manifest.Manifest.FromString(self.MANIFEST_FULL_XML))


class UtilitiesTest(ManifestTestCase):
  """Tests for all top-level utility functions."""

  def testCanBranchProject(self):
    projects = filter(CanBranchProject, self.full_manifest.Projects())
    self.assertItemsEqual([p.Path() for p in projects],
                          map(self.PathFor, self.BRANCHED_PROJECTS))

  def testCanPinProject(self):
    projects = filter(CanPinProject, self.full_manifest.Projects())
    self.assertItemsEqual([p.Path() for p in projects],
                          map(self.PathFor, self.PINNED_PROJECTS))

  def testTotMutualExclusivity(self):
    is_tot = lambda proj: not (CanPinProject(proj) or CanBranchProject(proj))
    projects = filter(is_tot, self.full_manifest.Projects())
    self.assertItemsEqual([p.Path() for p in projects],
                          map(self.PathFor, self.TOT_PROJECTS))


class ManifestRepositoryTest(ManifestTestCase):
  """Tests for ManifestRepository functions."""

  def setUp(self):
    self.manifest_repo = ManifestRepository(self.PATH)
    self.PatchObject(git, 'GetGitRepoRevision', return_value=self.PINNED_BRANCH)

  def testManifestPath(self):
    self.assertEqual(
        self.manifest_repo.ManifestPath(self.MANIFEST_BASE_FILE),
        self.MANIFEST_BASE_PATH)
    self.assertEqual(
        self.manifest_repo.ManifestPath(self.MANIFEST_INCLUDED_FILE),
        self.MANIFEST_INCLUDED_PATH)

  def testListManifestsSingleFileNoIncludes(self):
    found = self.manifest_repo.ListManifests([self.MANIFEST_INCLUDED_FILE])
    self.assertItemsEqual([self.MANIFEST_INCLUDED_PATH], found)

  def testListManifestsSingleFileWithIncludes(self):
    found = self.manifest_repo.ListManifests([self.MANIFEST_BASE_FILE])
    expected = [self.MANIFEST_BASE_PATH, self.MANIFEST_INCLUDED_PATH]
    self.assertItemsEqual(expected, found)

  def testListManifestsMultipleFilesWithIncludes(self):
    found = self.manifest_repo.ListManifests(
        [self.MANIFEST_BASE_FILE, self.MANIFEST_INCLUDED_FILE])
    expected = [self.MANIFEST_BASE_PATH, self.MANIFEST_INCLUDED_PATH]
    self.assertItemsEqual(expected, found)

  def testRepairManifest(self):
    branches = {
        self.PathFor(project): self.RevisionFor(project)
        for project in self.BRANCHED_PROJECTS
    }
    manifest = self.manifest_repo.RepairManifest(self.MANIFEST_FULL_PATH,
                                                 branches)
    self.assertIsNone(manifest.Default().revision)
    self.assertIsNone(manifest.GetRemote('cros').revision)
    for project in manifest.Projects():
      project_id = os.path.basename(project.path)
      if project_id in self.PINNED_PROJECTS:
        self.assertEqual(project.revision, self.PINNED_BRANCH)
      elif project_id in self.TOT_PROJECTS:
        self.assertEqual(project.revision, self.TOT_BRANCH)
      else:
        self.assertEqual(project.revision, self.RevisionFor(project_id))


class BranchTest(ManifestTestCase):
  """Establishes environment for testing sublcasses of Branch."""

  MILESTONE = '12'
  VERSION = '3.4.5'
  BRANCH_NAME = 'branch'

  def CreateInstance(self):
    return Branch('fake-kind', self.BRANCH_NAME)

  def AssertSynced(self, version):
    self.rc_mock.assertCommandContains(
        [partial_mock.ListRegex('.*/repo_sync_manifest'), '--version', version])

  def AssertProjectBranched(self, project, branch):
    self.rc_mock.assertCommandContains(
        ['git', 'checkout', '-B', branch,
         self.RevisionFor(project)],
        cwd=partial_mock.ListRegex('.*/%s' % self.PathFor(project)))

  def AssertProjectNotBranched(self, project):
    self.rc_mock.assertCommandContains(
        ['git', 'checkout', '-B'],
        cwd=partial_mock.ListRegex('.*/%s' % self.PathFor(project)),
        expected=False)

  def setUp(self):
    self.rc_mock = cros_test_lib.RunCommandMock()
    self.rc_mock.SetDefaultCmdResult()
    self.StartPatcher(self.rc_mock)

    self.PatchObject(
        VersionInfo,
        'from_repo',
        return_value=VersionInfo(self.VERSION, self.MILESTONE))

    # ManifestRepository is tested separately, so mock it out.
    self.PatchObject(ManifestRepository, 'RepairManifestsOnDisk')

    # Instance must be created last for patches to be applied.
    self.inst = self.CreateInstance()

  def testCreateLocal(self):
    self.inst.Create(self.VERSION)

    # Assert that local checkout was synced to correct version.
    self.AssertSynced(self.VERSION)

    # Assert that all branchable projects were branched.
    for proj in self.SINGLE_CHECKOUT_PROJECTS:
      self.AssertProjectBranched(proj, self.BRANCH_NAME)
    for proj in self.MULTI_CHECKOUT_PROJECTS:
      self.AssertProjectBranched(proj, '%s-%s' % (self.BRANCH_NAME, proj))
    for proj in self.PINNED_PROJECTS + self.TOT_PROJECTS:
      self.AssertProjectNotBranched(proj)

    # Assuming ManifestRepository did its job, make sure repairs are committed.
    for manifest_project in ('manifest', 'manifest-internal'):
      self.rc_mock.assertCommandContains(
          ['git', 'commit', '-a'],
          cwd=partial_mock.ListRegex('.*/%s' % manifest_project))


class ReleaseBranchTest(BranchTest):
  """Tests and config specifically for ReleaseBranch."""
  BRANCH_NAME = 'release-R12-3.B'

  def CreateInstance(self):
    return ReleaseBranch()


class FactoryBranchTest(BranchTest):
  """Tests and config specifically for FactoryBranch."""
  BRANCH_NAME = 'factory-3.B'

  def CreateInstance(self):
    return FactoryBranch()


class FirmwareBranchTest(BranchTest):
  """Tests and config specifically for FirmwareBranch."""
  BRANCH_NAME = 'firmware-3.B'

  def CreateInstance(self):
    return FirmwareBranch()


class StabilizeBranchTest(BranchTest):
  """Tests and config specifically for StabilizeBranch."""
  BRANCH_NAME = 'stabilize-3.B'

  def CreateInstance(self):
    return StabilizeBranch()


class MockBranchCommand(command_unittest.MockCommand):
  """Mock out the `cros branch` command."""
  TARGET = 'chromite.cli.cros.cros_branch.BranchCommand'
  TARGET_CLASS = BranchCommand
  COMMAND = 'branch'

  def __init__(self, *args, **kwargs):
    command_unittest.MockCommand.__init__(self, *args, **kwargs)

  def Run(self, inst):
    return command_unittest.MockCommand.Run(self, inst)


class BranchCommandTest(cros_test_lib.MockTestCase):
  """Tests for BranchCommand functions."""
  VERSION = '3.4.5'
  BRANCH_NAME = 'branch'

  def SetUpCommandMock(self, args):
    cmd = MockBranchCommand(args)
    self.StartPatcher(cmd)
    return cmd

  def TestCreateCommandParsesFor(self, cls, kind_flag):
    cmd = self.SetUpCommandMock(['create', self.VERSION, kind_flag])

    self.assertIs(cmd.inst.options.cls, cls)

    # It's important that `cros branch` does not perform dangerous
    # operations by default.
    self.assertIsNone(cmd.inst.options.name)
    self.assertFalse(cmd.inst.options.force)
    self.assertFalse(cmd.inst.options.push)

  def testCreateReleaseCommandParses(self):
    self.TestCreateCommandParsesFor(ReleaseBranch, '--release')

  def testCreateFactoryCommandParses(self):
    self.TestCreateCommandParsesFor(FactoryBranch, '--factory')

  def testCreateFirmwareCommandParses(self):
    self.TestCreateCommandParsesFor(FirmwareBranch, '--firmware')

  def testCreateStabilizeCommandParses(self):
    self.TestCreateCommandParsesFor(StabilizeBranch, '--stabilize')
