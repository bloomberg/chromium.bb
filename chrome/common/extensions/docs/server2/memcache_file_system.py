# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from file_system import FileSystem, StatInfo, FileNotFoundError
from future import Future
import object_store

class _AsyncUncachedFuture(object):
  def __init__(self,
               uncached,
               current_result,
               file_system,
               object_store,
               namespace):
    self._uncached = uncached
    self._current_result = current_result
    self._file_system = file_system
    self._object_store = object_store
    self._namespace = namespace

  def Get(self):
    mapping = {}
    new_items = self._uncached.Get()
    for item in new_items:
      version = self._file_system.Stat(item).version
      mapping[item] = (new_items[item], version)
      self._current_result[item] = new_items[item]
    self._object_store.SetMulti(mapping, self._namespace, time=0)
    return self._current_result

class MemcacheFileSystem(FileSystem):
  """FileSystem implementation which memcaches the results of Read.
  """
  def __init__(self, file_system, object_store):
    self._file_system = file_system
    self._object_store = object_store

  def Stat(self, path, stats=None):
    """Stats the directory given, or if a file is given, stats the files parent
    directory to get info about the file.
    """
    # TODO(kalman): store the whole stat info, not just the version.
    version = self._object_store.Get(path, object_store.FILE_SYSTEM_STAT).Get()
    if version is not None:
      return StatInfo(version)

    # Always stat the parent directory, since it will have the stat of the child
    # anyway, and this gives us an entire directory's stat info at once.
    if path.endswith('/'):
      dir_path = path
    else:
      dir_path = path.rsplit('/', 1)[0] + '/'

    dir_stat = self._file_system.Stat(dir_path)
    if path == dir_path:
      version = dir_stat.version
    else:
      version = dir_stat.child_versions.get(path.split('/')[-1], None)
      if version is None:
        raise FileNotFoundError(path)
    mapping = { path: version }

    for child_path, child_version in dir_stat.child_versions.iteritems():
      child_path = dir_path + child_path
      mapping[child_path] = child_version
    self._object_store.SetMulti(mapping, object_store.FILE_SYSTEM_STAT)
    return StatInfo(version)

  def Read(self, paths, binary=False):
    """Reads a list of files. If a file is in memcache and it is not out of
    date, it is returned. Otherwise, the file is retrieved from the file system.
    """
    result = {}
    uncached = []
    namespace = object_store.FILE_SYSTEM_READ
    if binary:
      namespace = '%s.binary' % namespace
    results = self._object_store.GetMulti(paths, namespace, time=0).Get()
    result_values = [x[1] for x in sorted(results.iteritems())]
    stats = self._object_store.GetMulti(paths,
                                        object_store.FILE_SYSTEM_STAT).Get()
    stat_values = [x[1] for x in sorted(stats.iteritems())]
    for path, cached_result, stat in zip(sorted(paths),
                                         result_values,
                                         stat_values):
      if cached_result is None:
        uncached.append(path)
        continue
      data, version = cached_result
      # TODO(cduvall): Make this use a multi stat.
      if stat is None:
        stat = self.Stat(path).version
      if stat != version:
        uncached.append(path)
        continue
      result[path] = data

    if not uncached:
      return Future(value=result)
    return Future(delegate=_AsyncUncachedFuture(
        self._file_system.Read(uncached, binary=binary),
        result,
        self,
        self._object_store,
        namespace))
