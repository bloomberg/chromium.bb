# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

import object_store

APPS                      = 'Apps'
APPS_FS                   = 'AppsFileSystem'
CRON                      = 'Cron'
EXTENSIONS                = 'Extensions'
EXTENSIONS_FS             = 'ExtensionsFileSystem'
CRON_FILE_LISTING         = 'Cron.FileListing'
CRON_GITHUB_INVALIDATION  = 'Cron.GithubInvalidation'
CRON_INVALIDATION         = 'Cron.Invalidation'
HANDLEBAR                 = 'Handlebar'
IDL                       = 'IDL'
IDL_NO_REFS               = 'IDLNoRefs'
IDL_NAMES                 = 'IDLNames'
INTRO                     = 'Intro'
JSON                      = 'JSON'
JSON_NO_REFS              = 'JSONNoRefs'
LIST                      = 'List'
NAMES                     = 'Names'
PERMS                     = 'Perms'
SIDENAV                   = 'Sidenav'
STATIC                    = 'Static'
ZIP                       = 'Zip'

class _CacheEntry(object):
  def __init__(self, cache_data, version):
    self._cache_data = cache_data
    self.version = version

class CompiledFileSystem(object):
  """This class caches FileSystem data that has been processed.
  """
  class Factory(object):
    """A class to build a CompiledFileSystem.
    """
    def __init__(self, file_system, object_store):
      self._file_system = file_system
      self._object_store = object_store

    def Create(self, populate_function, namespace, version=None):
      """Create a CompiledFileSystem that populates the cache by calling
      |populate_function| with (path, data), where |data| is the data that was
      fetched from |path|. The keys to the cache are put in the namespace
      specified by |namespace|, and optionally adding |version|.  """
      return CompiledFileSystem(self._file_system,
                                populate_function,
                                self._object_store,
                                namespace,
                                version=version)

  def __init__(self,
               file_system,
               populate_function,
               object_store,
               namespace,
               version=None):
    self._file_system = file_system
    self._populate_function = populate_function
    self._object_store = object_store
    self._namespace = 'CompiledFileSystem.' + namespace
    if version is not None:
      self._namespace = '%s.%s' % (self._namespace, version)

  def _MakeKey(self, key):
    return self._namespace + '.' + key

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
    cache_entry = self._object_store.Get(self._MakeKey(path),
                                         object_store.FILE_SYSTEM_CACHE,
                                         time=0).Get()
    if (cache_entry is not None) and (version == cache_entry.version):
      return cache_entry._cache_data
    cache_data = self._populate_function(path,
                                         self._file_system.ReadSingle(path))
    self._object_store.Set(self._MakeKey(path),
                           _CacheEntry(cache_data, version),
                           object_store.FILE_SYSTEM_CACHE,
                           time=0)
    return cache_data

  def GetFromFileListing(self, path):
    """Calls |populate_function| on the listing of the files at |path|.
    Assumes that the path given is to a directory.
    """
    if not path.endswith('/'):
      path += '/'
    version = self._file_system.Stat(path).version
    cache_entry = self._object_store.Get(
        self._MakeKey(path),
        object_store.FILE_SYSTEM_CACHE_LISTING,
        time=0).Get()
    if (cache_entry is not None) and (version == cache_entry.version):
        return cache_entry._cache_data
    cache_data = self._populate_function(
        path,
        self._RecursiveList(
            [path + f for f in self._file_system.ReadSingle(path)]))
    self._object_store.Set(self._MakeKey(path),
                           _CacheEntry(cache_data, version),
                           object_store.FILE_SYSTEM_CACHE_LISTING,
                           time=0)
    return cache_data
