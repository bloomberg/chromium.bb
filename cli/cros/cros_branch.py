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
from chromite.lib import osutils
from chromite.lib import repo_manifest
from chromite.lib import repo_util
from chromite.lib import retry_util

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


class ManifestRepository(object):
  """Represents a git repository of manifest XML files."""

  def __init__(self, checkout, project):
    self._checkout = checkout
    self._project = project

  def _AbsoluteManifestPath(self, path):
    """Returns the full path to the manifest.

    Args:
      path: Relative path to the manifest.

    Returns:
      Full path to the manifest.
    """
    return self._checkout.AbsoluteProjectPath(self._project, path)

  def _ReadManifest(self, path):
    """Read the manifest at the given path.

    Args:
      path: Path to the manifest.

    Returns:
      repo_manifest.Manifest object.
    """
    return repo_manifest.Manifest.FromFile(
        path, allow_unsupported_features=True)

  def _ListManifests(self, root_manifests):
    """Finds all manifests included directly or indirectly by root manifests.

    For convenience, the returned set includes the root manifests. If any
    manifest is not found on disk, it is ignored.

    Args:
      root_manifests: Names of manifests whose includes will be traversed.

    Returns:
      Set of paths to included manifests.
    """
    pending = list(root_manifests)
    found = set()
    while pending:
      path = self._AbsoluteManifestPath(pending.pop())
      if path in found or not os.path.exists(path):
        continue
      found.add(path)
      manifest = self._ReadManifest(path)
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
    manifest = self._ReadManifest(path)

    # Delete the default revision if specified by original manifest.
    default = manifest.Default()
    if default.revision:
      del default.revision

    # Delete remote revisions if specified by original manifest.
    for remote in manifest.Remotes():
      if remote.revision:
        del remote.revision

    # Update all project revisions. Note we cannot call CanBranchProject and
    # related functions because they read the project remote, which may not
    # be defined in the current manifest file.
    for project in manifest.Projects():
      self._checkout.EnsureProject(project)
      path = project.Path()

      # If project path is in the dict, the project must've been branched
      if path in branches_by_path:
        project.revision = git.NormalizeRef(branches_by_path[path])

      # Otherwise, check if project is explicitly TOT.
      elif BranchMode(project) == constants.MANIFEST_ATTR_BRANCHING_TOT:
        project.revision = git.NormalizeRef('master')

      # If not, it's pinned.
      else:
        project.revision = self._checkout.GitRevision(project)

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
    logging.notice('Repairing manifest project %s.', self._project.name)
    manifest_paths = self._ListManifests(
        [constants.DEFAULT_MANIFEST, constants.OFFICIAL_MANIFEST])
    branches_by_path = {project.Path(): branch for project, branch in branches}
    for manifest_path in manifest_paths:
      logging.notice('Repairing manifest file %s', manifest_path)
      manifest = self.RepairManifest(manifest_path, branches_by_path)
      manifest.Write(manifest_path)


class CrosCheckout(object):
  """Represents a checkout of chromiumos on disk."""

  def __init__(self, root, manifest=None, manifest_url=None, repo_url=None):
    """Read the checkout manifest.

    Args:
      root: The repo root.
      manifest: The checkout manifest. Read from `repo manifest` if None.
      manifest_url: Manifest repository URL. repo_sync_manifest sets default.
      repo_url: Repo repository URL. repo_sync_manifest sets default.
    """
    self.root = root
    self.manifest = manifest or repo_util.Repository(root).Manifest()
    self.manifest_url = manifest_url
    self.repo_url = repo_url

  @staticmethod
  def TempRoot():
    """Returns an osutils.TempDir for a checkout. Not inlined for testing."""
    return osutils.TempDir(prefix='cros-branch-')

  @classmethod
  def Initialize(cls, root, manifest_url, repo_url=None, repo_branch=None):
    """Initialize the checkout if necessary. Otherwise a no-op.

    Args:
      root: The repo root.
      manifest_url: Manifest repository URL.
      repo_url: Repo repository URL. Uses default googlesource repo if None.
      repo_branch: Repo repository branch.
    """
    osutils.SafeMakedirs(root)
    if git.FindRepoCheckoutRoot(root) is None:
      logging.notice('Will initialize checkout %s for this run.', root)
      repo_util.Repository.Initialize(
          root, manifest_url, repo_url=repo_url, repo_branch=repo_branch)
    else:
      logging.notice('Will use existing checkout %s for this run.', root)
    return cls(root, manifest_url=manifest_url, repo_url=repo_url)

  def _Sync(self, manifest_args):
    """Run repo_sync_manifest command.

    Args:
      manifest_args: List of args for manifest group of repo_sync_manifest.
    """
    cmd = [
        os.path.join(constants.CHROMITE_DIR, 'scripts/repo_sync_manifest'),
        '--repo-root', self.root
    ] + manifest_args
    if self.repo_url:
      cmd += ['--repo-url', self.repo_url]
    if self.manifest_url:
      cmd += ['--manifest-url', self.manifest_url]
    cros_build_lib.RunCommand(cmd, print_cmd=True)
    self.manifest = repo_util.Repository(self.root).Manifest()

  def SyncBranch(self, branch):
    """Sync to the given branch.

    Args:
      branch: Name of branch to sync to.
    """
    logging.notice('Syncing checkout %s to branch %s.', self.root, branch)
    self._Sync(['--branch', branch])

  def SyncVersion(self, version):
    """Sync to the given manifest version.

    Args:
      version: Version string to sync to.
    """
    logging.notice('Syncing checkout %s to version %s.', self.root, version)
    site_params = config_lib.GetSiteParams()
    self._Sync([
        '--manifest-versions-int',
        self.AbsolutePath(site_params.INTERNAL_MANIFEST_VERSIONS_PATH),
        '--manifest-versions-ext',
        self.AbsolutePath(site_params.EXTERNAL_MANIFEST_VERSIONS_PATH),
        '--version', version
    ])

  def SyncFile(self, path):
    """Sync to the given manifest file.

    Args:
      path: Path to the manifest file.
    """
    logging.notice('Syncing checkout %s to manifest %s.', self.root, path)
    # SyncFile uses repo sync instead of repo_sync_manifest because
    # repo_sync_manifest sometimes corrupts .repo/manifest.xml when
    # syncing to a file. See crbug.com/973106.
    cmd = ['repo', 'sync', '--manifest-name', os.path.abspath(path)]
    cros_build_lib.RunCommand(cmd, cwd=self.root, print_cmd=True)
    self.manifest = repo_util.Repository(self.root).Manifest()

  def ReadVersion(self, **kwargs):
    """Returns VersionInfo for the current checkout."""
    return manifest_version.VersionInfo.from_repo(self.root, **kwargs)

  def BumpVersion(self, which, branch, message, dry_run=True, fetch=False):
    """Increment version in chromeos_version.sh and commit it.

    Args:
      which: Which version should be incremented. One of
          'chrome_branch', 'build', 'branch, 'patch'.
      branch: The branch to push to.
      message: The commit message for the version bump.
      dry_run: Whether to use git --dry-run.
      fetch: Whether to fetch and checkout to the given branch.
    """
    logging.notice(message)

    chromiumos_overlay = self.manifest.GetUniqueProject(
        'chromiumos/overlays/chromiumos-overlay')
    remote = chromiumos_overlay.Remote().GitName()
    ref = git.NormalizeRef(branch)

    if fetch:
      self.RunGit(chromiumos_overlay, ['fetch', remote, ref])
      self.RunGit(chromiumos_overlay, ['checkout', '-B', branch, 'FETCH_HEAD'])

    new_version = self.ReadVersion(incr_type=which)
    new_version.IncrementVersion()
    remote_ref = git.RemoteRef(remote, ref)
    new_version.UpdateVersionFile(message, dry_run=dry_run, push_to=remote_ref)

  def EnsureProject(self, project):
    """Checks that the project exists in the checkout.

    Args:
      project: The repo_manifest.Project in question.

    Raises:
      BranchError if project does not exist in checkout.
    """
    path = self.AbsoluteProjectPath(project)
    if not os.path.exists(path):
      raise BranchError(
          'Project %s does not exist at path %s in checkout. '
          'This likely means that manifest-internal is out of sync '
          'with manifest, and the manifest file you are branching from '
          'is corrupted.' % (project.name, path))

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

  def RunGit(self, project, cmd, retries=3):
    """Run a git command inside the given project.

    Args:
      project: repo_manifest.Project to run the command in.
      cmd: Command as a list of arguments. Callers should exclude 'git'.
      retries: Maximum number of retries for the git command.
    """
    retry_util.RetryCommand(
        git.RunGit,
        retries,
        self.AbsoluteProjectPath(project),
        cmd,
        print_cmd=True,
        sleep=2,
        log_retries=True)

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

  def BranchExists(self, project, pattern):
    """Determines if any branch exists that matches the given pattern.

    Args:
      project: The repo_manifest.Project in question.
      pattern: Branch name pattern to search for.

    Returns:
      True if a matching branch exists on the remote.
    """
    matches = git.MatchBranchName(self.AbsoluteProjectPath(project), pattern)
    return len(matches) != 0


class Branch(object):
  """Represents a branch of chromiumos, which may or may not exist yet.

  Note that all local branch operations assume the current checkout is
  synced to the correct version.
  """

  def __init__(self, checkout, name):
    """Cache various configuration used by all branch operations.

    Args:
      checkout: The synced CrosCheckout.
      name: The name of the branch.
    """
    self.checkout = checkout
    self.name = name

  def _ProjectBranchName(self, branch, project, original=None):
    """Determine's the git branch name for the project.

    Args:
      branch: The base branch name.
      project: The repo_manfest.Project in question.
      original: Original branch name to remove from the branch suffix.

    Returns:
      The branch name for the project.
    """
    # If project has only one checkout, the base branch name is fine.
    checkouts = [p.name for p in self.checkout.manifest.Projects()]
    if checkouts.count(project.name) == 1:
      return branch

    # Otherwise, the project branch name needs a suffix. We append its
    # upstream or revision to distinguish it from other checkouts.
    suffix = '-' + git.StripRefs(project.upstream or project.Revision())

    # If the revision is itself a branch, we need to strip the old branch name
    # from the suffix to keep naming consistent.
    if original:
      suffix = re.sub('^-%s-' % original, '-', suffix)

    return branch + suffix

  def _ProjectBranches(self, branch, original=None):
    """Return a list of ProjectBranches: one for each branchable project.

    Args:
      branch: The base branch name.
      original: Branch from which this branch of chromiumos stems, if any.
    """
    return [
        ProjectBranch(proj, self._ProjectBranchName(branch, proj, original))
        for proj in self.checkout.manifest.Projects()
        if CanBranchProject(proj)
    ]

  def _ValidateBranches(self, branches):
    """Validates that branches do not already exist.

    Args:
      branches: Collection of ProjectBranch objects to valdiate.

    Raises:
      BranchError if any branch exists.
    """
    logging.notice('Validating branch does not already exist.')
    for project, branch in branches:
      if self.checkout.BranchExists(project, branch):
        raise BranchError(
            'Branch %s exists for %s. '
            'Please rerun with --force to proceed.' % (branch, project.name))

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
    assert not int(vinfo.patch_number)
    return 'patch' if int(vinfo.branch_build_number) else 'branch'

  def _PushBranchesToRemote(self, branches, dry_run=True, force=False):
    """Push state of local git branches to remote.

    Args:
      branches: List of ProjectBranches to push.
      force: Whether or not to overwrite existing branches on the remote.
      dry_run: Whether or not to set --dry-run.
    """
    logging.notice('Pushing branches to remote (%s --dry-run).',
                   'with' if dry_run else 'without')
    for project, branch in branches:
      branch = git.NormalizeRef(branch)

      # The refspec should look like 'HEAD:refs/heads/branch'.
      refspec = 'HEAD:%s' % branch
      remote = project.Remote().GitName()

      cmd = ['push', remote, refspec]
      if dry_run:
        cmd.append('--dry-run')
      if force:
        cmd.append('--force')

      self.checkout.RunGit(project, cmd)

  def _DeleteBranchesOnRemote(self, branches, dry_run=True):
    """Push deletions of this branch for all projects.

    Args:
      branches: List of ProjectBranches for which to push delete.
      dry_run: Whether or not to set --dry-run.
    """
    logging.notice('Deleting old branches on remote (%s --dry-run).',
                   'with' if dry_run else 'without')
    for project, branch in branches:
      branch = git.NormalizeRef(branch)
      cmd = ['push', project.Remote().GitName(), '--delete', branch]
      if dry_run:
        cmd.append('--dry-run')
      self.checkout.RunGit(project, cmd)

  def Create(self, push=False, force=False):
    """Creates a new branch from the given version.

    Branches are always created locally, even when push is true.

    Args:
      push: Whether to push the new branch to remote.
      force: Whether or not to overwrite an existing branch.
    """
    branches = self._ProjectBranches(self.name)

    if not force:
      self._ValidateBranches(branches)

    self._RepairManifestRepositories(branches)
    self._PushBranchesToRemote(branches, dry_run=not push, force=force)

    # Must bump version last because of how VersionInfo is implemented. Sigh...
    which_version = self._WhichVersionShouldBump()
    self.checkout.BumpVersion(
        which_version,
        self.name,
        'Bump %s number after creating branch %s.' % (which_version, self.name),
        dry_run=not push)
    # Increment branch/build number for source 'master' branch.
    # manifest_version already does this for release branches.
    # TODO(@jackneus): Make this less of a hack.
    # In reality, this whole tool is being deleted pretty soon.
    if self.__class__.__name__ != 'ReleaseBranch':
      source_version = 'branch' if which_version == 'patch' else 'build'
      self.checkout.BumpVersion(
          source_version,
          'master',
          'Bump %s number for source branch after creating branch %s' %
          (source_version, self.name),
          dry_run=not push)

  def Rename(self, original, push=False, force=False):
    """Create this branch by renaming some other branch.

    There is no way to atomically rename a remote branch. Therefore, this
    method creates a new branch and then deletes the original.

    Args:
      original: Name of the original branch.
      push: Whether to push changes to remote.
      force: Whether or not to overwrite an existing branch.
    """
    new_branches = self._ProjectBranches(self.name, original=original)

    if not force:
      self._ValidateBranches(new_branches)

    self._RepairManifestRepositories(new_branches)
    self._PushBranchesToRemote(new_branches, dry_run=not push, force=force)

    old_branches = self._ProjectBranches(original, original=original)
    self._DeleteBranchesOnRemote(old_branches, dry_run=not push)

  def Delete(self, push=False, force=False):
    """Delete this branch.

    Args:
      push: Whether to push the deletion to remote.
      force: Are you *really* sure you want to delete this branch on remote?
    """
    if push and not force:
      raise BranchError('Must set --force to delete remote branches.')
    branches = self._ProjectBranches(self.name, original=self.name)
    self._DeleteBranchesOnRemote(branches, dry_run=not push)


class StandardBranch(Branch):
  """Branch with a standard name, meaning it is suffixed by version."""

  def __init__(self, checkout, *args):
    """Determine the name for this branch.

    By convention, standard branch names must end with the major version from
    which they were created, followed by '.B'.

    For example:
      - A branch created from 1.0.0 must end with -1.B
      - A branch created from 1.2.0 must end with -1-2.B

    Args:
      checkout: The synced CrosCheckout.
      *args: Additional name components, which will be joined by dashes.
    """
    vinfo = checkout.ReadVersion()
    version = '.'.join(str(comp) for comp in vinfo.VersionComponents() if comp)
    name = '-'.join([x for x in args if x] + [version]) + '.B'
    super(StandardBranch, self).__init__(checkout, name)


class ReleaseBranch(StandardBranch):
  """Represents a release branch.

  Release branches have a slightly different naming scheme. They include
  the milestone from which they were created. Example: release-R12-1.2.B.

  Additionally, creating a release branches requires updating the milestone
  (Chrome branch) in chromeos_version.sh on master.
  """

  def __init__(self, checkout, descriptor=None):
    super(ReleaseBranch, self).__init__(
        checkout, 'release', descriptor,
        'R%s' % checkout.ReadVersion().chrome_branch)

  def Create(self, push=False, force=False):
    super(ReleaseBranch, self).Create(push=push, force=force)
    # When a release branch has been successfully created, we report it by
    # bumping the milestone on the master. Note this also bumps build number
    # as a workaround for crbug.com/213075
    self.checkout.BumpVersion(
        'chrome_branch',
        'master',
        'Bump milestone after creating release branch %s.' % self.name,
        dry_run=not push,
        fetch=True)


class FactoryBranch(StandardBranch):
  """Represents a factory branch."""

  def __init__(self, checkout, descriptor=None):
    super(FactoryBranch, self).__init__(checkout, 'factory', descriptor)


class FirmwareBranch(StandardBranch):
  """Represents a firmware branch."""

  def __init__(self, checkout, descriptor=None):
    super(FirmwareBranch, self).__init__(checkout, 'firmware', descriptor)


class StabilizeBranch(StandardBranch):
  """Represents a minibranch."""

  def __init__(self, checkout, descriptor=None):
    super(StabilizeBranch, self).__init__(checkout, 'stabilize', descriptor)


@command.CommandDecorator('branch')
class BranchCommand(command.CliCommand):
  """Create, delete, or rename a branch of chromiumos.

  For details on what this tool does, see go/cros-branch.

  Performing any of these operations remotely requires special permissions.
  Please see go/cros-release-faq for details on obtaining those permissions.
  """

  EPILOG = """
Create example: firmware branch 'firmware-nocturne-11030.B'
  cros branch --push create --descriptor nocturne --version 11030.0.0 --firmware

Create example: release branch 'release-R70-11030.B'
  cros branch --push create --version 11030.0.0 --release

Create example: custom branch 'my-branch'
  cros branch --push create --version 11030.0.0 --custom my-branch

Create example: minibranch dry-run 'stabilize-test-11030.B'
  cros branch create --version 11030.0.0 --descriptor test --stabilize

Rename Examples:
  cros branch rename release-R70-10509.B release-R70-10508.B
  cros branch --push rename release-R70-10509.B release-R70-10508.B

Delete Examples:
  cros branch delete release-R70-10509.B
  cros branch --force --push delete release-R70-10509.B
"""

  @classmethod
  def AddParser(cls, parser):
    """Add parser arguments."""
    super(BranchCommand, cls).AddParser(parser)

    # Common flags.
    remote_group = parser.add_argument_group(
        'Remote options',
        description='Arguments determine how branch operations interact with '
        'remote repositories.')
    remote_group.add_argument(
        '--push',
        action='store_true',
        help='Push branch modifications to remote repos. '
        'Before setting this flag, ensure that you have the proper '
        'permissions and that you know what you are doing. Ye be warned.')
    remote_group.add_argument(
        '--force',
        action='store_true',
        help='Required for any remote operation that would delete an existing '
        'branch. Also required when trying to branch from a previously '
        'branched manifest version.')

    sync_group = parser.add_argument_group(
        'Sync options',
        description='Arguments relating to how the checkout is synced. '
        'These options are primarily used for testing.')
    sync_group.add_argument(
        '--root',
        help='Repo root of local checkout to branch. If the root does not '
        'exist, this tool will create it. If the root is not initialized, '
        'this tool will initialize it. If --root is not specificed, this '
        'tool will branch a fresh checkout in a temporary directory.')
    sync_group.add_argument(
        '--repo-url',
        help='Repo repository location. Defaults to repo '
        'googlesource URL.')
    sync_group.add_argument('--repo-branch', help='Branch to checkout repo to.')
    sync_group.add_argument(
        '--manifest-url',
        default='https://chrome-internal.googlesource.com'
        '/chromeos/manifest-internal.git',
        help='URL of the manifest to be checked out. Defaults to googlesource '
        'URL for manifest-internal.')

    # Create subcommand and flags.
    subparser = parser.add_subparsers(dest='subcommand')
    create_parser = subparser.add_parser('create', help='Create a branch.')

    name_group = create_parser.add_argument_group(
        'Name options', description='Arguments for determining branch name.')
    name_group.add_argument(
        '--descriptor',
        help='Optional descriptor for this branch. Typically, this is a build '
        'target or a device, depending on the nature of the branch. Used '
        'to generate the branch name. Cannot be used with --custom.')
    name_group.add_argument(
        '--yes',
        dest='yes',
        action='store_true',
        help='If set, disables the boolean prompt confirming the branch name.')

    manifest_group = create_parser.add_argument_group(
        'Manifest options', description='Which manifest should be branched?')
    manifest_ex_group = manifest_group.add_mutually_exclusive_group(
        required=True)
    manifest_ex_group.add_argument(
        '--version',
        help="Manifest version to branch off, e.g. '10509.0.0'."
        'You may not branch off of the same version twice unless you run '
        'with --force.')
    manifest_ex_group.add_argument(
        '--file', help='Path to manifest file to branch off.')

    kind_group = create_parser.add_argument_group(
        'Kind options',
        description='What kind of branch is this? '
        'These flags affect how manifest metadata is updated and '
        'how the branch is named.')
    kind_ex_group = kind_group.add_mutually_exclusive_group(required=True)
    kind_ex_group.add_argument(
        '--release',
        dest='cls',
        action='store_const',
        const=ReleaseBranch,
        help='The new branch is a release branch. '
        "Named as 'release-<descriptor>-R<Milestone>-<Major Version>.B'.")
    kind_ex_group.add_argument(
        '--factory',
        dest='cls',
        action='store_const',
        const=FactoryBranch,
        help='The new branch is a factory branch. '
        "Named as 'factory-<Descriptor>-<Major Version>.B'.")
    kind_ex_group.add_argument(
        '--firmware',
        dest='cls',
        action='store_const',
        const=FirmwareBranch,
        help='The new branch is a firmware branch. '
        "Named as 'firmware-<Descriptor>-<Major Version>.B'.")
    kind_ex_group.add_argument(
        '--stabilize',
        dest='cls',
        action='store_const',
        const=StabilizeBranch,
        help='The new branch is a minibranch. '
        "Named as 'stabilize-<Descriptor>-<Major Version>.B'.")
    kind_ex_group.add_argument(
        '--custom',
        dest='name',
        help='Use a custom branch type with an explicit name. '
        'WARNING: custom names are dangerous. This tool greps branch '
        'names to determine which versions have already been branched. '
        'Version validation is not possible when the naming convention '
        'is broken. Use this at your own risk.')

    # Rename subcommand and flags.
    rename_parser = subparser.add_parser('rename', help='Rename a branch.')
    rename_parser.add_argument('old', help='Branch to rename.')
    rename_parser.add_argument('new', help='New name for the branch.')

    # Delete subcommand and flags.
    delete_parser = subparser.add_parser('delete', help='Delete a branch.')
    delete_parser.add_argument('branch', help='Name of the branch to delete.')

  def _HandleCreate(self, checkout):
    """Sync to the version or file and create a branch.

    Args:
      checkout: The CrosCheckout to run commands in.
    """
    # Start with quick, immediate validations.
    if self.options.name and self.options.descriptor:
      raise BranchError('--descriptor cannot be used with --custom.')

    if self.options.version and not self.options.version.endswith('0'):
      raise BranchError('Cannot branch version from nonzero patch number.')

    # Handle sync. Unfortunately, we cannot fully validate the version until
    # we have a copy of chromeos_version.sh.
    if self.options.file:
      checkout.SyncFile(self.options.file)
    else:
      checkout.SyncVersion(self.options.version)

    # Now to validate the version. First, double check that the checkout
    # has a zero patch number in case we synced from file.
    vinfo = checkout.ReadVersion()
    if int(vinfo.patch_number):
      raise BranchError('Cannot branch version with nonzero patch number.')

    # Second, check that we did not already branch from this version.
    # manifest-internal serves as the sentinel project.
    manifest_internal = checkout.manifest.GetUniqueProject(
        'chromeos/manifest-internal')
    pattern = '.*-%s\\.B$' % '\\.'.join(
        str(comp) for comp in vinfo.VersionComponents() if comp)
    if (checkout.BranchExists(manifest_internal, pattern) and
        not self.options.force):
      raise BranchError(
          'Already branched %s. Please rerun with --force if you wish to '
          'proceed.' % vinfo.VersionString())

    # Determine if we are creating a custom branch or a standard branch.
    if self.options.cls:
      branch = self.options.cls(checkout, self.options.descriptor)
    else:
      branch = Branch(checkout, self.options.name)

    # Finally, double check the name with the user.
    proceed = self.options.yes or cros_build_lib.BooleanPrompt(
        prompt='New branch will be named %s. Continue?' % branch.name,
        default=False)

    if proceed:
      branch.Create(push=self.options.push, force=self.options.force)
      logging.notice('Successfully created branch %s.', branch.name)
    else:
      logging.notice('Aborted branch creation.')

  def _HandleRename(self, checkout):
    """Sync to the branch and rename it.

    Args:
      checkout: The CrosCheckout to run commands in.
    """
    checkout.SyncBranch(self.options.old)
    branch = Branch(checkout, self.options.new)
    branch.Rename(
        self.options.old, push=self.options.push, force=self.options.force)
    logging.notice('Successfully renamed branch %s to %s.', self.options.old,
                   self.options.new)

  def _HandleDelete(self, checkout):
    """Sync to the branch and delete it.

    Args:
      checkout: The CrosCheckout to run commands in.
    """
    checkout.SyncBranch(self.options.branch)
    branch = Branch(checkout, self.options.branch)
    branch.Delete(push=self.options.push, force=self.options.force)
    logging.notice('Successfully deleted branch %s.', self.options.branch)

  def _RunInCheckout(self, root):
    """Run cros branch in a checkout at the given root.

    Args:
      root: Path to checkout.
    """
    checkout = CrosCheckout.Initialize(
        root,
        self.options.manifest_url,
        repo_url=self.options.repo_url,
        repo_branch=self.options.repo_branch)
    handlers = {
        'create': self._HandleCreate,
        'rename': self._HandleRename,
        'delete': self._HandleDelete
    }
    handlers[self.options.subcommand](checkout)

  def Run(self):
    if self.options.root:
      self._RunInCheckout(self.options.root)
    else:
      with CrosCheckout.TempRoot() as root:
        self._RunInCheckout(root)
        logging.notice('Cleaning up...')
