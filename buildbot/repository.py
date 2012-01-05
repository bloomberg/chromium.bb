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

# File that marks a buildroot as being used by a trybot
_TRYBOT_MARKER = '.trybot'

_DEFAULT_SYNC_JOBS = 4

class SrcCheckOutException(Exception):
  """Exception gets thrown for failure to sync sources"""
  pass


def InARepoRepository(directory):
  """Returns True if directory is part of a repo checkout."""
  output = cros_lib.RunCommand(
      ['repo'], error_ok=True, redirect_stdout=True, redirect_stderr=True,
      cwd=directory, exit_code=True, print_cmd=False)
  return output.returncode == 0


def CloneGitRepo(working_dir, repo_url):
  """Clone given git repo
  Args:
    repo_url: git repo to clone
    repo_dir: location where it should be cloned to
  """
  if not os.path.exists(working_dir): os.makedirs(working_dir)
  cros_lib.RunCommand(['git', 'clone', repo_url, working_dir], cwd=working_dir)


def GetTrybotMarkerPath(buildroot):
  """Get path to trybot marker file given the buildroot."""
  return os.path.join(buildroot, _TRYBOT_MARKER)


def CreateTrybotMarker(buildroot):
  """Create the file that identifies a buildroot as being used by a trybot."""
  open(GetTrybotMarkerPath(buildroot), 'w').close()


def ClearBuildRoot(buildroot):
  """Remove and recreate the buildroot while preserving the trybot marker."""
  trybot_root = os.path.exists(GetTrybotMarkerPath(buildroot))
  cros_lib.RunCommand(['sudo', 'rm', '-rf', buildroot], error_ok=True)
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

  def __init__(self, repo_url, directory, branch=None, stable_sync=False):
    self.repo_url = repo_url
    self.directory = directory
    self.branch = branch
    self._stable_sync = stable_sync

  def Initialize(self):
    """Initializes a repository."""
    assert not os.path.exists(os.path.join(self.directory, '.repo')), \
        'Repo already initialized.'
    # Base command.
    init_cmd = self._INIT_CMD + ['--manifest-url', self.repo_url]

    # Handle branch / manifest options.
    if self.branch: init_cmd.extend(['--manifest-branch', self.branch])
    cros_lib.RunCommand(init_cmd, cwd=self.directory, input='\n\ny\n')

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

    logging.debug('Moving to manifest defined by %s' % local_manifest)
    # If no manifest passed in, assume default.
    if local_manifest == self.DEFAULT_MANIFEST:
      cros_lib.RunCommand(self._INIT_CMD + ['--manifest-name=default.xml'],
                          cwd=self.directory, input='\n\ny\n')
    else:
      # The 10x speed up magic.
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
      do_self_update = InARepoRepository(self.directory)
      if not do_self_update:
        self.Initialize()

      self._ReinitializeIfNecessary(local_manifest)

      if do_self_update:
        # selfupdate prior to sync'ing.  Repo's first sync is  the manifest.
        # if we're deploying a new manifest that uses new repo functionality,
        # we have to repo up to date else it would fail.
        cros_lib.RunCommand(['repo', 'selfupdate'],
                             cwd=self.directory)

      if cleanup:
        configure_repo.FixBrokenExistingRepos(self.directory)

      cros_lib.OldRunCommand(['repo', '--time', 'sync', '--jobs', str(jobs)],
                             cwd=self.directory, num_retries=2)

      configure_repo.FixExternalRepoPushUrls(self.directory)

    except cros_lib.RunCommandError, e:
      err_msg = 'Failed to sync sources %s' % e.message
      logging.error(err_msg)
      raise SrcCheckOutException(err_msg)

    if self._stable_sync:
      cros_lib.Info('Syncing cros-workon repositories '
                    'to last stable commit.')
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
    black_list = ['manifest-versions']
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
