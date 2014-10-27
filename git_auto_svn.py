#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Performs all git-svn setup steps necessary for 'git svn dcommit' to work.

Assumes that trunk of the svn remote maps to master of the git remote.

Example:
git clone https://chromium.googlesource.com/chromium/tools/depot_tools
cd depot_tools
git auto-svn
"""

import argparse
import os
import sys
import urlparse

import subprocess2

from git_common import run as run_git
from git_common import run_stream as run_git_stream
from git_common import set_config, root, ROOT
from git_footers import parse_footers, get_unique, GIT_SVN_ID_PATTERN


SVN_EXE = ROOT+'\\svn.bat' if sys.platform.startswith('win') else 'svn'


def run_svn(*cmd, **kwargs):
  """Runs an svn command.

  Returns (stdout, stderr) as a pair of strings.

  Raises subprocess2.CalledProcessError on nonzero return code.
  """
  kwargs.setdefault('stdin', subprocess2.PIPE)
  kwargs.setdefault('stdout', subprocess2.PIPE)
  kwargs.setdefault('stderr', subprocess2.PIPE)

  cmd = (SVN_EXE,) + cmd
  proc = subprocess2.Popen(cmd, **kwargs)
  ret, err = proc.communicate()
  retcode = proc.wait()
  if retcode != 0:
    raise subprocess2.CalledProcessError(retcode, cmd, os.getcwd(), ret, err)

  return ret, err


def main(argv):
  # No command line flags. Just use the parser to prevent people from trying
  # to pass flags that don't do anything, and to provide 'usage'.
  parser = argparse.ArgumentParser(
      description='Automatically set up git-svn for a repo mirrored from svn.')
  parser.parse_args(argv[1:])

  upstream = root()
  message = run_git('log', '-1', '--format=%B', upstream)
  footers = parse_footers(message)
  git_svn_id = get_unique(footers, 'git-svn-id')
  match = GIT_SVN_ID_PATTERN.match(git_svn_id)
  assert match, 'No valid git-svn-id footer found on %s.' % upstream
  print 'Found git-svn-id footer %s on %s' % (match.group(1), upstream)

  parsed_svn = urlparse.urlparse(match.group(1))
  path_components = parsed_svn.path.split('/')
  svn_repo = None
  svn_path = None
  for i in xrange(len(path_components)):
    try:
      maybe_repo = '%s://%s%s' % (
          parsed_svn.scheme, parsed_svn.netloc, '/'.join(path_components[:i+1]))
      print 'Checking ', maybe_repo
      run_svn('info', maybe_repo)
      svn_repo = maybe_repo
      svn_path = '/'.join(path_components[i+1:])
      break
    except subprocess2.CalledProcessError:
      continue
  assert svn_repo is not None, 'Unable to find svn repo for %s' % match.group(1)
  print 'Found upstream svn repo %s and path %s' % (svn_repo, svn_path)

  set_config('svn-remote.svn.url', svn_repo)
  set_config('svn-remote.svn.fetch',
             '%s:refs/remotes/%s' % (svn_path, upstream))
  print 'Configured metadata, running "git svn fetch". This may take some time.'
  for line in run_git_stream('svn', 'fetch').xreadlines():
    print line.strip()


if __name__ == '__main__':
  sys.exit(main(sys.argv))
