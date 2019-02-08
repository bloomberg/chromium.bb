# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Command for managing branches of chromiumos.

See go/cros-release-faq for information on types of branches, branching
frequency, naming conventions, etc.
"""

from __future__ import print_function

import collections
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


# A ProjectBranch is, simply, a git branch on a project.
#
# Fields:
#  - project: The repo_manifest.Project associated with the git branch.
#  - branch: The name of the git branch.
ProjectBranch = collections.namedtuple('ProjectBranch', ['project', 'branch'])


class BranchError(Exception):
  """Raised whenever any branch operation fails."""


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


class CheckoutManager(object):
  """Context manager for checking out a chromiumos project to a revision.

  This manager handles fetching and checking out to a project's remote ref, as
  specified in the manifest. It also ensures that the local checkout always
  returns to its initial state once the context exits.
  """

  def __init__(self, checkout, project):
    """Determine whether project needs to be checked out.

    Args:
      checkout: The CrosCheckout containing the project.
      project: A repo_manifest.Project to be checked out.
    """
    self._path = checkout.AbsoluteProjectPath(project)
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

  def __init__(self, checkout, project):
    self._checkout = checkout
    self._project = project

  def AbsoluteManifestPath(self, path):
    """Returns the full path to the manifest.

    Args:
      path: Relative path to the manifest.

    Returns:
      Full path to the manifest.
    """
    return self._checkout.AbsoluteProjectPath(self._project, path)

  def ReadManifest(self, path):
    """Read the manifest at the given path.

    Args:
      path: Path to the manifest.

    Returns:
      repo_manifest.Manifest object.
    """
    return repo_manifest.Manifest.FromFile(
        path,
        allow_unsupported_features=True)

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
      path = pending.pop()
      if path in found:
        continue
      found.add(path)
      manifest = self.ReadManifest(self.AbsoluteManifestPath(path))
      pending.extend([inc.name for inc in manifest.Includes()])
    return found

  def RepairManifest(self, path, branches_by_path):
    """Reads the manifest at the given path and repairs it in memory.

    Because humans rarely read branched manifests, this function optimizes for
    code readability and explicitly sets revision on every project in the
    manifest, deleting any defaults.

    Args:
      path: Path to the manifest, relative to the manifest project root.
      branches_by_path: Dict mapping project paths to branch names.

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
      if CanBranchProject(project):
        branch = branches_by_path[project.Path()]
        project.revision = git.NormalizeRef(branch)
      elif CanPinProject(project):
        project.revision = self._checkout.GitRevision(project)
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
      branches: List a ProjectBranches for each branched project.
    """
    manifest_names = self.ListManifests(
        [constants.DEFAULT_MANIFEST, constants.OFFICIAL_MANIFEST])
    branches_by_path = {project.Path(): branch for project, branch in branches}
    for manifest_name in manifest_names:
      manifest_path = self.AbsoluteManifestPath(manifest_name)
      logging.info('Repairing manifest file %s', manifest_path)
      manifest = self.RepairManifest(manifest_path, branches_by_path)
      manifest.Write(manifest_path)


class CrosCheckout(object):
  """Represents a checkout of chromiumos on disk."""

  def __init__(self, root, manifest=None, repo_url=None, manifest_url=None):
    """Read the checkout manifest.

    Args:
      root: The repo root.
      manifest: The checkout manifest. Read from `repo manifest` if None.
      repo_url: Repo repository URL. Uses default googlesource repo if None.
      manifest_url: Manifest repository URL. Uses manifest-internal if None.
    """
    self.root = root
    self.manifest = manifest or repo_util.Repository(root).Manifest()
    self.repo_url = repo_url
    self.manifest_url = manifest_url

  def _Sync(self, manifest_args):
    """Run repo_sync_manifest command and return the full synced manifest.

    Args:
      manifest_args: List of args for manifest group of repo_sync_manifest.

    Returns:
      The full synced manifest as a repo_manifest.Manifest.
    """
    cmd = [os.path.join(constants.CHROMITE_DIR, 'scripts/repo_sync_manifest'),
           '--repo-root', self.root] + manifest_args
    if self.repo_url:
      cmd += ['--repo-url', self.repo_url]
    if self.manifest_url:
      cmd += ['--manifest-url', self.manifest_url]
    cros_build_lib.RunCommand(cmd, print_cmd=True)
    self.manifest = repo_util.Repository(self.root).Manifest()

  def SyncBranch(self, branch):
    """Sync to the given branch and return the synced manifest.

    Args:
      branch: Name of branch to sync to.

    Returns:
      The synced manifest as a repo_manifest.Manifest.
    """
    return self._Sync(['--branch', branch])

  def SyncVersion(self, version):
    """Sync to the given version and return the synced manifest.

    Args:
      version: Version string to sync to.

    Returns:
      The synced manifest as a repo_manifest.Manifest.
    """
    site_params = config_lib.GetSiteParams()
    return self._Sync([
        '--manifest-versions-int',
        self.AbsolutePath(site_params.INTERNAL_MANIFEST_VERSIONS_PATH),
        '--manifest-versions-ext',
        self.AbsolutePath(site_params.EXTERNAL_MANIFEST_VERSIONS_PATH),
        '--version', version
    ])

  def SyncFile(self, path):
    """Sync to the given manifest file and return the synced manifest.

    Args:
      path: Path to the manifest file.

    Returns:
      The synced manifest as a repo_manifest.Manifest.
    """
    return self._Sync(['--manifest-file', path])

  def ReadVersion(self, **kwargs):
    """Returns VersionInfo for the current checkout."""
    return manifest_version.VersionInfo.from_repo(self.root, **kwargs)

  def BumpVersion(self, which, message):
    """Increment version in chromeos_version.sh and commit it.

    Args:
      which: Which version should be incremented. One of
          'chrome_branch', 'build', 'branch, 'patch'.
      message: The commit message for the version bump.
    """
    new_version = self.ReadVersion(incr_type=which)
    new_version.IncrementVersion()
    new_version.UpdateVersionFile(message, dry_run=True)

  def AbsolutePath(self, *args):
    """Joins the path components with the repo root.

    Args:
      *paths: Arbitrary relative path components, e.g. 'chromite/'

    Returns:
      The absolute checkout path.
    """
    return os.path.join(self.root, *args)

  def AbsoluteProjectPath(self, project, *args):
    """Joins the path components to the project's root.

    Args:
      project: The repo_manifest.Project in question.
      *args: Arbitrary relative path components.

    Returns:
      The joined project path.
    """
    return self.AbsolutePath(project.Path(), *args)

  def RunGit(self, project, cmd):
    """Run a git command inside the given project.

    Args:
      project: repo_manifest.Project to run the command in.
      cmd: Command as a list of arguments. Callers should exclude 'git'.
    """
    git.RunGit(self.AbsoluteProjectPath(project), cmd, print_cmd=True)

  def GitBranch(self, project):
    """Returns the project's current branch on disk.

    Args:
      project: The repo_manifest.Project in question.
    """
    return git.GetCurrentBranch(self.AbsoluteProjectPath(project))

  def GitRevision(self, project):
    """Return the project's current git revision on disk.

    Args:
      project: The repo_manifest.Project in question.

    Returns:
      Git revision as a string.
    """
    return git.GetGitRepoRevision(self.AbsoluteProjectPath(project))


class Branch(object):
  """Represents a branch of chromiumos, which may or may not exist yet.

  Note that all local branch operations assume the current checkout is
  synced to the correct version.
  """

  def __init__(self, name, checkout):
    """Cache various configuration used by all branch operations.

    Args:
      name: The name of the branch.
      checkout: The synced CrosCheckout.
    """
    self.name = name
    self.checkout = checkout

  def _ProjectBranchName(self, branch, project):
    """Determine's the git branch name for the project.

    Args:
      branch: The base branch name.
      project: The repo_manfest.Project in question.

    Returns:
      The branch name for the project.
    """
    # If project has only one checkout, the base branch name is fine.
    checkouts = [p.name for p in self.checkout.manifest.Projects()]
    if checkouts.count(project.name) == 1:
      return branch
    # Otherwise, the project branch name needs a suffix. We append its
    # upstream or revision to distinguish it from other checkouts.
    suffix = git.StripRefs(project.upstream or project.Revision())
    return '%s-%s' % (branch, suffix)

  def _ProjectBranches(self, branch):
    """Return a list of ProjectBranches: one for each branchable project.

    Args:
      branch: The base branch name.
    """
    return [
        ProjectBranch(project, self._ProjectBranchName(branch, project)) for
        project in filter(CanBranchProject, self.checkout.manifest.Projects())
    ]

  def _CreateLocalBranches(self, branches):
    """Create git branches for all branchable projects in the local checkout.

    The branch uses the HEAD commit as the branch point.

    Args:
      branches: List of ProjectBranches to create.
    """
    for project, branch in branches:
      self.checkout.RunGit(project, ['checkout', '-B', branch])

  def _DeleteLocalBranches(self, branches):
    """Delete the given project branches in the local checkout.

    Args:
      branches: List of ProjectBranches to delete.
    """
    for project, branch in branches:
      self.checkout.RunGit(project, ['branch', '-D', branch])

  def _RepairManifestRepositories(self, branches):
    """Repair all manifests in all manifest repositories on current branch.

    Args:
      branches: List of ProjectBranches describing the repairs needed.
    """
    for project_name in config_lib.GetSiteParams().MANIFEST_PROJECTS:
      manifest_project = self.checkout.manifest.GetUniqueProject(project_name)
      manifest_repo = ManifestRepository(self.checkout, manifest_project)
      manifest_repo.RepairManifestsOnDisk(branches)
      self.checkout.RunGit(
          manifest_project,
          ['commit', '-a', '-m',
           'Manifests point to branch %s.' % self.name])

  def _WhichVersionShouldBump(self):
    """Returns which version is incremented by builds on a new branch."""
    vinfo = self.checkout.ReadVersion()
    if int(vinfo.patch_number):
      raise BranchError('Cannot bump version with nonzero patch number.')
    return 'patch' if int(vinfo.branch_build_number) else 'branch'

  def _PushBranchesToRemote(self, branches, force=False):
    """Push state of local git branches to remote.

    Args:
      branches: List of ProjectBranches to push.
      force: Whether or not to overwrite existing branches on the remote.
    """
    for project, branch in branches:
      branch = git.NormalizeRef(branch)

      # We push the local branch to the same ref on the remote.
      # So the refspec should look like 'refs/heads/branch:refs/heads/branch'.
      refspec = '%s:%s' % (branch, branch)
      remote = project.Remote().GitName()
      cmd = ['push', remote, refspec]
      if force:
        cmd += ['--force']

      # TODO(evanhernandez): Add check to see if branch exists.
      self.checkout.RunGit(project, cmd)

  def _DeleteBranchesOnRemote(self, branches):
    """Deletes this branch for all projects on the remote.

    Args:
      branches: List of ProjectBranches to delete on remote.
    """
    for project, branch in branches:
      self.checkout.RunGit(
          project,
          ['push', project.Remote().GitName(),
           '--delete', git.NormalizeRef(branch)])

  def Create(self, push=False, force=False):
    """Creates a new branch from the given version.

    Branches are always created locally, even when push is true.

    Args:
      push: Whether to push the new branch to remote.
      force: Whether or not to overwrite an existing branch.
    """
    branches = self._ProjectBranches(self.name)
    self._CreateLocalBranches(branches)
    self._RepairManifestRepositories(branches)
    which_version = self._WhichVersionShouldBump()
    self.checkout.BumpVersion(
        which_version,
        'Bump %s number after creating branch %s.' % (which_version, self.name))

    # Only once every step has succeeded do we push a new branch.
    # This is as close to atomicity as we can get with GOB.
    if push:
      self._PushBranchesToRemote(branches, force=force)

  def Rename(self, original, push=False, force=False):
    """Create this branch by renaming some other branch.

    There is no way to atomically rename a remote branch. Therefore, this
    method creates a new branch and then deletes the original.

    Args:
      original: Name of the original branch.
      push: Whether to push the new branch to remote.
      force: Whether or not to overwrite an existing branch.
    """
    new_branches = self._ProjectBranches(self.name)
    self._CreateLocalBranches(new_branches)
    self._RepairManifestRepositories(new_branches)

    old_branches = self._ProjectBranches(original)
    self._DeleteLocalBranches(old_branches)

    if push:
      self._PushBranchesToRemote(new_branches, force=force)
      self._DeleteBranchesOnRemote(old_branches)

  def Delete(self, push=False, force=False):
    """Delete this branch.

    Args:
      push: Whether to push the deletion to remote.
      force: Are you *really* sure you want to delete this branch on remote?
    """
    if push and not force:
      raise BranchError('Must set --force to delete remote branches.')
    branches = self._ProjectBranches(self.name)
    self._DeleteLocalBranches(branches)
    if push:
      self._DeleteBranchesOnRemote(branches)


class StandardBranch(Branch):
  """Branch with a standard name and reporting for branch operations."""

  def __init__(self, prefix, checkout):
    """Determine the name for this branch.

    By convention, standard branch names must end with the version from which
    they were created followed by '.B'.

    For example:
      - A branch created from 1.0.0 must end with -1.B
      - A branch created from 1.2.0 must end with -1-2.B

    Args:
      prefix: Any tag that describes the branch type (e.g. 'firmware').
      checkout: The synced CrosCheckout.
    """
    vinfo = checkout.ReadVersion()
    name = '%s-%s' % (prefix, vinfo.build_number)
    if int(vinfo.branch_build_number):
      name += '.%s' % vinfo.branch_build_number
    name += '.B'
    super(StandardBranch, self).__init__(name, checkout)


class ReleaseBranch(StandardBranch):
  """Represents a release branch."""

  def __init__(self, checkout):
    super(ReleaseBranch, self).__init__(
        'release-R%s' % checkout.ReadVersion().chrome_branch, checkout)

  def Create(self, push=False, force=False):
    super(ReleaseBranch, self).Create(push=push, force=force)
    # When a release branch has been successfully created, we report it by
    # bumping the milestone on the source branch (usually master).
    # We intentionally do this *after* the new branch has been pushed.
    chromiumos_overlay = self.checkout.manifest.GetUniqueProject(
        'chromiumos/overlays/chromiumos-overlay')
    with CheckoutManager(self.checkout, chromiumos_overlay):
      self.checkout.BumpVersion(
          'chrome_branch',
          'Bump milestone after creating release branch %s.' % self.name)
      # TODO(evanhernandez): Push this change to remote.


class FactoryBranch(StandardBranch):
  """Represents a factory branch."""

  def __init__(self, checkout):
    # TODO(evanhernandez): Allow adding device to branch name.
    super(FactoryBranch, self).__init__('factory', checkout)


class FirmwareBranch(StandardBranch):
  """Represents a firmware branch."""

  def __init__(self, checkout):
    # TODO(evanhernandez): Allow adding device to branch name.
    super(FirmwareBranch, self).__init__('firmware', checkout)


class StabilizeBranch(StandardBranch):
  """Represents a factory branch."""

  def __init__(self, checkout):
    super(StabilizeBranch, self).__init__('stabilize', checkout)


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

    sync_group = parser.add_argument_group(
        'Sync',
        description='Arguments relating to how the checkout is synced. '
        'These options are primarily used for testing.')
    sync_group.add_argument(
        '--root',
        dest='root',
        default=constants.SOURCE_ROOT,
        help='Repo root of local checkout to branch. If not specificed, this '
        'tool will branch the checkout from which it is run.')
    sync_group.add_argument('--repo-url', help='Repo repository location.')
    sync_group.add_argument(
        '--manifest-url', help='URL of the manifest to be checked out.')

    # Create subcommand and flags.
    subparser = parser.add_subparsers(dest='subcommand')
    create_parser = subparser.add_parser(
        'create', help='Create a branch from a specified maniefest version.')

    sync_group = create_parser.add_argument_group(
        'Manifest',
        description='Which manifest should be branched?')
    sync_ex_group = sync_group.add_mutually_exclusive_group(required=True)
    sync_ex_group.add_argument(
        '--version',
        dest='version',
        help="Manifest version to branch off, e.g. '10509.0.0'.")
    sync_ex_group.add_argument(
        '--file',
        dest='file',
        help='Path to manifest file to branch off.')

    type_group = create_parser.add_argument_group(
        'Branch Type',
        description='You must specify the type of the new branch. '
        'This affects how manifest metadata is updated and how '
        'the branch is named (if not specified manually).')
    type_ex_group = type_group.add_mutually_exclusive_group(required=True)
    type_ex_group.add_argument(
        '--release',
        dest='cls',
        action='store_const',
        const=ReleaseBranch,
        help='The new branch is a release branch. '
        "Named as 'release-R<Milestone>-<Major Version>.B'.")
    type_ex_group.add_argument(
        '--factory',
        dest='cls',
        action='store_const',
        const=FactoryBranch,
        help='The new branch is a factory branch. '
        "Named as 'factory-<Major Version>.B'.")
    type_ex_group.add_argument(
        '--firmware',
        dest='cls',
        action='store_const',
        const=FirmwareBranch,
        help='The new branch is a firmware branch. '
        "Named as 'firmware-<Major Version>.B'.")
    type_ex_group.add_argument(
        '--stabilize',
        dest='cls',
        action='store_const',
        const=StabilizeBranch,
        help='The new branch is a minibranch. '
        "Named as 'stabilize-<Major Version>.B'.")
    type_ex_group.add_argument(
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
    checkout = CrosCheckout(
        self.options.root,
        repo_url=self.options.repo_url,
        manifest_url=self.options.manifest_url)

    # TODO(evanhernandez): If branch a operation is interrupted, some artifacts
    # might be left over. We should check for this.
    if self.options.subcommand == 'create':
      if self.options.file:
        checkout.SyncFile(self.options.file)
      else:
        checkout.SyncVersion(self.options.version)

      if self.options.name:
        branch = Branch(self.options.name, checkout)
      else:
        branch = self.options.cls(checkout)

      branch.Create(push=self.options.push, force=self.options.force)

    elif self.options.subcommand == 'rename':
      checkout.SyncBranch(self.options.old)
      branch = Branch(self.options.new, checkout)
      branch.Rename(
          self.options.old,
          push=self.options.push,
          force=self.options.force)

    elif self.options.subcommand == 'delete':
      checkout.SyncBranch(self.options.branch)
      branch = Branch(self.options.branch, checkout)
      branch.Delete(push=self.options.push, force=self.options.force)

    else:
      raise BranchError('Unrecognized option.')
