# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

class FileSystemCache(object):
  """This class caches FileSystem data that has been processed.
  """
  class Builder(object):
    """A class to build a FileSystemCache.
    """
    def __init__(self, file_system):
      self._file_system = file_system

    def build(self, populate_function):
      return FileSystemCache(self._file_system, populate_function)

  class _CacheEntry(object):
    def __init__(self, cache_data, version):
      self._cache_data = cache_data
      self.version = version

  def __init__(self, file_system, populate_function):
    self._file_system = file_system
    self._populate_function = populate_function
    self._cache = {}

  def _RecursiveList(self, files):
    all_files = files[:]
    dirs = {}
    for filename in files:
      if filename.endswith('/'):
        all_files.remove(filename)
        dirs.update(self._file_system.Read([filename]).Get())
    for dir_, files in dirs.iteritems():
      all_files.extend(self._RecursiveList([dir_ + f for f in files]))
    return all_files

  def GetFromFile(self, path):
    """Calls |populate_function| on the contents of the file at |path|.
    """
    version = self._file_system.Stat(path).version
    if path in self._cache:
      if version > self._cache[path].version:
        self._cache.pop(path)
      else:
        return self._cache[path]._cache_data
    cache_data = self._file_system.ReadSingle(path)
    self._cache[path] = self._CacheEntry(self._populate_function(cache_data),
                                         version)
    return self._cache[path]._cache_data

  def GetFromFileListing(self, path):
    """Calls |populate_function| on the listing of the files at |path|.
    Assumes that the path given is to a directory.
    """
    if not path.endswith('/'):
      path += '/'
    version = self._file_system.Stat(path).version
    if path in self._cache:
      if version > self._cache[path].version:
        self._cache.pop(path)
      else:
        return self._cache[path]._cache_data
    cache_data = self._RecursiveList(
        [path + f for f in self._file_system.ReadSingle(path)])
    self._cache[path] = self._CacheEntry(self._populate_function(cache_data),
                                         version)
    return self._cache[path]._cache_data
