#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Hashing related operations.

Provides for hashing files or directory trees.
Timestamps, archive order, and dot files/directories are ignored to keep
hashes stable.
"""

import os


def HashFileContents(filename, hasher):
  """Feed the data in a file to a hasher.

  Args:
    filename: Filename to read.
    hasher: Hashing object to update.
  """
  fh = open(filename, 'rb')
  try:
    while True:
      data = fh.read(4096)
      if not data:
        break
      hasher.update(data)
  finally:
    fh.close()


def StableHashPath(path, hasher):
  """Hash everything in a stable (reproducible) way.

  Args:
    path: Path to hash.
    hasher: Hashing object to update.
  """
  if os.path.isfile(path):
    hasher.update('singlefile:')
    HashFileContents(path, hasher)
    return

  def RemoveExcludedPaths(paths):
    for p in [p for p in paths if p.startswith('.')]:
      paths.remove(p)

  for root, dirs, files in os.walk(path):
    dirs.sort()
    files.sort()
    RemoveExcludedPaths(dirs)
    RemoveExcludedPaths(files)
    hasher.update('dir_count:%d' % len(dirs))
    for d in dirs:
      hasher.update('dir:' + d + '\x00')
    hasher.update('file_count:%d' % len(files))
    for f in files:
      hasher.update('filename:' + f + '\x00')
      hasher.update('contents:')
      HashFileContents(os.path.join(root, f), hasher)
