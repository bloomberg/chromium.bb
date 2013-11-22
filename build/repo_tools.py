#!/usr/bin/python
# Copyright (c) 2013 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os

import file_tools
import log_tools
import platform_tools

def SyncGitRepo(url, destination, revision, reclone=False, clean=True):
  """Sync an individual git repo.

  Args:
  url: URL to sync
  destination: Directory to check out into.
  revision: Pinned revision to check out. If None, do not check out a
            pinned revision.
  reclone: If True, delete the destination directory and re-clone the repo.
  clean: If True, discard local changes and untracked files.
         Otherwise the checkout will fail if there are uncommitted changes.
  """

  if reclone:
    file_tools.RemoveDirectoryIfPresent(destination)
  if platform_tools.IsWindows():
    # On windows, we want to use the depot_tools version of git, which has
    # git.bat as an entry point. When running through the msys command
    # prompt, subprocess does not handle batch files. Explicitly invoking
    # cmd.exe to be sure we run the correct git in this case.
    git = ['cmd.exe', '/c', 'git.bat']
  else:
    git = ['git']
  if not os.path.exists(destination):
    logging.info('Cloning %s...' % url)
    log_tools.CheckCall(git + ['clone', '-n', url, destination])
  if revision is not None:
    logging.info('Checking out pinned revision...')
    log_tools.CheckCall(git + ['fetch', '--all'], cwd=destination)
    checkout_flags = ['-f'] if clean else []
    log_tools.CheckCall(git + ['checkout'] + checkout_flags + [revision],
                        cwd=destination)
  if clean:
    log_tools.CheckCall(git + ['clean', '-dffx'], cwd=destination)
