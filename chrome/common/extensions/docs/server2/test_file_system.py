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

  # TODO(kalman): this method would be unnecessary if we injected paths properly
  # in ServerInstance.
  @staticmethod
  def MoveTo(base, obj):
    '''Returns an object as |obj| moved to |prefix|. That is,
    MoveTo('foo/bar', {'a': 'b'}) -> {'foo': {'bar': {'a': 'b'}}}
    '''
    result = {}
    leaf = result
    for k in base.split('/'):
      leaf[k] = {}
      leaf = leaf[k]
    leaf.update(obj)
    return result

  def __init__(self, obj):
    self._obj = obj
    self._global_stat = 0
    self._path_stats = {}
    self._read_count = 0
    self._stat_count = 0

  #
  # FileSystem implementation.
  #

  def Read(self, paths, binary=False):
    self._read_count += 1
    return self._ReadImpl(paths, binary=binary)

  def _ReadImpl(self, paths, binary=False):
    test_fs = self
    class Delegate(object):
      def Get(self):
        return dict((path, test_fs._ResolvePath(path)) for path in paths)
    return Future(delegate=Delegate())

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
        raise ValueError('Cannot convert type %s to path', type(v))
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
    self._stat_count += 1
    return self._StatImpl(path)

  def _StatImpl(self, path):
    read_result = self._ReadImpl([path]).Get().get(path)
    stat_result = StatInfo(self._SinglePathStat(path))
    if isinstance(read_result, list):
      stat_result.child_versions = dict(
          (file_result, self._SinglePathStat('%s%s' % (path, file_result)))
          for file_result in read_result)
    return stat_result

  def _SinglePathStat(self, path):
    return str(self._global_stat + self._path_stats.get(path, 0))

  #
  # Testing methods.
  #

  def IncrementStat(self, path=None):
    if path is not None:
      self._path_stats[path] = self._path_stats.get(path, 0) + 1
    else:
      self._global_stat += 1

  def CheckAndReset(self, stat_count=0, read_count=0):
    '''Returns a tuple (success, error). Use in tests like:
    self.assertTrue(*object_store.CheckAndReset(...))
    '''
    errors = []
    for desc, expected, actual in (
        ('read_count', read_count, self._read_count),
        ('stat_count', stat_count, self._stat_count)):
      if actual != expected:
        errors.append('%s: expected %s got %s' % (desc, expected, actual))
    try:
      return (len(errors) == 0, ', '.join(errors))
    finally:
      self.Reset()

  def Reset(self):
    self._read_count = 0
    self._stat_count = 0
