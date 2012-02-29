# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Repository module to handle different types of repositories the Builders use.
"""

import collections
import constants
import filecmp
import itertools
import logging
import os
import re
import shutil
import tempfile

from chromite.buildbot import portage_utilities, configure_repo
from chromite.lib import cros_build_lib as cros_lib
from chromite.lib import rewrite_git_alternates

# File that marks a buildroot as being used by a trybot
_TRYBOT_MARKER = '.trybot'

_DEFAULT_SYNC_JOBS = 4

class SrcCheckOutException(Exception):
  """Exception gets thrown for failure to sync sources"""
  pass


def InARepoRepository(directory, require_project=False):
  """Returns True if directory is part of a repo checkout.

  Args:
    directory: Path to check.
    require_project: Whether to require that directory is inside a valid
     project in the repo root.
  """
  directory = os.path.abspath(directory)
  while not os.path.isdir(directory):
    directory = os.path.dirname(directory)

  if require_project:
    cmd = ['repo', 'forall', '.', '-c', 'true']
  else:
    cmd = ['repo']

  output = cros_lib.RunCommand(
      cmd, error_code_ok=True, redirect_stdout=True, redirect_stderr=True,
      cwd=directory, print_cmd=False)
  return output.returncode == 0


def IsARepoRoot(root):
  """Returns True if directory is the root of a repo checkout."""
  # Check for the underlying git-repo checkout.  If it exists, it's
  # definitely the repo root.  If it doesn't, it may be an aborted
  # checkout- either way it isn't usable.
  repo_dir = os.path.join(root, '.repo', 'repo')
  return os.path.isdir(repo_dir)


def RepoSyncUsingSSH(git_repo):
  """Fetch the latest code for the given directory using repo sync over ssh.

  Args:
    git_repo: Git repository to sync.
  """
  cros_lib.RunCommand(['git',
                       'config',
                       'url.%s.insteadof' % constants.GERRIT_SSH_URL,
                       constants.GIT_HTTP_URL], cwd=git_repo)
  try:
    cros_lib.RunCommand(['repo', 'sync', '.'], cwd=git_repo)
  finally:
    cros_lib.RunCommand(['git',
                         'config',
                         '--unset',
                         'url.%s.insteadof' % constants.GERRIT_SSH_URL],
                         cwd=git_repo)



def CloneGitRepo(working_dir, repo_url):
  """Clone given git repo
  Args:
    repo_url: git repo to clone
    repo_dir: location where it should be cloned to
  """
  if not os.path.exists(working_dir): os.makedirs(working_dir)
  cros_lib.RunCommand(['git', 'clone', repo_url, working_dir],
                      redirect_stderr=True, redirect_stdout=True,
                      cwd=working_dir)


def GetTrybotMarkerPath(buildroot):
  """Get path to trybot marker file given the buildroot."""
  return os.path.join(buildroot, _TRYBOT_MARKER)


def CreateTrybotMarker(buildroot):
  """Create the file that identifies a buildroot as being used by a trybot."""
  open(GetTrybotMarkerPath(buildroot), 'w').close()


def ClearBuildRoot(buildroot):
  """Remove and recreate the buildroot while preserving the trybot marker."""
  trybot_root = os.path.exists(GetTrybotMarkerPath(buildroot))
  cros_lib.SudoRunCommand(['rm', '-rf', buildroot], error_ok=True)
  os.makedirs(buildroot)
  if trybot_root:
    CreateTrybotMarker(buildroot)


def DisableInteractiveRepoManifestCommand():
  """Set the PAGER repo manifest uses to be non-interactive."""
  os.environ['PAGER'] = 'cat'


class RepoRepository(object):
  """ A Class that encapsulates a repo repository.
  Args:
    repo_url: gitserver URL to fetch repo manifest from.
    directory: local path where to checkout the repository.
    branch: Branch to check out the manifest at.
    stable_sync:  Sync to latest stable commit rather than ToT.
  """
  DEFAULT_MANIFEST = 'default'
  # Use our own repo, in case android.kernel.org (the default location) is down.
  _INIT_CMD = ['repo', 'init', '--repo-url', constants.REPO_URL]

  def __init__(self, repo_url, directory, branch=None, stable_sync=False,
               referenced_repo=None, manifest=None, depth=None):
    self.repo_url = repo_url
    self.directory = directory
    self.branch = branch
    self._stable_sync = stable_sync

    # It's perfectly acceptable to pass in a reference pathway that isn't
    # usable.  Detect it, and suppress the setting so that any depth
    # settings aren't disabled due to it.
    if referenced_repo is not None and not IsARepoRoot(referenced_repo):
      referenced_repo = None
    self._referenced_repo = referenced_repo
    self._manifest = manifest
    self._initialized = IsARepoRoot(self.directory)

    if not self._initialized and InARepoRepository(self.directory):
      raise ValueError('Given directory %s is not the root of a repository.'
                       % self.directory)

    if depth is not None:
      depth = int(depth)
      if referenced_repo is not None or self._initialized:
        # Depth can only be enforced during initialization.
        depth = None
    self._depth = depth

  def Initialize(self, extra_args=(), force=False):
    """Initializes a repository."""
    if self._initialized and not force:
      raise SrcCheckOutException('Repo already initialized.')

    # Base command.
    init_cmd = self._INIT_CMD + ['--manifest-url', self.repo_url]
    if self._referenced_repo:
      init_cmd.extend(['--reference', self._referenced_repo])
    if self._manifest:
      init_cmd.extend(['--manifest-name', self._manifest])
    if self._depth is not None:
      init_cmd.extend(['--depth', str(self._depth)])
    init_cmd.extend(extra_args)

    # Handle branch / manifest options.
    if self.branch: init_cmd.extend(['--manifest-branch', self.branch])
    cros_lib.RunCommand(init_cmd, cwd=self.directory, input='\n\ny\n')
    self._initialized = True

  @property
  def _ManifestConfig(self):
    return os.path.join(self.directory, '.repo', 'manifests.git', 'config')

  def _EnsureMirroring(self, post_sync=False):
    """Ensure git is usable from w/in the chroot if --references is enabled

    repo init --references hardcodes the abspath to parent; this pathway
    however isn't usable from the chroot (it doesn't exist).  As such the
    pathway is rewritten to use relative pathways pointing at the root of
    the repo, which via I84988630 enter_chroot sets up a helper bind mount
    allowing git/repo to access the actual referenced repo.

    This has to be invoked prior to a repo sync of the target trybot to
    fix any pathways that may have been broken by the parent repo moving
    on disk, and needs to be invoked after the sync has completed to rewrite
    any new project's abspath to relative.
    """

    if not self._referenced_repo:
      return

    proj_root = os.path.join(self.directory, '.repo', 'projects')
    if not os.path.exists(proj_root):
      # Not yet synced, nothing to be done.
      return

    rewrite_git_alternates.RebuildRepoCheckout(self.directory,
                                               self._referenced_repo)

    if post_sync:
      chroot_path = os.path.join(constants.SOURCE_ROOT, '.repo', 'chroot',
                                 'external')
      chroot_path = cros_lib.ReinterpretPathForChroot(chroot_path)
      rewrite_git_alternates.RebuildRepoCheckout(
          self.directory, self._referenced_repo, chroot_path)

    # Finally, force the git config marker that enter_chroot looks for
    # to know when to do bind mounting trickery; this normally will exist,
    # but if we're converting a pre-existing repo checkout, it's possible
    # that it was invoked w/out the reference arg.  Note this must be
    # an absolute path to the source repo- enter_chroot uses that to know
    # what to bind mount into the chroot.
    cros_lib.RunCommand(['git', 'config', '--file', self._ManifestConfig,
                         'repo.reference', self._referenced_repo])

  def _ReinitializeIfNecessary(self, local_manifest):
    """Reinitializes the repository if the manifest has changed."""
    def _ShouldReinitialize():
      if local_manifest != self.DEFAULT_MANIFEST:
        return not filecmp.cmp(local_manifest, manifest_path)
      else:
        return not filecmp.cmp(default_manifest_path, manifest_path)

    manifest_path = self.GetRelativePath('.repo/manifest.xml')
    default_manifest_path = self.GetRelativePath('.repo/manifests/default.xml')
    if not (local_manifest and _ShouldReinitialize()):
      return

    logging.debug('Moving to manifest defined by %s', local_manifest)
    # If no manifest passed in, assume default.
    if local_manifest == self.DEFAULT_MANIFEST:
      self.Initialize(['--manifest-name=default.xml'], force=True)
    else:
      # The 10x speed up magic.
      # TODO: get documentation as to *why* this is supposedly 10x faster.
      os.unlink(manifest_path)
      shutil.copyfile(local_manifest, manifest_path)

  def _SyncToEBuilds(self):
    """Synchronize cros-workon packages to their stable commits.

    Find all stable ebuilds inheriting from 'cros-workon':  if the
    package repository's current tip-of-tree doesn't match the last
    recorded stable revision in the ebuild, 'git checkout' the
    stable revision.
    """
    overlay_list = portage_utilities.FindOverlays(self.directory, 'both')
    directory_src = os.path.join(self.directory, 'src')
    overlay_dict = dict((o, []) for o in overlay_list)
    repo_hash_map = {}
    repo_package_map = collections.defaultdict(list)
    portage_utilities.BuildEBuildDictionary(overlay_dict, True, None)
    for ebuild in itertools.chain.from_iterable(overlay_dict.values()):
      _, repo_path = ebuild.GetSourcePath(directory_src)
      stable_hash = ebuild.GetStableCommitId()
      if stable_hash:
        repo_package_map[repo_path].append(ebuild.package)
        if repo_path in repo_hash_map:
          if repo_hash_map[repo_path] != stable_hash:
            message = ('Conflicting git revisions '
                       'for repository %s in these packages:' % repo_path)
            cros_lib.Die('\n    '.join(
                             [message] + repo_package_map[repo_path]))
          continue
        repo_hash_map[repo_path] = stable_hash
        latest_hash = ebuild.GetCommitId(directory_src)
        if stable_hash != latest_hash:
          print 'Syncing package %s to last stable commit:' % ebuild.package
          cros_lib.RunCommand(['git', 'checkout', stable_hash],
                              cwd=repo_path, print_cmd=False)
          print

  def Sync(self, local_manifest=None, jobs=_DEFAULT_SYNC_JOBS, cleanup=True):
    """Sync/update the source.  Changes manifest if specified.

    local_manifest:  If set, checks out source to manifest.  DEFAULT_MANIFEST
    may be used to set it back to the default manifest.
    jobs: An integer representing how many repo jobs to run.
    """
    try:
      force_repo_update = self._initialized
      if not self._initialized:
        self.Initialize(force=True)

      self._ReinitializeIfNecessary(local_manifest)

      # Fix existing broken mirroring configurations.
      self._EnsureMirroring()

      if force_repo_update:
        # selfupdate prior to sync'ing.  Repo's first sync is  the manifest.
        # if we're deploying a new manifest that uses new repo functionality,
        # we have to repo up to date else it would fail.
        cros_lib.RunCommand(['repo', 'selfupdate'], cwd=self.directory)

      if cleanup:
        configure_repo.FixBrokenExistingRepos(self.directory)

      cros_lib.RunCommandWithRetries(2, ['repo', '--time', 'sync',
                                         '--jobs', str(jobs)],
                                     cwd=self.directory)

      # Fixup any new mirroring configurations.
      configure_repo.FixExternalRepoPushUrls(self.directory)

      # We do a second run to fix any new repositories created by repo to
      # use relative object pathways.  Note that cros_sdk also triggers the
      # same cleanup- we however kick it erring on the side of caution.
      self._EnsureMirroring(True)

    except cros_lib.RunCommandError, e:
      err_msg = 'Failed to sync sources %s' % e.message
      logging.error(err_msg)
      raise SrcCheckOutException(err_msg)

    if self._stable_sync:
      logging.info('Syncing cros-workon repositories to last stable commit.')
      self._SyncToEBuilds()

  def GetRelativePath(self, path):
    """Returns full path including source directory of path in repo."""
    return os.path.join(self.directory, path)

  def ExportManifest(self, output_file):
    """Export current manifest to a file.

    Args:
      output_file: Self explanatory.
    """
    DisableInteractiveRepoManifestCommand()
    cros_lib.RunCommand(['repo', 'manifest', '-r', '-o', output_file],
                        cwd=self.directory, print_cmd=True)

  def IsManifestDifferent(self, other_manifest):
    """Checks whether this manifest is different than another.

    May blacklists certain repos as part of the diff.

    Args:
      other_manfiest: Second manifest file to compare against.
    Returns:
      True: If the manifests are different
      False: If the manifests are same
    """
    black_list = ['="chromium/']
    logging.debug('Calling DiffManifests against %s', other_manifest)

    temp_manifest_file = tempfile.mktemp()
    try:
      self.ExportManifest(temp_manifest_file)
      blacklist_pattern = re.compile(r'|'.join(black_list))
      with open(temp_manifest_file, 'r') as manifest1_fh:
        with open(other_manifest, 'r') as manifest2_fh:
          for (line1, line2) in zip(manifest1_fh, manifest2_fh):
            if blacklist_pattern.search(line1):
              logging.debug('%s ignored %s', line1, line2)
              continue

            if line1 != line2:
              return True
          return False
    finally:
      os.remove(temp_manifest_file)
