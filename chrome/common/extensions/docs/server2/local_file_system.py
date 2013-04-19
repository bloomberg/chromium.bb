# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from file_system import FileSystem, FileNotFoundError, StatInfo, ToUnicode
from future import Future

class LocalFileSystem(FileSystem):
  """FileSystem implementation which fetches resources from the local
  filesystem.
  """
  def __init__(self, base_path):
    self._base_path = self._ConvertToFilepath(base_path)

  def _ConvertToFilepath(self, path):
    return path.replace('/', os.sep)

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
        result[path] = self._ListDir(self._ConvertToFilepath(path))
      else:
        result[path] = self._ReadFile(self._ConvertToFilepath(path), binary)
    return Future(value=result)

  def _CreateStatInfo(self, path):
    if path.endswith('/'):
      versions = dict((filename, os.stat(os.path.join(path, filename)).st_mtime)
                      for filename in os.listdir(path))
    else:
      versions = None
    try:
      return StatInfo(os.stat(path).st_mtime, versions)
    except OSError as e:
      raise FileNotFoundError('os.stat failed for %s: %s' % (path, e))

  def Stat(self, path):
    return self._CreateStatInfo(os.path.join(self._base_path, path))
