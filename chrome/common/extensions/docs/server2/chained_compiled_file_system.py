# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from compiled_file_system import CompiledFileSystem
from file_system import FileNotFoundError
from future import Gettable, Future


class ChainedCompiledFileSystem(object):
  ''' A CompiledFileSystem implementation that fetches data from a chain of
  CompiledFileSystems that have different file systems and separate cache
  namespaces.

  The rules for the compiled file system chain are:
    - Versions are fetched from the first compiled file system's underlying
      file system.
    - Each compiled file system is read in the reverse order (the last one is
      read first). If the version matches, return the data. Otherwise, read
      from the previous compiled file system until the first one is read.

  It is used to chain compiled file systems whose underlying file systems are
  slightly different. This makes it possible to reuse cached compiled data in
  one of them without recompiling everything that is shared by them.
  '''
  class Factory(CompiledFileSystem.Factory):
    def __init__(self,
                 factory_and_fs_chain):
      self._factory_and_fs_chain = factory_and_fs_chain

    def Create(self, populate_function, cls, category=None):
      return ChainedCompiledFileSystem(
          [(factory.Create(populate_function, cls, category), fs)
           for factory, fs in self._factory_and_fs_chain])

  def __init__(self, compiled_fs_chain):
    '''|compiled_fs_chain| is a list of tuples (compiled_fs, file_system).
    '''
    assert len(compiled_fs_chain) > 0
    self._compiled_fs_chain = compiled_fs_chain

  def GetFromFile(self, path, binary=False):
    return self._GetImpl(
        path,
        lambda compiled_fs: compiled_fs.GetFromFile(path, binary=binary),
        lambda compiled_fs: compiled_fs.StatFile(path))

  def GetFromFileListing(self, path):
    if not path.endswith('/'):
      path += '/'
    return self._GetImpl(
        path,
        lambda compiled_fs: compiled_fs.GetFromFileListing(path),
        lambda compiled_fs: compiled_fs.StatFileListing(path))

  def _GetImpl(self, path, reader, statter):
    # It's possible that a new file is added in the first compiled file system
    # and it doesn't exist in other compiled file systems.
    read_futures = [(reader(compiled_fs), compiled_fs)
                    for compiled_fs, _ in self._compiled_fs_chain]

    def resolve():
      try:
        first_compiled_fs, first_file_system = self._compiled_fs_chain[0]
        # The first file system contains both files of a newer version and
        # files shared with other compiled file systems. We are going to try
        # each compiled file system in the reverse order and return the data
        # when version matches. Data cached in other compiled file system will
        # be reused whenever possible so that we don't need to recompile things
        # that are not changed across these file systems.
        version = first_file_system.Stat(path).version
        for read_future, compiled_fs in reversed(read_futures):
          if statter(compiled_fs) == version:
            return read_future.Get()
      except FileNotFoundError:
        pass
      # Try an arbitrary operation again to generate a realistic stack trace.
      return read_futures[0][0].Get()

    return Future(delegate=Gettable(resolve))
