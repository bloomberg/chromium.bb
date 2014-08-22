# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import posixpath
import sys

from file_system import FileSystem, StatInfo, FileNotFoundError
from future import Future
from path_util import IsDirectory, ToDirectory
from third_party.json_schema_compiler.memoize import memoize


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
    # The read caches can start populated (start_empty=False) because file
    # updates are picked up by the stat, so it doesn't need the force-refresh
    # which starting empty is designed for. Without this optimisation, cron
    # runs are extra slow.
    self._read_object_store = create_object_store('read', start_empty=False)

  def Refresh(self):
    return self._file_system.Refresh()

  def StatAsync(self, path):
    '''Stats the directory given, or if a file is given, stats the file's parent
    directory to get info about the file.
    '''
    # Always stat the parent directory, since it will have the stat of the child
    # anyway, and this gives us an entire directory's stat info at once.
    dir_path, file_path = posixpath.split(path)
    dir_path = ToDirectory(dir_path)

    def make_stat_info(dir_stat):
      '''Converts a dir stat into the correct resulting StatInfo; if the Stat
      was for a file, the StatInfo should just contain that file.
      '''
      if path == dir_path:
        return dir_stat
      # Was a file stat. Extract that file.
      file_version = dir_stat.child_versions.get(file_path)
      if file_version is None:
        raise FileNotFoundError('No stat found for %s in %s (found %s)' %
                                (path, dir_path, dir_stat.child_versions))
      return StatInfo(file_version)

    dir_stat = self._stat_object_store.Get(dir_path).Get()
    if dir_stat is not None:
      return Future(value=make_stat_info(dir_stat))

    def next(dir_stat):
      assert dir_stat is not None  # should have raised a FileNotFoundError
      # We only ever need to cache the dir stat.
      self._stat_object_store.Set(dir_path, dir_stat)
      return make_stat_info(dir_stat)
    return self._MemoizedStatAsyncFromFileSystem(dir_path).Then(next)

  @memoize
  def _MemoizedStatAsyncFromFileSystem(self, dir_path):
    '''This is a simple wrapper to memoize Futures to directory stats, since
    StatAsync makes heavy use of it. Only cache directories so that the
    memoized cache doesn't blow up.
    '''
    assert IsDirectory(dir_path)
    return self._file_system.StatAsync(dir_path)

  def Read(self, paths, skip_not_found=False):
    '''Reads a list of files. If a file is in memcache and it is not out of
    date, it is returned. Otherwise, the file is retrieved from the file system.
    '''
    cached_read_values = self._read_object_store.GetMulti(paths).Get()
    cached_stat_values = self._stat_object_store.GetMulti(paths).Get()

    # Populate a map of paths to Futures to their stat. They may have already
    # been cached in which case their Future will already have been constructed
    # with a value.
    stat_futures = {}

    def handle(error):
      if isinstance(error, FileNotFoundError):
        return None
      raise error

    for path in paths:
      stat_value = cached_stat_values.get(path)
      if stat_value is None:
        stat_future = self.StatAsync(path)
        if skip_not_found:
          stat_future = stat_future.Then(lambda x: x, handle)
      else:
        stat_future = Future(value=stat_value)
      stat_futures[path] = stat_future

    # Filter only the cached data which is fresh by comparing to the latest
    # stat. The cached read data includes the cached version. Remove it for
    # the result returned to callers.
    fresh_data = dict(
        (path, data) for path, (data, version) in cached_read_values.iteritems()
        if stat_futures[path].Get().version == version)

    if len(fresh_data) == len(paths):
      # Everything was cached and up-to-date.
      return Future(value=fresh_data)

    def next(new_results):
      # Update the cache. This is a path -> (data, version) mapping.
      self._read_object_store.SetMulti(
          dict((path, (new_result, stat_futures[path].Get().version))
               for path, new_result in new_results.iteritems()))
      new_results.update(fresh_data)
      return new_results
    # Read in the values that were uncached or old.
    return self._file_system.Read(set(paths) - set(fresh_data.iterkeys()),
                                  skip_not_found=skip_not_found).Then(next)

  def GetIdentity(self):
    return self._file_system.GetIdentity()

  def __repr__(self):
    return '%s of <%s>' % (type(self).__name__, repr(self._file_system))
