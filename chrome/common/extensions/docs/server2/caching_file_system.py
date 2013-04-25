# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from file_system import FileSystem, StatInfo, FileNotFoundError
from future import Future

class _AsyncUncachedFuture(object):
  def __init__(self,
               uncached,
               current_result,
               file_system,
               object_store):
    self._uncached = uncached
    self._current_result = current_result
    self._file_system = file_system
    self._object_store = object_store

  def Get(self):
    mapping = {}
    new_items = self._uncached.Get()
    for item in new_items:
      version = self._file_system.Stat(item).version
      mapping[item] = (new_items[item], version)
      self._current_result[item] = new_items[item]
    self._object_store.SetMulti(mapping)
    return self._current_result

class CachingFileSystem(FileSystem):
  """FileSystem implementation which caches its results in an object store.
  """
  def __init__(self, file_system, object_store_creator_factory):
    self._file_system = file_system
    def create_object_store(category):
      return (object_store_creator_factory.Create(CachingFileSystem)
          .Create(category='%s/%s' % (file_system.GetName(), category)))
    self._stat_object_store = create_object_store('stat')
    self._read_object_store = create_object_store('read')
    self._read_binary_object_store = create_object_store('read-binary')

  def Stat(self, path, stats=None):
    """Stats the directory given, or if a file is given, stats the file's parent
    directory to get info about the file.
    """
    # TODO(kalman): store the whole stat info, not just the version.
    version = self._stat_object_store.Get(path).Get()
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
        raise FileNotFoundError('No stat found for %s in %s' % (path, dir_path))
    mapping = { path: version }

    for child_path, child_version in dir_stat.child_versions.iteritems():
      child_path = dir_path + child_path
      mapping[child_path] = child_version
    self._stat_object_store.SetMulti(mapping)
    return StatInfo(version)

  def Read(self, paths, binary=False):
    """Reads a list of files. If a file is in memcache and it is not out of
    date, it is returned. Otherwise, the file is retrieved from the file system.
    """
    read_object_store = (self._read_binary_object_store if binary else
                         self._read_object_store)
    read_values = read_object_store.GetMulti(paths).Get()
    stat_values = self._stat_object_store.GetMulti(paths).Get()
    result = {}
    uncached = []
    for path in paths:
      read_value = read_values.get(path)
      stat_value = stat_values.get(path)
      if read_value is None:
        uncached.append(path)
        continue
      data, version = read_value
      # TODO(cduvall): Make this use a multi stat.
      if stat_value is None:
        stat_value = self.Stat(path).version
      if stat_value != version:
        uncached.append(path)
        continue
      result[path] = data

    if not uncached:
      return Future(value=result)

    return Future(delegate=_AsyncUncachedFuture(
        self._file_system.Read(uncached, binary=binary),
        result,
        self,
        read_object_store))
