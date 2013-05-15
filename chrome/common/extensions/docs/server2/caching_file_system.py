# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from file_system import FileSystem, StatInfo, FileNotFoundError
from future import Future
from object_store_creator import ObjectStoreCreator

class _AsyncUncachedFuture(object):
  def __init__(self,
               uncached_read_futures,
               stats_for_uncached,
               current_results,
               file_system,
               object_store):
    self._uncached_read_futures = uncached_read_futures
    self._stats_for_uncached = stats_for_uncached
    self._current_results = current_results
    self._file_system = file_system
    self._object_store = object_store

  def Get(self):
    new_results = self._uncached_read_futures.Get()
    # Update the cached data in the object store. This is a path -> (read,
    # version) mapping.
    self._object_store.SetMulti(dict(
        (path, (new_result, self._stats_for_uncached[path].version))
        for path, new_result in new_results.iteritems()))
    new_results.update(self._current_results)
    return new_results

class CachingFileSystem(FileSystem):
  '''FileSystem which implements a caching layer on top of |file_system|. It's
  smart, using Stat() to decided whether to skip Read()ing from |file_system|,
  and only Stat()ing directories never files.
  '''
  def __init__(self, file_system, object_store_creator):
    self._file_system = file_system
    def create_object_store(category, **optargs):
      return object_store_creator.Create(
          CachingFileSystem,
          category='%s/%s' % (file_system.GetIdentity(), category),
          **optargs)
    self._stat_object_store = create_object_store('stat')
    # The read caches can both (a) start populated and (b) be shared with all
    # other app versions, because the data changing is detected by the stat.
    # Without this optimisation, bumping app version is extremely slow.
    self._read_object_store = create_object_store(
        'read', start_empty=False, app_version=None)
    self._read_binary_object_store = create_object_store(
        'read-binary', start_empty=False, app_version=None)

  def Stat(self, path):
    '''Stats the directory given, or if a file is given, stats the file's parent
    directory to get info about the file.
    '''
    # Always stat the parent directory, since it will have the stat of the child
    # anyway, and this gives us an entire directory's stat info at once.
    if path.endswith('/'):
      dir_path = path
    else:
      dir_path, file_path = path.rsplit('/', 1)
      dir_path += '/'

    # ... and we only ever need to cache the dir stat, too.
    dir_stat = self._stat_object_store.Get(dir_path).Get()
    if dir_stat is None:
      dir_stat = self._file_system.Stat(dir_path)
      assert dir_stat is not None  # should raise a FileNotFoundError
      self._stat_object_store.Set(dir_path, dir_stat)

    if path == dir_path:
      stat_info = dir_stat
    else:
      file_version = dir_stat.child_versions.get(file_path)
      if file_version is None:
        raise FileNotFoundError('No stat found for %s in %s' % (path, dir_path))
      stat_info = StatInfo(file_version)

    return stat_info

  def Read(self, paths, binary=False):
    '''Reads a list of files. If a file is in memcache and it is not out of
    date, it is returned. Otherwise, the file is retrieved from the file system.
    '''
    read_object_store = (self._read_binary_object_store if binary else
                         self._read_object_store)
    read_values = read_object_store.GetMulti(paths).Get()
    stat_values = self._stat_object_store.GetMulti(paths).Get()
    results = {}  # maps path to read value
    uncached = {}  # maps path to stat value
    for path in paths:
      stat_value = stat_values.get(path)
      if stat_value is None:
        # TODO(cduvall): do a concurrent Stat with the missing stat values.
        stat_value = self.Stat(path)
      read_value = read_values.get(path)
      if read_value is None:
        uncached[path] = stat_value
        continue
      read_data, read_version = read_value
      if stat_value.version != read_version:
        uncached[path] = stat_value
        continue
      results[path] = read_data

    if not uncached:
      return Future(value=results)

    return Future(delegate=_AsyncUncachedFuture(
        self._file_system.Read(uncached.keys(), binary=binary),
        uncached,
        results,
        self,
        read_object_store))

  def GetIdentity(self):
    return self._file_system.GetIdentity()
