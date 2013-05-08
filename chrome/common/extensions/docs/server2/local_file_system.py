# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

from docs_server_utils import StringIdentity
from file_system import FileSystem, FileNotFoundError, StatInfo, ToUnicode
from future import Future

def _ConvertToFilepath(path):
  return path.replace('/', os.sep)

class LocalFileSystem(FileSystem):
  '''FileSystem implementation which fetches resources from the local
  filesystem.
  '''
  def __init__(self, base_path):
    self._base_path = _ConvertToFilepath(base_path)

  @staticmethod
  def Create():
    return LocalFileSystem(os.path.join(sys.path[0], os.pardir, os.pardir))

  def _ReadFile(self, filename, binary):
    try:
      mode = 'rb' if binary else 'r'
      with open(os.path.join(self._base_path, filename), mode) as f:
        contents = f.read()
        if binary:
          return contents
        return ToUnicode(contents)
    except IOError as e:
      raise FileNotFoundError('Read failed for %s: %s' % (filename, e))

  def _ListDir(self, dir_name):
    all_files = []
    full_path = os.path.join(self._base_path, dir_name)
    try:
      files = os.listdir(full_path)
    except OSError as e:
      raise FileNotFoundError('os.listdir failed for %s: %s' % (dir_name, e))
    for path in files:
      if path.startswith('.'):
        continue
      if os.path.isdir(os.path.join(full_path, path)):
        all_files.append(path + '/')
      else:
        all_files.append(path)
    return all_files

  def Read(self, paths, binary=False):
    result = {}
    for path in paths:
      if path.endswith('/'):
        result[path] = self._ListDir(_ConvertToFilepath(path))
      else:
        result[path] = self._ReadFile(_ConvertToFilepath(path), binary)
    return Future(value=result)

  def _CreateStatInfo(self, path):
    try:
      path_mtime = os.stat(path).st_mtime
      if path.endswith('/'):
        child_versions = dict(
            (filename, os.stat(os.path.join(path, filename)).st_mtime)
            for filename in os.listdir(path))
        # This file system stat mimics subversion, where the stat of directories
        # is max(file stats). That means we need to recursively check the whole
        # file system tree :\ so approximate that by just checking this dir.
        version = max([path_mtime] + child_versions.values())
      else:
        child_versions = None
        version = path_mtime
      return StatInfo(version, child_versions)
    except OSError as e:
      raise FileNotFoundError('os.stat failed for %s: %s' % (path, e))

  def Stat(self, path):
    return self._CreateStatInfo(os.path.join(self._base_path, path))

  def GetIdentity(self):
    return '@'.join((self.__class__.__name__, StringIdentity(self._base_path)))
