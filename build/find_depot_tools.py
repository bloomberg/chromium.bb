#!/usr/bin/python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Small utility to find depot_tools."""

import os
import sys


def _IsRealDepotTools(path):
  return os.path.isfile(os.path.join(path, 'gclient'))


def FindDepotTools():
  """Search for depot_tools and return its path."""
  # First look if depot_tools is already in PYTHONPATH.
  for i in sys.path:
    if i.rstrip(os.sep).endswith('depot_tools') and _IsRealDepotTools(i):
      return i
  # Then look if depot_tools is in PATH, common case.
  for i in os.environ['PATH'].split(os.pathsep):
    if _IsRealDepotTools(i):
      return i.rstrip(os.sep)
  # Rare case, it's not even in PATH, look upward up to root.
  root_dir = os.path.dirname(os.path.abspath(__file__))
  previous_dir = os.path.abspath(__file__)
  while root_dir and root_dir != previous_dir:
    i = os.path.join(root_dir, 'depot_tools')
    if _IsRealDepotTools(i):
      return i
    previous_dir = root_dir
    root_dir = os.path.dirname(root_dir)
  return None


def main():
  path = FindDepotTools()
  if not path:
    print >> sys.stderr, 'Failed to find depot_tools'
    return 1
  print path
  return 0


if __name__ == '__main__':
  sys.exit(main())
