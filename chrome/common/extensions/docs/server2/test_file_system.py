# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from file_system import FileSystem, FileNotFoundError, StatInfo
from future import Future

class TestFileSystem(FileSystem):
  '''A FileSystem backed by an object. Create with an object representing file
  paths such that {'a': {'b': 'hello'}} will resolve Read('a/b') as 'hello',
  Read('a/') as ['b'], and Stat determined by a value incremented via
  IncrementStat.
  '''
  def __init__(self, obj):
    self._obj = obj
    self._stat = 0

  def Read(self, paths, binary=False):
    try:
      result = {}
      for path in paths:
        result[path] = self._ResolvePath(path)
      return Future(value=result)
    except FileNotFoundError as error:
      return Future(error=error)

  def _ResolvePath(self, path):
    def Resolve(parts):
      '''Resolves |parts| of a path info |self._obj|.
      '''
      result = self._obj.get(parts[0])
      for part in parts[1:]:
        if not isinstance(result, dict):
          raise FileNotFoundError(
              '%s at %s did not resolve to a dict, instead %s' %
              (path, part, result))
        result = result.get(part)
      return result

    def GetPaths(obj):
      '''Lists the paths within |obj|; this is basially keys() but with
      directory paths (i.e. dicts) with a trailing /.
      '''
      def ToPath(k, v):
        if isinstance(v, basestring):
          return k
        if isinstance(v, dict):
          return '%s/' % k
        raise ValueError('Cannot convert type % to path', type(v))
      return [ToPath(k, v) for k, v in obj.items()]

    path = path.lstrip('/')

    if path == '':
      return GetPaths(self._obj)

    parts = path.split('/')
    if parts[-1] != '':
      file_contents = Resolve(parts)
      if not isinstance(file_contents, basestring):
        raise FileNotFoundError(
            '%s (%s) did not resolve to a string, instead %s' %
            (path, parts, file_contents))
      return file_contents

    dir_contents = Resolve(parts[:-1])
    if not isinstance(dir_contents, dict):
      raise FileNotFoundError(
          '%s (%s) did not resolve to a dict, instead %s' %
          (path, parts, dir_contents))

    return GetPaths(dir_contents)

  def Stat(self, path):
    read_result = self.ReadSingle(path)
    stat_result = StatInfo(str(self._stat))
    if isinstance(read_result, list):
      stat_result.child_versions = dict((file_result, str(self._stat))
                                        for file_result in read_result)
    return stat_result

  def IncrementStat(self):
    self._stat += 1
