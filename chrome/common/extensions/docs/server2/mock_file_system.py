# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from file_system import FileSystem

class MockFileSystem(FileSystem):
  '''Wraps a FileSystem to add simple mock behaviour - asserting how often
  Stat/Read calls are being made to it. The Read/Stat implementations
  themselves are provided by a delegate FileSystem.
  '''
  def __init__(self, file_system):
    self._file_system = file_system
    self._read_count = 0
    self._stat_count = 0

  #
  # FileSystem implementation.
  #

  def Read(self, paths, binary=False):
    self._read_count += 1
    return self._file_system.Read(paths, binary=binary)

  def Stat(self, path):
    self._stat_count += 1
    return self._file_system.Stat(path)

  def GetIdentity(self):
    return self._file_system.GetIdentity()

  def __str__(self):
    return repr(self)

  def __repr__(self):
    return 'MockFileSystem(read_count=%s, stat_count=%s)' % (
        self._read_count, self._stat_count)

  #
  # Testing methods.
  #

  def GetReadCount(self):
    return self._read_count

  def GetStatCount(self):
    return self._stat_count

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
