# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Command for managing branches of chromiumos.

See go/cros-release-faq for information on types of branches, branching
frequency, naming conventions, etc.
"""

from __future__ import print_function

import os
import re

from chromite.cbuildbot import manifest_version
from chromite.cli import command
from chromite.lib import cros_logging as logging
from chromite.lib import config_lib
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import git
from chromite.lib import repo_manifest
from chromite.lib import repo_util


class BranchError(Exception):
  """Raised whenever any branch operation fails."""


def AbsoluteProjectPath(relative_path):
  """Returns the absolute path of a project on disk.

  Args:
    relative_path: Relative path to the project, e.g. 'chromite/'

  Returns:
    The absolute path to the project.
  """
  return os.path.join(constants.SOURCE_ROOT, relative_path)


def ReadVersionInfo(**kwargs):
  """Returns VersionInfo for the current checkout."""
  return manifest_version.VersionInfo.from_repo(constants.SOURCE_ROOT, **kwargs)


def BranchMode(project):
  """Returns the project's explicit branch mode, if specified."""
  return project.Annotations().get('branch-mode', None)


def CanBranchProject(project):
  """Returns true if the project can be branched.

  The preferred way to specify branchability is by adding a "branch-mode"
  annotation on the project in the manifest. Of course, only one project
  in the manifest actually does this.

  The legacy method is to peek at the project's remote.

  Args:
    project: The repo_manifest.Project in question.

  Returns:
    True if the project is not pinned or ToT.
  """
  site_params = config_lib.GetSiteParams()
  remote = project.Remote().GitName()
  explicit_mode = BranchMode(project)
  if not explicit_mode:
    return (remote in site_params.CROS_REMOTES and
            remote in site_params.BRANCHABLE_PROJECTS and
            re.match(site_params.BRANCHABLE_PROJECTS[remote], project.name))
  return explicit_mode == constants.MANIFEST_ATTR_BRANCHING_CREATE


def CanPinProject(project):
  """Returns true if the project can be pinned.

  Args:
    project: The repo_manifest.Project in question.

  Returns:
    True if the project is pinned.
  """
  explicit_mode = BranchMode(project)
  if not explicit_mode:
    return not CanBranchProject(project)
  return explicit_mode == constants.MANIFEST_ATTR_BRANCHING_PIN


def WhichVersionShouldBump():
  """Returns which version is incremented by builds on a new branch."""
  vinfo = ReadVersionInfo()
  if int(vinfo.patch_number):
    raise BranchError('Cannot bump version with nonzero patch number.')
  return 'patch' if int(vinfo.branch_build_number) else 'branch'


# TODO(evanhernandez): Consider a chromite/lib library for repo_sync_manifest.
def Sync(manifest_args):
  """Run repo_sync_manifest command and return the full synced manifest.

  Args:
    manifest_args: List of args for manifest group of repo_sync_manifest.

  Returns:
    The full synced manifest as a repo_manifest.Manifest.
  """
  cros_build_lib.RunCommand([
      os.path.join(constants.CHROMITE_DIR, 'scripts/repo_sync_manifest'),
      '--repo-root', constants.SOURCE_ROOT
  ] + manifest_args, quiet=True)
  return repo_util.Repository(constants.SOURCE_ROOT).Manifest()


def SyncBranch(branch):
  """Sync to the given branch and return the synced manifest.

  Args:
    branch: Name of branch to sync to.

  Returns:
    The synced manifest as a repo_manifest.Manifest.
  """
  return Sync(['--branch', branch])


def SyncVersion(version):
  """Sync to the given version and return the synced manifest.

  Args:
    version: Version string to sync to.

  Returns:
    The synced manifest as a repo_manifest.Manifest.
  """
  site_params = config_lib.GetSiteParams()
  return Sync([
      '--manifest-versions-int',
      AbsoluteProjectPath(site_params.INTERNAL_MANIFEST_VERSIONS_PATH),
      '--manifest-versions-ext',
      AbsoluteProjectPath(site_params.EXTERNAL_MANIFEST_VERSIONS_PATH),
      '--version', version
  ])


class CheckoutManager(object):
  """Context manager for checking out a chromiumos project to a revision.

  This manager handles fetching and checking out to a project's remote ref, as
  specified in the manifest. It also ensures that the local checkout always
  returns to its initial state once the context exits.
  """

  def __init__(self, project):
    """Determine whether project needs to be checked out.

    Args:
      project: A repo_manifest.Project to be checked out.
    """
    self._path = AbsoluteProjectPath(project.Path())
    self._remote = project.Remote().GitName()
    self._ref = git.NormalizeRef(project.upstream or project.Revision())
    self._original_ref = git.NormalizeRef(git.GetCurrentBranch(self._path))
    self._needs_checkout = self._ref != self._original_ref

  def __enter__(self):
    if self._needs_checkout:
      git.RunGit(self._path, ['fetch', self._remote, self._ref])
      git.RunGit(self._path, ['checkout', 'FETCH_HEAD'])
    return self

  def __exit__(self, exc_type, exc_value, traceback):
    if self._needs_checkout:
      git.RunGit(self._path, ['checkout', self._original_ref])


class ManifestRepository(object):
  """Represents a git repository of manifest XML files."""

  def __init__(self, path):
    self._path = path

  def ManifestPath(self, name):
    """Returns the full path to the manifest.

    Args:
      name: Name of the manifest.

    Returns:
      Full path to the manifest.
    """
    return os.path.join(self._path, name)

  def ReadManifest(self, path):
    """Read the manifest at the given path.

    Args:
      path: Path to the manifest.

    Returns:
      repo_manifest.Manifest object.
    """
    return repo_manifest.Manifest.FromFile(
        path, allow_unsupported_features=True)

  def ListManifests(self, root_manifests):
    """Finds all manifests included directly or indirectly by root manifests.

    For convenience, the returned set includes the root manifests.

    Args:
      root_manifests: Manifests whose includes will be traversed.

    Returns:
      Set of paths to included manifests.
    """
    pending = list(root_manifests)
    found = set()
    while pending:
      path = self.ManifestPath(pending.pop())
      if path in found:
        continue
      found.add(path)
      pending.extend([inc.name for inc in self.ReadManifest(path).Includes()])
    return found

  def RepairManifest(self, path, branches):
    """Reads the manifest at the given path and repairs it in memory.

    Because humans rarely read branched manifests, this function optimizes for
    code readability and explicitly sets revision on every project in the
    manifest, deleting any defaults.

    Args:
      path: Path to the manifest.
      branches: Dict mapping project path to branch name.

    Returns:
      The repaired repo_manifest.Manifest object.
    """
    manifest = self.ReadManifest(path)

    # Delete the default revision if specified by original manifest.
    default = manifest.Default()
    if default.revision:
      del default.revision

    # Delete remote revisions if specified by original manifest.
    for remote in manifest.Remotes():
      if remote.revision:
        del remote.revision

    # Update all project revisions.
    for project in manifest.Projects():
      path = project.Path()
      if CanBranchProject(project):
        project.revision = git.NormalizeRef(branches[path])
      elif CanPinProject(project):
        project.revision = git.GetGitRepoRevision(AbsoluteProjectPath(path))
      else:
        project.revision = git.NormalizeRef('master')

      if project.upstream:
        del project.upstream

    return manifest

  def RepairManifestsOnDisk(self, branches):
    """Repairs the revision and upstream attributes of manifest elements.

    The original manifests are overwritten by the repaired manifests.
    Note this method is "deep" because it processes includes.

    Args:
      branches: Dict mapping project path to branch name.
    """
    logging.info('Repairing manifest repository %s', self._path)
    manifest_paths = self.ListManifests(
        [constants.DEFAULT_MANIFEST, constants.OFFICIAL_MANIFEST])
    for path in manifest_paths:
      logging.info('Repairing manifest file %s', path)
      self.RepairManifest(path, branches).Write(path)


class Branch(object):
  """Represents a branch of chromiumos, which may or may not exist yet.

  Note that all local branch operations assume the current checkout is
  synced to the correct version.
  """

  def __init__(self, name, manifest):
    """Cache various configuration used by all branch operations.

    Args:
      name: The name of the branch.
      manifest: The currently synced manifest.
    """
    self.name = name
    self.manifest = manifest

  def _ProjectBranchName(self, project):
    """Determine's the git branch name for the project.

    Args:
      project: The repo_manfest.Project in question.

    Returns:
      The branch name for the project.
    """
    # If project has only one checkout, the base branch name is fine.
    checkouts = [p.name for p in self.manifest.Projects()].count(project.name)
    if checkouts == 1:
      return self.name
    # Otherwise, the project branch name needs a suffix. We append its
    # upstream or revision to distinguish it from other checkouts.
    suffix = git.StripRefs(project.upstream or project.Revision())
    return '%s-%s' % (self.name, suffix)

  def _CreateProjectBranch(self, project):
    """Create this branch for the given project.

    Args:
      project: The repo_manifest.Project to branch.

    Returns:
      Name of the new branch on the project.
    """
    branch = self._ProjectBranchName(project)
    git.RunGit(
        AbsoluteProjectPath(project.Path()),
        ['checkout', '-B', branch, project.Revision()],
        print_cmd=True)
    return branch

  def _RenameProjectBranch(self, project):
    """Rename the current branch of the project checkout.

    Args:
      project: The repo_manifest.Project whose current branch will be renamed.

    Returns:
      Name of the new branch on the project.
    """
    branch = self._ProjectBranchName(project)
    git.RunGit(
        AbsoluteProjectPath(project.Path()),
        ['branch', '-m', branch],
        print_cmd=True)
    return branch

  def _DeleteProjectBranch(self, project):
    """Delete this branch for the given project.

    Args:
      project: The repo_manifest.Project whose current branch will be deleted.

    Returns:
      Name of the deleted branch on the project.
    """
    branch = self._ProjectBranchName(project)
    git.RunGit(
        AbsoluteProjectPath(project.Path()),
        ['branch', '-D', branch],
        print_cmd=True)
    return branch

  def _MapBranchableProjects(self, transform):
    """Apply the given operator to all branchable projects.

    Args:
      transform: Function mapping branchable repo_manifest.Project to new value.

    Returns:
      Dict mapping (unique) project path to transformation result.
    """
    return {project.Path(): transform(project) for project in
            filter(CanBranchProject, self.manifest.Projects())}

  def _RepairManifestRepositories(self, branches):
    """Repair all manifests in all manifest repositories.

    Args:
      branches: Dict mapping project names to branch names.
    """
    for repo_name in ('manifest', 'manifest-internal'):
      manifest_repo_path = AbsoluteProjectPath(repo_name)
      ManifestRepository(manifest_repo_path).RepairManifestsOnDisk(branches)
      git.RunGit(
          manifest_repo_path,
          ['commit', '-a', '-m',
           'Manifests point to branch %s.' % self.name])

  def _BumpVersion(self, which):
    """Increment version in chromeos_version.sh and commit it.

    Args:
      which: Which version should be incremented. One of
          'chrome_branch', 'build', 'branch, 'patch'.
    """
    message = 'Bump %s number after creating branch %s.' % (which, self.name)
    logging.info(message)
    new_version = ReadVersionInfo(incr_type=which)
    new_version.IncrementVersion()
    new_version.UpdateVersionFile(message, dry_run=True)

  def ReportCreateSuccess(self):
    """Report branch creation successful. Behavior depends on branch type."""
    pass

  def Create(self, push=False, force=False):
    """Creates a new branch from the given version.

    Args:
      kind: The kind of branch to create (e.g., firmware).
      version: The manifest version off which to branch.
      push: Whether to push the new branch to remote.
      force: Whether or not to overwrite an existing branch.
    """
    if push or force:
      raise NotImplementedError('--push and --force unavailable.')
    branches = self._MapBranchableProjects(self._CreateProjectBranch)
    self._RepairManifestRepositories(branches)
    self._BumpVersion(WhichVersionShouldBump())
    self.ReportCreateSuccess()

  def Rename(self, push=False, force=False):
    """Rename this branch.

    Args:
      push: Whether to push the new branch to remote.
      force: Whether or not to overwrite an existing branch.
    """
    if push or force:
      raise NotImplementedError('--push and --force unavailable.')
    branches = self._MapBranchableProjects(self._RenameProjectBranch)
    self._RepairManifestRepositories(branches)

  def Delete(self, push=False, force=False):
    """Delete this branch.

    Args:
      push: Whether to push the deletion to remote.
      force: Are you *really* sure you want to delete this branch on remote?
    """
    if push or force:
      raise NotImplementedError('--push and --force unavailable.')
    self._MapBranchableProjects(self._DeleteProjectBranch)


class StandardBranch(Branch):
  """Branch with a standard name and reporting for branch operations."""

  def __init__(self, prefix, manifest):
    """Determine the name for this branch.

    By convention, standard branch names must end with the version from which
    they were created followed by '.B'.

    For example:
      - A branch created from 1.0.0 must end with -1.B
      - A branch created from 1.2.0 must end with -1-2.B

    Args:
      prefix: Any tag that describes the branch type (e.g. 'firmware').
      manifest: The currently synced manifest.
    """
    vinfo = ReadVersionInfo()
    name = '%s-%s' % (prefix, vinfo.build_number)
    if int(vinfo.branch_build_number):
      name += '.%s' % vinfo.branch_build_number
    name += '.B'
    super(StandardBranch, self).__init__(name, manifest)


class ReleaseBranch(StandardBranch):
  """Represents a release branch."""

  def __init__(self, manifest):
    super(ReleaseBranch, self).__init__(
        'release-R%s' % ReadVersionInfo().chrome_branch, manifest)

  def ReportCreateSuccess(self):
    # When a release branch has been successfully created, we report it by
    # bumping the milestone on the source branch (usually master).
    chromiumos_overlay_source = self.manifest.GetUniqueProject(
        'chromiumos/overlays/chromiumos-overlay')
    with CheckoutManager(chromiumos_overlay_source):
      self._BumpVersion('chrome_branch')


class FactoryBranch(StandardBranch):
  """Represents a factory branch."""

  def __init__(self, manifest):
    # TODO(evanhernandez): Allow adding device to branch name.
    super(FactoryBranch, self).__init__('factory', manifest)


class FirmwareBranch(StandardBranch):
  """Represents a firmware branch."""

  def __init__(self, manifest):
    # TODO(evanhernandez): Allow adding device to branch name.
    super(FirmwareBranch, self).__init__('firmware', manifest)


class StabilizeBranch(StandardBranch):
  """Represents a factory branch."""

  def __init__(self, manifest):
    super(StabilizeBranch, self).__init__('stabilize', manifest)


@command.CommandDecorator('branch')
class BranchCommand(command.CliCommand):
  """Create, delete, or rename a branch of chromiumos.

  Branch creation implies branching all git repositories under chromiumos and
  then updating metadata on the new branch and occassionally the source branch.

  Metadata is updated as follows:
    1. The new branch's manifest is repaired to point to the new branch.
    2. Chrome OS version increments on new branch (e.g., 4230.0.0 -> 4230.1.0).
    3. If the new branch is a release branch, Chrome major version increments
       the on source branch (e.g., R70 -> R71).

  Performing any of these operations remotely requires special permissions.
  Please see go/cros-release-faq for details on obtaining those permissions.
  """

  EPILOG = """
Create Examples:
  cros branch create 11030.0.0 --factory
  cros branch --force --push create 11030.0.0 --firmware
  cros branch create 11030.0.0 --custom my-custom-branch

Rename Examples:
  cros branch rename release-10509.0.B release-10508.0.B
  cros branch --force --push rename release-10509.0.B release-10508.0.B

Delete Examples:
  cros branch delete release-10509.0.B
  cros branch --force --push delete release-10509.0.B
"""

  @classmethod
  def AddParser(cls, parser):
    """Add parser arguments."""
    super(BranchCommand, cls).AddParser(parser)

    # Common flags.
    parser.add_argument(
        '--push',
        dest='push',
        action='store_true',
        help='Push branch modifications to remote repos. '
        'Before setting this flag, ensure that you have the proper '
        'permissions and that you know what you are doing. Ye be warned.')
    parser.add_argument(
        '--force',
        dest='force',
        action='store_true',
        help='Required for any remote operation that would delete an existing '
        'branch.')

    # Create subcommand and flags.
    subparser = parser.add_subparsers(dest='subcommand')
    create_parser = subparser.add_parser(
        'create', help='Create a branch from a specified maniefest version.')
    create_parser.add_argument(
        'version', help="Manifest version to branch off, e.g. '10509.0.0'.")

    create_group = create_parser.add_argument_group(
        'Branch Type',
        description='You must specify the type of the new branch. '
        'This affects how manifest metadata is updated and how '
        'the branch is named (if not specified manually).')
    create_ex_group = create_group.add_mutually_exclusive_group(required=True)
    create_ex_group.add_argument(
        '--release',
        dest='cls',
        action='store_const',
        const=ReleaseBranch,
        help='The new branch is a release branch. '
        "Named as 'release-R<Milestone>-<Major Version>.B'.")
    create_ex_group.add_argument(
        '--factory',
        dest='cls',
        action='store_const',
        const=FactoryBranch,
        help='The new branch is a factory branch. '
        "Named as 'factory-<Major Version>.B'.")
    create_ex_group.add_argument(
        '--firmware',
        dest='cls',
        action='store_const',
        const=FirmwareBranch,
        help='The new branch is a firmware branch. '
        "Named as 'firmware-<Major Version>.B'.")
    create_ex_group.add_argument(
        '--stabilize',
        dest='cls',
        action='store_const',
        const=StabilizeBranch,
        help='The new branch is a minibranch. '
        "Named as 'stabilize-<Major Version>.B'.")
    create_ex_group.add_argument(
        '--custom',
        dest='name',
        help='Custom branch type with arbitrary name. Beware: custom '
             'branches are dangerous. This tool depends on branch naming '
             'to know when a version has already been branched. Therefore, '
             'it can provide NO validation with custom branches. This has '
             'broken release builds in the past.')

    # Rename subcommand and flags.
    rename_parser = subparser.add_parser('rename', help='Rename a branch.')
    rename_parser.add_argument('old', help='Branch to rename.')
    rename_parser.add_argument('new', help='New name for the branch.')

    # Delete subcommand and flags.
    delete_parser = subparser.add_parser('delete', help='Delete a branch.')
    delete_parser.add_argument('branch', help='Name of the branch to delete.')

  def Run(self):
    # TODO(evanhernandez): If branch a operation is interrupted, some artifacts
    # might be left over. We should check for this.
    if self.options.subcommand == 'create':
      manifest = SyncVersion(self.options.version)
      branch = (Branch(self.options.name, manifest) if self.options.name else
                self.options.cls(manifest))
      branch.Create(push=self.options.push, force=self.options.force)
    elif self.options.subcommand == 'rename':
      manifest = SyncBranch(self.options.old)
      Branch(self.options.new, manifest).Rename(
          push=self.options.push,
          force=self.options.force)
    elif self.options.subcommand == 'delete':
      manifest = SyncBranch(self.options.branch)
      Branch(self.options.branch, manifest).Delete(
          push=self.options.push,
          force=self.options.force)
    else:
      raise BranchError('Unrecognized option.')
