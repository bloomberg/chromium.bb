#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Implement directory storage on top of file only storage.

Given a storage object capable of storing and retrieving files,
embellish with methods for storing and retrieving directories (using tar).
"""

import os
import sys
import tempfile

import file_tools
import subprocess


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
CYGTAR_PATH = os.path.join(SCRIPT_DIR, 'cygtar.py')


class DirectoryStorageAdapter(object):
  """Adapter that implements directory storage on top of file storage.

  Tars directories as needed to keep operations on the data-store atomic.
  """

  def __init__(self, storage):
    """Init for this class.

    Args:
      storage: File storage object supporting GetFile and PutFile.
    """
    self._storage = storage

  def PutDirectory(self, path, key):
    """Write a directory to storage.

    Args:
      path: Path of the directory to write.
      key: Key to store under.
    """
    handle, tmp_tgz = tempfile.mkstemp(prefix='dirstore', suffix='.tmp.tgz')
    try:
      os.close(handle)
      # Calling cygtar thru subprocess as it's cwd handling is not currently
      # usable.
      subprocess.check_call([sys.executable, CYGTAR_PATH,
                             '-c', '-z', '-f', os.path.abspath(tmp_tgz), '.'],
                             cwd=os.path.abspath(path))
      return self._storage.PutFile(tmp_tgz, key)
    finally:
      os.remove(tmp_tgz)

  def GetDirectory(self, key, path):
    """Read a directory from storage.

    Clobbers anything at the destination currently.
    Args:
      key: Key to fetch from.
      path: Path of the directory to write.
    """
    file_tools.RemoveDirectoryIfPresent(path)
    os.mkdir(path)
    handle, tmp_tgz = tempfile.mkstemp(prefix='dirstore', suffix='.tmp.tgz')
    try:
      os.close(handle)
      url = self._storage.GetFile(key, tmp_tgz)
      if url is None:
        return None
      # Calling cygtar thru subprocess as it's cwd handling is not currently
      # usable.
      subprocess.check_call([sys.executable, CYGTAR_PATH,
                             '-x', '-z', '-f', os.path.abspath(tmp_tgz)],
                             cwd=os.path.abspath(path))
      return url
    finally:
      os.remove(tmp_tgz)
