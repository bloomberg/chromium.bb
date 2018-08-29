# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common functions for interacting with repo."""

from __future__ import print_function

import collections
import contextlib
import os
import re

from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import git
from chromite.lib import osutils
from chromite.lib import repo_manifest


# Match `repo` error: "error: project <name> not found"
PROJECT_NOT_FOUND_RE = re.compile(r'^error: project (?P<name>[^ ]+) not found$',
                                  re.MULTILINE)

PROJECT_OBJECTS_PATH_FORMAT = os.path.join(
    '.repo', 'project-objects', '%s.git', 'objects')

# ProjectInfo represents the information returned by `repo list`.
ProjectInfo = collections.namedtuple('ProjectInfo', ('name', 'path'))


class Error(Exception):
  """An error related to repo."""


class NotInRepoError(Error):
  """A repo operation was attempted outside of a repo repository."""


class ProjectNotFoundError(Error):
  """A repo operation was attempted on a project that wasn't found."""


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
      if isinstance(reference, Repository):
        reference = reference.root
      cmd += ['--reference', reference]
    if depth is not None:
      cmd += ['--depth', str(depth)]
    if groups is not None:
      cmd += ['--groups', groups]
    if repo_url is not None:
      cmd += ['--repo-url', repo_url]

    repo_dir = os.path.join(root, '.repo')
    warning_msg = 'Removing %r due to `repo init` failures.' % repo_dir
    with _RmDirOnError(repo_dir, msg=warning_msg):
      cros_build_lib.RunCommand(cmd, cwd=root)
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

  def _Run(self, repo_cmd, cwd=None, capture_output=False):
    """RunCommand wrapper for `repo`.

    Args:
      repo_cmd: List of arguments to pass to `repo`.
      cwd: The path to run the command in. Defaults to Repository root.
        Must be within the root.
      capture_output: Whether to capture the output, making it available in the
        CommandResult object, or print it to stdout/err. Defaults to False.

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
    return cros_build_lib.RunCommand(cmd, cwd=cwd,
                                     capture_output=capture_output,
                                     debug_level=logging.DEBUG)

  def Sync(self, projects=None, local_only=False, current_branch=False,
           jobs=None, cwd=None):
    """Run `repo sync`.

    Args:
      projects: A list of project names to sync.
      local_only: Only update working tree; don't fetch.
      current_branch: Fetch only the current branch.
      jobs: Number of projects to sync in parallel.
      cwd: The path to run the command in. Defaults to Repository root.

    Raises:
      NotInRepoError: if cwd is not within the Repository root.
      RunCommandError: if the command failed.
    """
    args = _ListArg(projects)
    if local_only:
      args += ['--local-only']
    if current_branch:
      args += ['--current-branch']
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

  def List(self, projects=None, cwd=None):
    """Run `repo list` and returns a list of ProjectInfos for synced projects.

    Note that this may produce a different list than Manifest().Projects()
    due to partial project syncing (e.g. `repo init -g minilayout`).

    Args:
      projects: A list of projects to return. Defaults to all projects.
      cwd: The path to run the command in. Defaults to Repository root.

    Raises:
      ProjectNotFoundError: if a project in 'projects' was not found.
      NotInRepoError: if cwd is not within the Repository root.
      RunCommandError: if `repo list` otherwise failed.
    """
    projects = _ListArg(projects)
    try:
      result = self._Run(['list'] + projects, cwd=cwd, capture_output=True)
    except cros_build_lib.RunCommandError, rce:
      m = PROJECT_NOT_FOUND_RE.search(rce.result.error)
      if m:
        raise ProjectNotFoundError(m.group('name'))
      raise rce

    infos = []
    for line in result.output.splitlines():
      path, name = line.rsplit(' : ', 1)
      infos.append(ProjectInfo(name=name, path=path))
    return infos

  def Manifest(self, revision_locked=False):
    """Run `repo manifest` and return a repo_manifest.Manifest.

    Args:
      revision_locked: If True, create a "revision locked" manifest with each
      project's revision set to that project's current HEAD.

    Raises:
      RunCommandError: if `repo list` otherwise failed.
      repo_manifest.Error: if the output couldn't be parsed into a Manifest.
    """
    cmd = ['manifest']
    if revision_locked:
      cmd += ['--revision-as-HEAD']
    result = self._Run(cmd, capture_output=True)
    return repo_manifest.Manifest.FromString(result.output)

  def Copy(self, dest_root):
    """Efficiently `cp` the .repo directory, using hardlinks if possible.

    Args:
      dest_root: Path to copy the .repo directory into. Must exist and must
        not already contain a .repo directory.

    Returns:
      A Repository pointing at dest_root.

    Raises:
      Error: if dest_root already contained a .repo subdir.
      RunCommandError: if `cp` failed.
    """
    existing_root = git.FindRepoCheckoutRoot(dest_root)
    if existing_root is not None:
      raise Error('cannot copy into existing repo %r' % existing_root)

    dest_path = os.path.abspath(dest_root)

    with _RmDirOnError(os.path.join(dest_root, '.repo')):
      # First, try to hard link project objects to dest_dir; this may fail if
      # e.g. the src and dest are on different mounts.
      for project in self.List():
        objects_dir = PROJECT_OBJECTS_PATH_FORMAT % project.name
        try:
          cros_build_lib.RunCommand(
              ['cp', '--archive', '--link', '--parents', objects_dir,
               dest_path],
              extra_env={'LC_MESSAGES': 'C'}, cwd=self.root)
        except cros_build_lib.RunCommandError as e:
          if 'Invalid cross-device link' in e.result.error:
            logging.warning("Can't hard link across devices; aborting linking.")
            break
          logging.warning('Copy linking failed: %s', e.result.error)

      # Copy everything that wasn't created by the hard linking above.
      try:
        cros_build_lib.RunCommand(
            ['cp', '--archive', '--no-clobber', '.repo', dest_path],
            cwd=self.root, extra_env={'LC_MESSAGES': 'C'})
      except cros_build_lib.RunCommandError as e:
        # Despite the --no-clobber, `cp` still complains when trying to copy a
        # file to its existing hard link. Filter these errors from the output
        # to see if there were any real failures.
        errors = e.result.error.splitlines()
        real_errors = [x for x in errors if 'are the same file' not in x]
        if real_errors:
          e.result.error = '\n'.join(real_errors)
          raise e
      return Repository(dest_root)


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


@contextlib.contextmanager
def _RmDirOnError(path, msg=None):
  """Context that will RmDir(path) if its block throws an exception."""
  try:
    yield
  except:
    if os.path.exists(path):
      if msg:
        logging.warning(msg)
      try:
        osutils.RmDir(path, ignore_missing=True)
      except StandardError as e:
        logging.warning('Failed to clean up %r: %s', path, e)
    raise
