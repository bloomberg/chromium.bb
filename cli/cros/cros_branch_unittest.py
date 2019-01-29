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
from chromite.cli.cros.cros_branch import CheckoutManager
from chromite.cli.cros.cros_branch import FactoryBranch
from chromite.cli.cros.cros_branch import FirmwareBranch
from chromite.cli.cros.cros_branch import ManifestRepository
from chromite.cli.cros.cros_branch import ReleaseBranch
from chromite.cli.cros.cros_branch import StabilizeBranch
from chromite.cli.cros.cros_branch import WhichVersionShouldBump
from chromite.lib import config_lib
from chromite.lib import constants
from chromite.lib import cros_test_lib
from chromite.lib import git
from chromite.lib import partial_mock
from chromite.lib import repo_manifest
from chromite.lib import repo_util


def ManifestXml(*args):
  """Joins arbitrary XML and wraps it in a <manifest> element."""
  xml = '\n'.join(args)
  return '<?xml version="1.0" encoding="UTF-8"?><manifest>%s</manifest>' % xml


def DefaultXml(remote, revision):
  """Generates a <default> XML element as a string.

  Args:
    remote: Remote attribute.
    revision: Revision attribute.

  Returns:
    The default element as a string.
  """
  return '<default remote="%s" revision="%s" sync-j="8"/>' % (remote, revision)


def RemoteXml(name, host):
  """Generates a <remote> XML element as a string.

  Args:
    name: Name attribute.
    host: Host for source code and review server, e.g. 'chromium'.

  Returns:
    The remote element as a string.
  """
  fetch = '%s.googlesource.com' % host
  review = '%s-review.googlesource.com' % host
  return '<remote fetch="%s" review="%s" name="%s"/>' % (fetch, review, name)


def ProjectXml(pid, name=None, root=None, branch_mode=None):
  """Generates a <project> XML element as a string.

  Args:
    pid: Project ID. Used to generate attributes not provided by caller.
    name: Name attribute.
    root: Root of the project path. PID is always appended as leaf directory.
    branch_mode: Value for branch-mode annotation.

  Returns:
    The project element as a string.
  """
  name = name or os.path.join('chromiumos', pid)
  path = pid if not root else os.path.join(root, pid)
  revision = git.NormalizeRef(pid)
  xml = '<project name="%s" path="%s" revision="%s">' % (name, path, revision)
  if branch_mode:
    xml += '<annotation name="branch-mode" value="%s"/>' % branch_mode
  return xml + '</project>'


def IncludeXml(name):
  """Generates a <include> XML element as a string.

  Args:
    name: Name attribute.

  Returns:
    The include element as a string.
  """
  return '<include name="%s"/>' % name


class ManifestTestCase(cros_test_lib.TestCase):
  """Test case providing valid manifest test data.

  This class generates a diverse collection of manifest XML strings, and
  provides convenience methods for reading from those manifests.
  """

  # A "project" in this dictionary is actually a project ID, which
  # is used by helper functions to generate project name/path/revision/etc.
  # If you add a project to this list, remember to update the categories below
  # as well as PROJECTS_EXTERNAL_XML and its internal equivalent.
  PROJECTS = config_lib.AttrDict({v.upper().replace('-', '_'): v for v in [
      'manifest',
      'manifest-internal',
      'chromiumos-overlay',
      'multicheckout-a',
      'multicheckout-b',
      'implicit-pinned',
      'explicit-tot',
      'explicit-branch',
      'explicit-pinned',
  ]})

  # Categorize the projects above for use in testing.
  PINNED_PROJECTS = (PROJECTS.EXPLICIT_PINNED, PROJECTS.IMPLICIT_PINNED)
  TOT_PROJECTS = (PROJECTS.EXPLICIT_TOT,)
  MULTI_CHECKOUT_PROJECTS = (PROJECTS.MULTICHECKOUT_A, PROJECTS.MULTICHECKOUT_B)
  SINGLE_CHECKOUT_PROJECTS = (PROJECTS.CHROMIUMOS_OVERLAY,
                              PROJECTS.EXPLICIT_BRANCH,
                              PROJECTS.MANIFEST,
                              PROJECTS.MANIFEST_INTERNAL)
  BRANCHED_PROJECTS = SINGLE_CHECKOUT_PROJECTS + MULTI_CHECKOUT_PROJECTS
  MANIFEST_PROJECTS = (PROJECTS.MANIFEST, PROJECTS.MANIFEST_INTERNAL)

  # Store commonly used values for convenience.
  TOT = git.NormalizeRef('master')
  SRC_PATH = 'src'
  THIRD_PARTY_PATH = os.path.join(SRC_PATH, 'third_party')
  EXTERNAL_FILE_NAME = 'external.xml'
  INTERNAL_FILE_NAME = 'internal.xml'

  # Create the raw XML using the above data. Note that by convention,
  # the leaf directory of the project path MUST end with the project ID.
  DEFAULT_XML = DefaultXml('cros', TOT)
  REMOTE_EXTERNAL_XML = RemoteXml('cros', 'chromium')
  REMOTE_INTERNAL_XML = RemoteXml('cros-internal', 'chrome-internal')
  PROJECTS_EXTERNAL_XML = '\n'.join([
      ProjectXml(PROJECTS.MANIFEST),
      ProjectXml(PROJECTS.CHROMIUMOS_OVERLAY,
                 name='chromiumos/overlays/chromiumos-overlay',
                 root=THIRD_PARTY_PATH),
      ProjectXml(PROJECTS.IMPLICIT_PINNED, name='faraway', root=SRC_PATH),
      ProjectXml(PROJECTS.MULTICHECKOUT_A,
                 name='chromiumos/multicheckout',
                 root=THIRD_PARTY_PATH),
      ProjectXml(PROJECTS.MULTICHECKOUT_B,
                 name='chromiumos/multicheckout',
                 root=THIRD_PARTY_PATH),
  ])
  PROJECTS_INTERNAL_XML = '\n'.join([
      ProjectXml(PROJECTS.MANIFEST_INTERNAL),
      ProjectXml(PROJECTS.EXPLICIT_PINNED,
                 root=SRC_PATH,
                 branch_mode=constants.MANIFEST_ATTR_BRANCHING_PIN),
      ProjectXml(PROJECTS.EXPLICIT_BRANCH,
                 root=SRC_PATH,
                 branch_mode=constants.MANIFEST_ATTR_BRANCHING_CREATE),
      ProjectXml(PROJECTS.EXPLICIT_TOT,
                 root=SRC_PATH,
                 branch_mode=constants.MANIFEST_ATTR_BRANCHING_TOT),
  ])
  INCLUDE_EXTERNAL_XML = IncludeXml(EXTERNAL_FILE_NAME)
  INCLUDE_INTERNAL_XML = IncludeXml(INTERNAL_FILE_NAME)

  # Combine the XML chunks above into meaningful files. Create files for
  # both manifest and manifest-internal projects.
  MANIFEST_FILES = {
      EXTERNAL_FILE_NAME: ManifestXml(DEFAULT_XML,
                                      REMOTE_EXTERNAL_XML,
                                      PROJECTS_EXTERNAL_XML),
      constants.OFFICIAL_MANIFEST: ManifestXml(INCLUDE_EXTERNAL_XML),
      constants.DEFAULT_MANIFEST: ManifestXml(INCLUDE_EXTERNAL_XML),
  }
  MANIFEST_INTERNAL_FILES = {
      EXTERNAL_FILE_NAME: MANIFEST_FILES[EXTERNAL_FILE_NAME],
      INTERNAL_FILE_NAME: ManifestXml(INCLUDE_EXTERNAL_XML,
                                      DEFAULT_XML,
                                      REMOTE_EXTERNAL_XML,
                                      REMOTE_INTERNAL_XML,
                                      PROJECTS_INTERNAL_XML),
      constants.OFFICIAL_MANIFEST: ManifestXml(INCLUDE_INTERNAL_XML,
                                               INCLUDE_EXTERNAL_XML),
      constants.DEFAULT_MANIFEST: ManifestXml(INCLUDE_INTERNAL_XML,
                                              INCLUDE_EXTERNAL_XML),
  }

  # Finally, store the full, parsed manifest XML. Essentially the output
  # of the command `repo manifest`.
  FULL_XML = ManifestXml(DEFAULT_XML,
                         REMOTE_EXTERNAL_XML,
                         REMOTE_INTERNAL_XML,
                         PROJECTS_EXTERNAL_XML,
                         PROJECTS_INTERNAL_XML)

  def PathFor(self, pid):
    """Return the test project's path.

    Args:
      pid: The test project ID (e.g. 'chromiumos-overlay').

    Returns:
      Path to the project, always of the form '<test path>/<project ID>'.
    """
    return self.ProjectFor(pid).Path()

  def PathListRegexFor(self, pid):
    """Return the test project's path as a ListRegex.

    Args:
      pid: The test project ID (e.g. 'chromiumos-overlay').

    Returns:
      partial_mock.ListRegex for project path.
    """
    return partial_mock.ListRegex('.*/%s' % self.PathFor(pid))

  def RevisionFor(self, pid):
    """Return the test project's revision.

    Args:
      pid: The test project ID (e.g. 'chromiumos-overlay')

    Returns:
      Reivision for the project, always of form 'refs/heads/<project ID>'.
    """
    return self.ProjectFor(pid).Revision()

  def ProjectFor(self, pid):
    """Return the test project's repo_manifest.Project.

    Args:
      pid: The test project ID (e.g. 'chromiumos-overlay')

    Returns:
      Corresponding repo_manifest.Project.
    """
    # Project paths always end with the project ID, so use that as key.
    match = [p for p in self.full_manifest.Projects() if p.Path().endswith(pid)]
    assert len(match) == 1
    return match[0]

  def setUp(self):
    # Parse and cache the full manifest to take advantage of the
    # utility functions in repo_manifest.
    self.full_manifest = repo_manifest.Manifest.FromString(self.FULL_XML)


class ManifestMockTestCase(ManifestTestCase, cros_test_lib.MockTestCase):
  """A ManifestTestCase that also mocks the standard repo_manifest library."""

  # Mimic the layout of manifest files on disk.
  MANIFESTS = {
      ManifestTestCase.PROJECTS.MANIFEST: ManifestTestCase.MANIFEST_FILES,
      ManifestTestCase.PROJECTS.MANIFEST_INTERNAL:
          ManifestTestCase.MANIFEST_INTERNAL_FILES,
  }

  def FromFileMock(self, source, allow_unsupported_features=False):
    """Forward repo_manifest.FromFile to repo_manifest.FromString.

    Args:
      source: Path to the test manifest. Used to look up XML in a table.
      allow_unsupported_features: See repo_manifest.Manifest.

    Returns:
      repo_manifest.Manifest created from test data.
    """
    manifest_path, manifest_file = os.path.split(source)
    manifest_project = os.path.basename(manifest_path)
    return repo_manifest.Manifest.FromString(
        self.MANIFESTS[manifest_project][manifest_file],
        allow_unsupported_features=allow_unsupported_features)

  def setUp(self):
    self.PatchObject(repo_manifest.Manifest, 'FromFile', self.FromFileMock)
    self.PatchObject(
        repo_util.Repository, 'Manifest', return_value=self.full_manifest)


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

  def testWhichVersionShouldBump(self):
    self.assertEqual(WhichVersionShouldBump('1.0.0'), 'branch')
    self.assertEqual(WhichVersionShouldBump('1.2.0'), 'patch')


class CheckoutManagerTest(ManifestMockTestCase):
  """Tests for CheckoutManager functions."""

  def AssertCommandCalledInProject(self, cmd, expected=True):
    self.rc_mock.assertCommandContains(
        cmd,
        cwd=partial_mock.ListRegex('.*/' + self.project.Path()),
        expected=expected)

  def SetCurrentBranch(self, branch):
    self.PatchObject(git, 'GetCurrentBranch', return_value=branch)

  def setUp(self):
    self.project = self.ProjectFor(self.PROJECTS.CHROMIUMOS_OVERLAY)
    self.branch = self.project.Revision()
    self.remote = self.project.Remote().GitName()
    self.rc_mock = cros_test_lib.RunCommandMock()
    self.rc_mock.SetDefaultCmdResult()
    self.StartPatcher(self.rc_mock)

  def testEnterNoCheckout(self):
    self.SetCurrentBranch(self.branch)
    with CheckoutManager(self.project):
      self.AssertCommandCalledInProject(['git', 'fetch'], expected=False)
      self.AssertCommandCalledInProject(['git', 'checkout'], expected=False)

  def testEnterWithCheckout(self):
    self.SetCurrentBranch(self.TOT)
    with CheckoutManager(self.project):
      self.AssertCommandCalledInProject(
          ['git', 'fetch', self.remote, self.branch])
      self.AssertCommandCalledInProject(['git', 'checkout', 'FETCH_HEAD'])

  def testExitNoCheckout(self):
    self.SetCurrentBranch(self.branch)
    with CheckoutManager(self.project):
      pass
    self.AssertCommandCalledInProject(['git', 'checkout'], expected=False)

  def testExitWithCheckout(self):
    self.SetCurrentBranch(self.TOT)
    with CheckoutManager(self.project):
      pass
    self.AssertCommandCalledInProject(['git', 'checkout', self.TOT])


class ManifestRepositoryTest(ManifestMockTestCase):
  """Tests for ManifestRepository functions."""

  LEAF_MANIFESTS = [ManifestTestCase.EXTERNAL_FILE_NAME]
  PARENT_MANIFESTS = [ManifestTestCase.INTERNAL_FILE_NAME]
  ALL_MANIFESTS = LEAF_MANIFESTS + PARENT_MANIFESTS

  BRANCHED_INTERNAL_PROJECTS = [ManifestTestCase.PROJECTS.MANIFEST_INTERNAL,
                                ManifestTestCase.PROJECTS.EXPLICIT_BRANCH]
  BRANCH = git.NormalizeRef('branch')

  def FilePathFor(self, filename):
    return os.path.join(self.PathFor(self.PROJECTS.MANIFEST_INTERNAL), filename)

  def BranchDictFor(self, projects):
    return {self.PathFor(project): self.BRANCH for project in projects}

  def GetGitRepoRevisionMock(self, cwd):
    return self.RevisionFor(os.path.basename(cwd))

  def AssertBranchedRevision(self, project):
    self.assertEqual(project.revision, self.BRANCH)

  def AssertPinnedRevision(self, project):
    self.assertEqual(
        project.revision,
        self.RevisionFor(os.path.basename(project.path)))

  def AssertTotRevision(self, project):
    self.assertEqual(project.revision, self.TOT)

  def TestListManifests(self, roots, expected_files):
    found = self.manifest_repo.ListManifests(roots)
    expected = map(self.FilePathFor, expected_files)
    self.assertItemsEqual(found, expected)

  def TestRepairManifest(self, group, assertion):
    repaired = self.manifest_repo.RepairManifest(
        self.FilePathFor(self.INTERNAL_FILE_NAME),
        self.BranchDictFor(self.BRANCHED_INTERNAL_PROJECTS))
    for project in repaired.Projects():
      if project in group:
        assertion(project)

  def setUp(self):
    self.PatchObject(git, 'GetGitRepoRevision', self.GetGitRepoRevisionMock)
    self.manifest_repo = ManifestRepository(
        self.PathFor(self.PROJECTS.MANIFEST_INTERNAL))

  def testManifestPath(self):
    self.assertEqual(
        self.manifest_repo.ManifestPath(self.EXTERNAL_FILE_NAME),
        self.FilePathFor(self.EXTERNAL_FILE_NAME))

  def testListManifestsSingleFileNoIncludes(self):
    self.TestListManifests(self.LEAF_MANIFESTS, self.LEAF_MANIFESTS)

  def testListManifestsSingleFileWithIncludes(self):
    self.TestListManifests(self.PARENT_MANIFESTS, self.ALL_MANIFESTS)

  def testListManifestsMultipleFilesWithIncludes(self):
    self.TestListManifests(self.ALL_MANIFESTS, self.ALL_MANIFESTS)

  def testRepairManifestDeletesDefaultRevisions(self):
    repaired = self.manifest_repo.RepairManifest(
        self.FilePathFor(self.INTERNAL_FILE_NAME),
        self.BranchDictFor(self.BRANCHED_INTERNAL_PROJECTS))
    self.assertIsNone(repaired.Default().revision)
    self.assertIsNone(repaired.GetRemote('cros').revision)

  def testRepairManifestUpdatesBranchedProjectRevisions(self):
    self.TestRepairManifest(self.BRANCHED_PROJECTS, self.AssertBranchedRevision)

  def testRepairManifestUpdatesPinnedProjectRevisions(self):
    self.TestRepairManifest(self.PINNED_PROJECTS, self.AssertPinnedRevision)

  def testRepairManifestUpdatesTotProjectRevisions(self):
    self.TestRepairManifest(self.TOT_PROJECTS, self.AssertTotRevision)


class BranchTest(ManifestMockTestCase):
  """Tests core functionality of Branch class."""

  MILESTONE = '12'
  VERSION = '3.4.0'
  BRANCH_NAME = 'branch'
  ORIGINAL_BRANCH_NAME = 'original'

  def CreateInstance(self):
    return Branch('kind', self.BRANCH_NAME)

  def AssertSynced(self, args):
    self.rc_mock.assertCommandContains(
        [partial_mock.ListRegex('.*/repo_sync_manifest')] + args)

  def AssertProjectBranched(self, project, branch):
    self.rc_mock.assertCommandContains(
        ['git', 'checkout', '-B', branch, self.RevisionFor(project)],
        cwd=self.PathListRegexFor(project))

  def AssertBranchRenamed(self, project, branch):
    self.rc_mock.assertCommandContains(
        ['git', 'branch', '-m', branch],
        cwd=self.PathListRegexFor(project))

  def AssertBranchDeleted(self, project, branch):
    self.rc_mock.assertCommandContains(
        ['git', 'branch', '-D', branch],
        cwd=self.PathListRegexFor(project))

  def AssertProjectNotBranched(self, project):
    self.rc_mock.assertCommandContains(
        ['git', 'checkout', '-B'],
        cwd=self.PathListRegexFor(project),
        expected=False)

  def AssertBranchNotModified(self, project):
    self.rc_mock.assertCommandContains(
        ['git', 'branch'],
        cwd=self.PathListRegexFor(project),
        expected=False)

  def AssertManifestRepairsCommitted(self):
    for manifest_project in self.MANIFEST_PROJECTS:
      self.rc_mock.assertCommandContains(
          ['git', 'commit', '-a'],
          cwd=partial_mock.ListRegex('.*/%s' % manifest_project))

  def ForEachBranchableProject(self, assertion):
    for proj in self.SINGLE_CHECKOUT_PROJECTS:
      assertion(proj, self.BRANCH_NAME)
    for proj in self.MULTI_CHECKOUT_PROJECTS:
      assertion(proj, '%s-%s' % (self.BRANCH_NAME, proj))

  def ForEachNonBranchableProject(self, assertion):
    for proj in self.PINNED_PROJECTS + self.TOT_PROJECTS:
      assertion(proj)

  def setUp(self):
    self.rc_mock = cros_test_lib.RunCommandMock()
    self.rc_mock.SetDefaultCmdResult()
    self.StartPatcher(self.rc_mock)

    # ManifestRepository and VersionInfo tested separately, so mock them.
    self.PatchObject(ManifestRepository, 'RepairManifestsOnDisk')
    self.PatchObject(
        VersionInfo,
        'from_repo',
        return_value=VersionInfo(self.VERSION, self.MILESTONE))
    self.PatchObject(VersionInfo, 'IncrementVersion')
    self.PatchObject(VersionInfo, 'UpdateVersionFile')

    # Instance must be created last for patches to be applied.
    self.inst = self.CreateInstance()

  def testCreateSyncsLocalCheckout(self):
    self.inst.Create(self.VERSION)
    self.AssertSynced(['--version', self.VERSION])

  def testCreateBranchesCorrectProjects(self):
    self.inst.Create(self.VERSION)
    self.ForEachBranchableProject(self.AssertProjectBranched)
    self.ForEachNonBranchableProject(self.AssertProjectNotBranched)

  def testCreateRepairsManifests(self):
    self.inst.Create(self.VERSION)
    self.AssertManifestRepairsCommitted()

  def testRenameSyncsToBranch(self):
    self.inst.Rename(self.ORIGINAL_BRANCH_NAME)
    self.AssertSynced(['--branch', self.ORIGINAL_BRANCH_NAME])

  def testRenameModifiesCorrectProjects(self):
    self.inst.Rename(self.ORIGINAL_BRANCH_NAME)
    self.ForEachBranchableProject(self.AssertBranchRenamed)
    self.ForEachNonBranchableProject(self.AssertBranchNotModified)

  def testRenameRepairsManifests(self):
    self.inst.Rename(self.ORIGINAL_BRANCH_NAME)
    self.AssertManifestRepairsCommitted()

  def testDeleteSyncsToBranch(self):
    self.inst.Delete()
    self.AssertSynced(['--branch', self.BRANCH_NAME])

  def testDeleteModifiesCorrectProjects(self):
    self.inst.Delete()
    self.ForEachBranchableProject(self.AssertBranchDeleted)
    self.ForEachNonBranchableProject(self.AssertBranchNotModified)


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
