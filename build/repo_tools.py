#!/usr/bin/python
# Copyright (c) 2013 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os

import file_tools
import log_tools
import platform_tools
import subprocess

def GitCmd():
  """Return the git command to execute for the host platform."""
  if platform_tools.IsWindows():
    # On windows, we want to use the depot_tools version of git, which has
    # git.bat as an entry point. When running through the msys command
    # prompt, subprocess does not handle batch files. Explicitly invoking
    # cmd.exe to be sure we run the correct git in this case.
    return ['cmd.exe', '/c', 'git.bat']
  else:
    return ['git']


def CheckGitOutput(args):
  """Run a git subcommand and capture its stdout a la subprocess.check_output.
  Args:
    args: list of arguments to 'git'
  """
  return log_tools.CheckOutput(GitCmd() + args)


def SvnCmd():
  """Return the svn command to execute for the host platform."""
  if platform_tools.IsWindows():
    return ['cmd.exe', '/c', 'svn.bat']
  else:
    return ['svn']


def SyncGitRepo(url, destination, revision, reclone=False, clean=False,
                pathspec=None):
  """Sync an individual git repo.

  Args:
  url: URL to sync
  destination: Directory to check out into.
  revision: Pinned revision to check out. If None, do not check out a
            pinned revision.
  reclone: If True, delete the destination directory and re-clone the repo.
  clean: If True, discard local changes and untracked files.
         Otherwise the checkout will fail if there are uncommitted changes.
  pathspec: If not None, add the path to the git checkout command, which
            causes it to just update the working tree without switching
            branches.
  """
  if reclone:
    logging.debug('Clobbering source directory %s' % destination)
    file_tools.RemoveDirectoryIfPresent(destination)
  git = GitCmd()
  if not os.path.exists(destination) or len(os.listdir(destination)) == 0:
    logging.info('Cloning %s...' % url)
    log_tools.CheckCall(git + ['clone', '-n', url, destination])
  if revision is not None:
    logging.info('Checking out pinned revision...')
    log_tools.CheckCall(git + ['fetch', '--all'], cwd=destination)
    checkout_flags = ['-f'] if clean else []
    path = [pathspec] if pathspec else []
    log_tools.CheckCall(
        git + ['checkout'] + checkout_flags + [revision] + path,
        cwd=destination)
  if clean:
    log_tools.CheckCall(git + ['clean', '-dffx'], cwd=destination)


def CleanGitWorkingDir(directory, path):
  """Clean a particular path of an existing git checkout.

     Args:
     directory: Directory where the git repo is currently checked out
     path: path to clean, relative to the repo directory
  """
  log_tools.CheckCall(GitCmd() + ['clean', '-f', path], cwd=directory)


def GitRevInfo(directory):
  """Get the git revision information of a git checkout.

  Args:
    directory: Existing git working directory.
"""
  url = log_tools.CheckOutput(GitCmd() + ['ls-remote', '--get-url', 'origin'],
                              cwd=directory)
  rev = log_tools.CheckOutput(GitCmd() + ['rev-parse', 'HEAD'],
                              cwd=directory)
  return url.strip(), rev.strip()

def SvnRevInfo(directory):
  """Get the SVN revision information of an existing svn/gclient checkout.

  Args:
     directory: Directory where the svn repo is currently checked out
  """
  info = log_tools.CheckOutput(SvnCmd() + ['info'], cwd=directory)
  url = ''
  rev = ''
  for line in info.splitlines():
    pieces = line.split(':', 1)
    if len(pieces) != 2:
      continue
    if pieces[0] == 'URL':
      url = pieces[1].strip()
    elif pieces[0] == 'Revision':
      rev = pieces[1].strip()
  if not url or not rev:
    raise RuntimeError('Missing svn info url: %s and rev: %s' % (url, rev))
  return url, rev
