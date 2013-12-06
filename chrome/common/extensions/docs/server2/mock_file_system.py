# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from file_system import FileSystem, FileNotFoundError
from future import Gettable, Future
from test_file_system import TestFileSystem

class MockFileSystem(FileSystem):
  '''Wraps FileSystems to add a selection of mock behaviour:

  - asserting how often Stat/Read calls are being made to it.
  - primitive changes/versioning via applying object "diffs", mapping paths to
    new content (similar to how TestFileSystem works).
  '''
  def __init__(self, file_system):
    self._file_system = file_system
    # Updates are modelled are stored as TestFileSystems because they've
    # implemented a bunch of logic to interpret paths into dictionaries.
    self._updates = []
    self._read_count = 0
    self._read_resolve_count = 0
    self._stat_count = 0

  @staticmethod
  def Create(file_system, updates):
    mock_file_system = MockFileSystem(file_system)
    for update in updates:
      mock_file_system.Update(update)
    return mock_file_system

  #
  # FileSystem implementation.
  #

  def Read(self, paths):
    '''Reads |paths| from |_file_system|, then applies the most recent update
    from |_updates|, if any.
    '''
    self._read_count += 1
    future_result = self._file_system.Read(paths)
    def resolve():
      self._read_resolve_count += 1
      result = future_result.Get()
      for path in result.iterkeys():
        _, update = self._GetMostRecentUpdate(path)
        if update is not None:
          result[path] = update
      return result
    return Future(delegate=Gettable(resolve))

  def Refresh(self):
    return self._file_system.Refresh()

  def _GetMostRecentUpdate(self, path):
    for revision, update in reversed(list(enumerate(self._updates))):
      try:
        return (revision + 1, update.ReadSingle(path).Get())
      except FileNotFoundError:
        pass
    return (0, None)

  def Stat(self, path):
    self._stat_count += 1
    return self._StatImpl(path)

  def _StatImpl(self, path):
    result = self._file_system.Stat(path)
    result.version = self._UpdateStat(result.version, path)
    child_versions = result.child_versions
    if child_versions is not None:
      for child_path in child_versions.iterkeys():
        child_versions[child_path] = self._UpdateStat(
            child_versions[child_path],
            '%s%s' % (path, child_path))
    return result

  def _UpdateStat(self, version, path):
    if not path.endswith('/'):
      return str(int(version) + self._GetMostRecentUpdate(path)[0])
    # Bleh, it's a directory, need to recursively search all the children.
    child_paths = self._file_system.ReadSingle(path).Get()
    if not child_paths:
      return version
    return str(max([int(version)] +
                   [int(self._StatImpl('%s%s' % (path, child_path)).version)
                    for child_path in child_paths]))

  def GetIdentity(self):
    return self._file_system.GetIdentity()

  def __str__(self):
    return repr(self)

  def __repr__(self):
    return 'MockFileSystem(read_count=%s, stat_count=%s, updates=%s)' % (
        self._read_count, self._stat_count, len(self._updates))

  #
  # Testing methods.
  #

  def GetStatCount(self):
    return self._stat_count

  def CheckAndReset(self, stat_count=0, read_count=0, read_resolve_count=0):
    '''Returns a tuple (success, error). Use in tests like:
    self.assertTrue(*object_store.CheckAndReset(...))
    '''
    errors = []
    for desc, expected, actual in (
        ('read_count', read_count, self._read_count),
        ('read_resolve_count', read_resolve_count, self._read_resolve_count),
        ('stat_count', stat_count, self._stat_count)):
      if actual != expected:
        errors.append('%s: expected %s got %s' % (desc, expected, actual))
    try:
      return (len(errors) == 0, ', '.join(errors))
    finally:
      self.Reset()

  def Reset(self):
    self._read_count = 0
    self._read_resolve_count = 0
    self._stat_count = 0

  def Update(self, update):
    self._updates.append(TestFileSystem(update))
