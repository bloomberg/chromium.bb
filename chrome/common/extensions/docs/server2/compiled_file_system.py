# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

class _CacheEntry(object):
  def __init__(self, cache_data, version):
    self._cache_data = cache_data
    self.version = version

class CompiledFileSystem(object):
  """This class caches FileSystem data that has been processed.
  """
  class Factory(object):
    """A class to build a CompiledFileSystem backed by |file_system|.
    """
    def __init__(self, file_system, object_store_creator):
      self._file_system = file_system
      self._object_store_creator = object_store_creator

    def Create(self, populate_function, cls, category=None):
      """Create a CompiledFileSystem that populates the cache by calling
      |populate_function| with (path, data), where |data| is the data that was
      fetched from |path|.
      The namespace for the file system is derived like ObjectStoreCreator: from
      |cls| along with an optional |category|.
      """
      assert isinstance(cls, type)
      assert not cls.__name__[0].islower()  # guard against non-class types
      full_name = [cls.__name__, self._file_system.GetIdentity()]
      if category is not None:
        full_name.append(category)
      def create_object_store(my_category):
        return self._object_store_creator.Create(
            CompiledFileSystem, category='/'.join(full_name + [my_category]))
      return CompiledFileSystem(self._file_system,
                                populate_function,
                                create_object_store('file'),
                                create_object_store('list'))

  def __init__(self,
               file_system,
               populate_function,
               file_object_store,
               list_object_store):
    self._file_system = file_system
    self._populate_function = populate_function
    self._file_object_store = file_object_store
    self._list_object_store = list_object_store

  def _RecursiveList(self, path):
    files = []
    for filename in self._file_system.ReadSingle(path):
      if filename.endswith('/'):
        files.extend(['%s%s' % (filename, f)
                      for f in self._RecursiveList('%s%s' % (path, filename))])
      else:
        files.append(filename)
    return files

  def GetFromFile(self, path, binary=False):
    """Calls |populate_function| on the contents of the file at |path|.  If
    |binary| is True then the file will be read as binary - but this will only
    apply for the first time the file is fetched; if already cached, |binary|
    will be ignored.
    """
    version = self._file_system.Stat(path).version
    cache_entry = self._file_object_store.Get(path).Get()
    if (cache_entry is not None) and (version == cache_entry.version):
      return cache_entry._cache_data
    cache_data = self._populate_function(
        path,
        self._file_system.ReadSingle(path, binary=binary))
    self._file_object_store.Set(path, _CacheEntry(cache_data, version))
    return cache_data

  def GetFromFileListing(self, path):
    """Calls |populate_function| on the listing of the files at |path|.
    Assumes that the path given is to a directory.
    """
    if not path.endswith('/'):
      path += '/'
    version = self._file_system.Stat(path).version
    cache_entry = self._list_object_store.Get(path).Get()
    if (cache_entry is not None) and (version == cache_entry.version):
        return cache_entry._cache_data
    cache_data = self._populate_function(path, self._RecursiveList(path))
    self._list_object_store.Set(path, _CacheEntry(cache_data, version))
    return cache_data

  def StatFile(self, path):
    cache_entry = self._file_object_store.Get(path).Get()
    if cache_entry is not None:
      return cache_entry.version
    return self._file_system.Stat(path).version

  def StatFileListing(self, path):
    if not path.endswith('/'):
      path += '/'
    cache_entry = self._list_object_store.Get(path).Get()
    if cache_entry is not None:
      return cache_entry.version
    return self._file_system.Stat(path).version
