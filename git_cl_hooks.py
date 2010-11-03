# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import sys

import breakpad  # pylint: disable=W0611

from git_cl_repo import git_cl
from git_cl_repo import upload

import presubmit_support
import scm
import watchlists

# Really ugly hack to quiet upload.py
upload.verbosity = 0

def Backquote(cmd, cwd=None):
  """Like running `cmd` in a shell script."""
  return subprocess.Popen(cmd,
                          cwd=cwd,
                          stdout=subprocess.PIPE).communicate()[0].strip()

def ConvertToInteger(inputval):
  """Convert a string to integer, but returns either an int or None."""
  try:
    return int(inputval)
  except (TypeError, ValueError):
    return None


class ChangeOptions:
  def __init__(self, commit=None, upstream_branch=None):
    self.commit = commit
    self.verbose = None
    self.default_presubmit = None
    self.may_prompt = None

    root = Backquote(['git', 'rev-parse', '--show-cdup'])
    if not root:
      root = "."
    absroot = os.path.abspath(root)
    if not root:
      raise Exception("Could not get root directory.")
    # We use the sha1 of HEAD as a name of this change.
    name = Backquote(['git', 'rev-parse', 'HEAD'])
    files = scm.GIT.CaptureStatus([root], upstream_branch)
    cl = git_cl.Changelist()
    issue = ConvertToInteger(cl.GetIssue())
    patchset = ConvertToInteger(cl.GetPatchset())
    if issue:
      description = cl.GetDescription()
    else:
      # If the change was never uploaded, use the log messages of all commits
      # up to the branch point, as git cl upload will prefill the description
      # with these log messages.
      description = Backquote(['git', 'log', '--pretty=format:%s%n%n%b',
                               '%s...' % (upstream_branch)])
    self.change = presubmit_support.GitChange(name, description, absroot, files,
                                              issue, patchset)


def RunHooks(hook_name, upstream_branch):
  commit = (hook_name == 'pre-cl-dcommit')

  # Create our options based on the command-line args and the current checkout.
  options = ChangeOptions(commit=commit, upstream_branch=upstream_branch)

  # Apply watchlists on upload.
  if not commit:
    watchlist = watchlists.Watchlists(options.change.RepositoryRoot())
    files = [f.LocalPath() for f in options.change.AffectedFiles()]
    watchers = watchlist.GetWatchersForPaths(files)
    Backquote(['git', 'config', '--replace-all',
               'rietveld.extracc', ','.join(watchers)])

  # Run the presubmit checks.
  if presubmit_support.DoPresubmitChecks(options.change,
                                         options.commit,
                                         options.verbose,
                                         sys.stdout,
                                         sys.stdin,
                                         options.default_presubmit,
                                         options.may_prompt):
    sys.exit(0)
  else:
    sys.exit(1)
