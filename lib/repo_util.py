# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common functions for interacting with repo."""

from __future__ import print_function

import os

from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import git


class Error(Exception):
  """An error related to repo."""


class Repository(object):
  """Repository represents an initialized repo repository."""

  def __init__(self, root):
    """Initialize Repository.

    Args:
      root: Path to the root of a repo repository; must contain a .repo subdir.

    Raises:
      Error: root didn't contain a .repo subdir.
    """
    self.root = os.path.abspath(root)
    self._repo_dir = os.path.join(self.root, '.repo')
    self._ValidateRepoDir()

  def _ValidateRepoDir(self):
    """Validate that the repo dir exists."""
    if not os.path.isdir(self._repo_dir):
      raise Error('no .repo dir in %r' % self.root)

  @classmethod
  def Initialize(cls, root, manifest_url, manifest_branch=None,
                 manifest_name=None, mirror=False, reference=None, depth=None,
                 groups=None, repo_url=None):
    """Initialize and return a new Repository with `repo init`.

    Args:
      root: Path to the new repo root. Must not be within an existing
        repo repository.
      manifest_url: Manifest repository URL.
      manifest_branch: Manifest branch or revision.
      manifest_name: Initial manifest file from manifest repository.
      mirror: If True, create a repo mirror instead of a checkout.
      reference: Location of a mirror directory to reference.
      depth: Create shallow git clones with the given depth.
      groups: Restrict manifest projects to the given groups.
      repo_url: Repo command repository URL.

    Raises:
      Error: root already contained a .repo subdir.
      RunCommandError: `repo init` failed.
    """
    existing_root = git.FindRepoCheckoutRoot(root)
    if existing_root is not None:
      raise Error('cannot init in existing repo %r.' % existing_root)

    # TODO(lannm): Use 'chromite/bootstrap/repo'?
    cmd = ['repo', 'init', '--manifest-url', manifest_url]
    if manifest_branch is not None:
      cmd.extend(['--manifest-branch', manifest_branch])
    if manifest_name is not None:
      cmd.extend(['--manifest-name', manifest_name])
    if mirror:
      cmd.append('--mirror')
    if reference is not None:
      cmd.extend(['--reference', reference])
    if depth is not None:
      cmd.extend(['--depth', str(depth)])
    if groups is not None:
      cmd.extend(['--groups', groups])
    if repo_url is not None:
      cmd.extend(['--repo-url', repo_url])

    try:
      cros_build_lib.RunCommand(cmd, cwd=root)
    except cros_build_lib.RunCommandError as e:
      repo_dir = os.path.join(root, '.repo')
      if os.path.exists(repo_dir):
        logging.warning('Removing %r due to `repo init` failures.', repo_dir)
        try:
          cros_build_lib.RunCommand(['rm', '-rf', repo_dir])
        except cros_build_lib.RunCommandError, rm_err:
          logging.warning('Error removing repo dir: %s', rm_err)
      raise e
    return cls(root)

  @classmethod
  def Find(cls, path):
    """Searches for a repo directory and returns a Repository if found.

    Args:
      path: The path where the search starts.
    """
    repo_root = git.FindRepoCheckoutRoot(path)
    if repo_root is None:
      return None
    return cls(repo_root)

  def RunRepo(self, subcmd, *args, **kwargs):
    """RunCommand wrapper for `repo` commands.

    Args:
      subcmd: `repo` subcommand to run, e.g. 'sync'.
      args: arguments to pass to the repo subcommand.
      kwargs: Keyword arguments to RunCommand; 'cwd' will be set to
        the Repository root.

    Returns:
      A CommandResult object.

    Raises:
      RunCommandError: The command failed.
    """
    # Use the checkout's copy of repo so that it doesn't have to be in PATH.
    cmd = [os.path.join(self._repo_dir, 'repo', 'repo'), subcmd] + list(args)
    kwargs['cwd'] = self.root
    return cros_build_lib.RunCommand(cmd, **kwargs)

  def Sync(self, projects=(), jobs=None):
    """Run `repo sync`.

    Args:
      projects: A list of project names to sync.
      jobs: Number of projects to sync in parallel.

    Raises:
      RunCommandError: `repo sync` failed.
    """
    args = list(projects)
    if jobs is not None:
      args.extend(['--jobs', str(jobs)])
    self.RunRepo('sync', *args)
