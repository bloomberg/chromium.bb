#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script is a wrapper around the GN binary that is pulled from Google
Cloud Storage when you sync Chrome. The binaries go into platform-specific
subdirectories in the source tree.

This script makes there be one place for forwarding to the correct platform's
binary. It will also automatically try to find the gn binary when run inside
the chrome source tree, so users can just type "gn" on the command line
(normally depot_tools is on the path)."""

import os
import subprocess
import sys


class PlatformUnknownError(IOError):
  pass


def HasDotfile(path):
  """Returns True if the given path has a .gn file in it."""
  return os.path.exists(path + '/.gn')


def FindSourceRootOnPath():
  """Searches upward from the current directory for the root of the source
  tree and returns the found path. Returns None if no source root could
  be found."""
  cur = os.getcwd()
  while True:
    if HasDotfile(cur):
      return cur
    up_one = os.path.dirname(cur)
    if up_one == cur:
      return None  # Reached the top of the directory tree
    cur = up_one


def RunGN(sourceroot):
  # The binaries in platform-specific subdirectories in src/tools/gn/bin.
  gnpath = sourceroot + '/tools/gn/bin/'
  if sys.platform in ('cygwin', 'win32'):
    gnpath += 'win/gn.exe'
  elif sys.platform.startswith('linux'):
    gnpath += 'linux/gn'
  elif sys.platform == 'darwin':
    gnpath += 'mac/gn'
  else:
    raise PlatformUnknownError('Unknown platform for GN: ' + sys.platform)

  return subprocess.call([gnpath] + sys.argv[1:])


def main(args):
  sourceroot = FindSourceRootOnPath()
  if not sourceroot:
    print >> sys.stderr, '.gn file not found in any parent of the current path.'
    sys.exit(1)
  return RunGN(sourceroot)

if __name__ == '__main__':
  sys.exit(main(sys.argv))
