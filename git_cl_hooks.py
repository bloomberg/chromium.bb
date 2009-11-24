# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import subprocess
import sys

import presubmit_support
import scm

def Backquote(cmd, cwd=None):
  """Like running `cmd` in a shell script."""
  return subprocess.Popen(cmd,
                          cwd=cwd,
                          stdout=subprocess.PIPE).communicate()[0].strip()


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
    log = Backquote(['git', 'show', '--name-only',
                     '--pretty=format:%H%n%s%n%n%b'])
    m = re.match(r'^(\w+)\n(.*)$', log, re.MULTILINE|re.DOTALL)
    if not m:
      raise Exception("Could not parse log message: %s" % log)
    name = m.group(1)
    description = m.group(2)
    files = scm.GIT.CaptureStatus([root], upstream_branch)
    issue = Backquote(['git', 'cl', 'status', '--field=id'])
    patchset = None
    self.change = presubmit_support.GitChange(name, description, absroot, files,
                                              issue, patchset)


def RunHooks(hook_name, upstream_branch):
  commit = (hook_name == 'pre-cl-dcommit')

  # Create our options based on the command-line args and the current checkout.
  options = ChangeOptions(commit=commit, upstream_branch=upstream_branch)

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
