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


class NotInRepoError(Error):
  """A repo operation was attempted outside of a repo repository."""


class Repository(object):
  """Repository represents an initialized repo repository."""

  def __init__(self, root):
    """Initialize Repository.

    Args:
      root: Path to the root of a repo repository; must contain a .repo subdir.

    Raises:
      NotInRepoError: root didn't contain a .repo subdir.
    """
    self.root = os.path.abspath(root)
    self._repo_dir = os.path.join(self.root, '.repo')
    self._ValidateRepoDir()

  def _ValidateRepoDir(self):
    """Validate that the repo dir exists."""
    if not os.path.isdir(self._repo_dir):
      raise NotInRepoError('no .repo dir in %r' % self.root)

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
      cmd += ['--manifest-branch', manifest_branch]
    if manifest_name is not None:
      cmd += ['--manifest-name', manifest_name]
    if mirror:
      cmd += ['--mirror']
    if reference is not None:
      cmd += ['--reference', reference]
    if depth is not None:
      cmd += ['--depth', str(depth)]
    if groups is not None:
      cmd += ['--groups', groups]
    if repo_url is not None:
      cmd += ['--repo-url', repo_url]

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

  @classmethod
  def MustFind(cls, path):
    """Searches for a repo directory and returns a Repository if found.

    Args:
      path: The path where the search starts.

    Raises:
      NotInRepoError: if no Repository is found.
    """
    repo = cls.Find(path)
    if repo is None:
      raise NotInRepoError('no repo found from %r', path)
    return repo

  def _Run(self, repo_cmd, cwd=None):
    """RunCommand wrapper for `repo`.

    Args:
      repo_cmd: List of arguments to pass to `repo`.
      cwd: The path to run the command in. Defaults to Repository root.
        Must be within the root.

    Returns:
      A CommandResult object.

    Raises:
      NotInRepoError: if cwd is not within the Repository root.
      RunCommandError: if the command failed.
    """
    # Use the checkout's copy of repo so that it doesn't have to be in PATH.
    cmd = [os.path.join(self._repo_dir, 'repo', 'repo')] + repo_cmd
    if cwd is None:
      cwd = self.root
    elif git.FindRepoCheckoutRoot(cwd) != self.root:
      raise NotInRepoError('cannot run `repo` outside of Repository root '
                           '(cwd=%r root=%r)' % (cwd, self.root))
    return cros_build_lib.RunCommand(cmd, cwd=cwd)

  def Sync(self, projects=None, jobs=None, cwd=None):
    """Run `repo sync`.

    Args:
      projects: A list of project names to sync.
      jobs: Number of projects to sync in parallel.
      cwd: The path to run the command in. Defaults to Repository root.

    Raises:
      NotInRepoError: if cwd is not within the Repository root.
      RunCommandError: if the command failed.
    """
    args = _ListArg(projects)
    if jobs is not None:
      args += ['--jobs', str(jobs)]
    self._Run(['sync'] + args, cwd=cwd)

  def StartBranch(self, name, projects=None, cwd=None):
    """Run `repo start`.

    Args:
      name: The name of the branch to create.
      projects: A list of projects to create the branch in. Defaults to
        creating in all projects.
      cwd: The path to run the command in. Defaults to Repository root.

    Raises:
      NotInRepoError: if cwd is not within the Repository root.
      RunCommandError: if `repo start` failed.
    """
    if projects is None:
      projects = ['--all']
    else:
      projects = _ListArg(projects)
    self._Run(['start', name] + projects, cwd=cwd)


def _ListArg(arg):
  """Return a new list from arg.

  Args:
    arg: If a non-basestring iterable, return a new list with its contents. If
      None, return an empty list.

  Raises:
    TypeError: if arg is a basestring or non-iterable (except None).
  """
  if isinstance(arg, basestring):
    raise TypeError('string not allowed')
  if arg is None:
    return []
  return list(arg)
