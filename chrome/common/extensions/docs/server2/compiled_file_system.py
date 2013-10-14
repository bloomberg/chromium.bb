# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

from file_system import FileNotFoundError
from future import Gettable, Future


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
    def __init__(self, object_store_creator):
      self._object_store_creator = object_store_creator

    def Create(self, file_system, populate_function, cls, category=None):
      """Creates a CompiledFileSystem view over |file_system| that populates
      its cache by calling |populate_function| with (path, data), where |data|
      is the data that was fetched from |path| in |file_system|.

      The namespace for the compiled file system is derived similar to
      ObjectStoreCreator: from |cls| along with an optional |category|.
      """
      assert isinstance(cls, type)
      assert not cls.__name__[0].islower()  # guard against non-class types
      full_name = [cls.__name__, file_system.GetIdentity()]
      if category is not None:
        full_name.append(category)
      def create_object_store(my_category):
        return self._object_store_creator.Create(
            CompiledFileSystem, category='/'.join(full_name + [my_category]))
      return CompiledFileSystem(file_system,
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
    '''Returns a Future containing the recursive directory listing of |path| as
    a flat list of paths.
    '''
    def split_dirs_from_files(paths):
      '''Returns a tuple (dirs, files) where |dirs| contains the directory
      names in |paths| and |files| contains the files.
      '''
      result = [], []
      for path in paths:
        result[0 if path.endswith('/') else 1].append(path)
      return result

    def add_prefix(prefix, paths):
      return [prefix + path for path in paths]

    # Read in the initial list of files. Do this eagerly (i.e. not part of the
    # asynchronous Future contract) because there's a greater chance to
    # parallelise fetching with the second layer (can fetch multiple paths).
    try:
      first_layer_dirs, first_layer_files = split_dirs_from_files(
          self._file_system.ReadSingle(path).Get())
    except FileNotFoundError:
      return Future(exc_info=sys.exc_info())

    if not first_layer_dirs:
      return Future(value=first_layer_files)

    second_layer_listing = self._file_system.Read(
        add_prefix(path, first_layer_dirs))

    def resolve():
      def get_from_future_listing(futures):
        '''Recursively lists files from directory listing |futures|.
        '''
        dirs, files = [], []
        for dir_name, listing in futures.Get().iteritems():
          new_dirs, new_files = split_dirs_from_files(listing)
          # |dirs| are paths for reading. Add the full prefix relative to
          # |path| so that |file_system| can find the files.
          dirs += add_prefix(dir_name, new_dirs)
          # |files| are not for reading, they are for returning to the caller.
          # This entire function set (i.e. GetFromFileListing) is defined to
          # not include the fetched-path in the result, however, |dir_name|
          # will be prefixed with |path|. Strip it.
          assert dir_name.startswith(path)
          files += add_prefix(dir_name[len(path):], new_files)
        if dirs:
          files += get_from_future_listing(self._file_system.Read(dirs))
        return files

      return first_layer_files + get_from_future_listing(second_layer_listing)

    return Future(delegate=Gettable(resolve))

  def GetFromFile(self, path, binary=False):
    """Calls |populate_function| on the contents of the file at |path|.  If
    |binary| is True then the file will be read as binary - but this will only
    apply for the first time the file is fetched; if already cached, |binary|
    will be ignored.
    """
    try:
      version = self._file_system.Stat(path).version
    except FileNotFoundError:
      return Future(exc_info=sys.exc_info())

    cache_entry = self._file_object_store.Get(path).Get()
    if (cache_entry is not None) and (version == cache_entry.version):
      return Future(value=cache_entry._cache_data)

    future_files = self._file_system.Read([path], binary=binary)
    def resolve():
      cache_data = self._populate_function(path, future_files.Get().get(path))
      self._file_object_store.Set(path, _CacheEntry(cache_data, version))
      return cache_data
    return Future(delegate=Gettable(resolve))

  def GetFromFileListing(self, path):
    """Calls |populate_function| on the listing of the files at |path|.
    Assumes that the path given is to a directory.
    """
    if not path.endswith('/'):
      path += '/'

    try:
      version = self._file_system.Stat(path).version
    except FileNotFoundError:
      return Future(exc_info=sys.exc_info())

    cache_entry = self._list_object_store.Get(path).Get()
    if (cache_entry is not None) and (version == cache_entry.version):
      return Future(value=cache_entry._cache_data)

    recursive_list_future = self._RecursiveList(path)
    def resolve():
      cache_data = self._populate_function(path, recursive_list_future.Get())
      self._list_object_store.Set(path, _CacheEntry(cache_data, version))
      return cache_data
    return Future(delegate=Gettable(resolve))

  def GetFileVersion(self, path):
    cache_entry = self._file_object_store.Get(path).Get()
    if cache_entry is not None:
      return cache_entry.version
    return self._file_system.Stat(path).version

  def GetFileListingVersion(self, path):
    if not path.endswith('/'):
      path += '/'
    cache_entry = self._list_object_store.Get(path).Get()
    if cache_entry is not None:
      return cache_entry.version
    return self._file_system.Stat(path).version
