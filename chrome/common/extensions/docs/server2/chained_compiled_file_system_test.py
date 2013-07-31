#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from chained_compiled_file_system import ChainedCompiledFileSystem
from compiled_file_system import CompiledFileSystem
from object_store_creator import ObjectStoreCreator
from test_file_system import TestFileSystem

_TEST_DATA_BASE = {
  'a.txt': 'base a.txt',
  'dir': {
    'b.txt': 'base b.txt'
  },
}

_TEST_DATA_NEW = {
  'a.txt': 'new a.txt',
  'new.txt': 'a new file',
  'dir': {
    'b.txt': 'new b.txt',
    'new.txt': 'new file in dir',
  },
}

identity = lambda _, x: x

class ChainedCompiledFileSystemTest(unittest.TestCase):
  def setUp(self):
    object_store_creator = ObjectStoreCreator(start_empty=False)
    base_file_system = TestFileSystem(_TEST_DATA_BASE)
    self._base_factory = CompiledFileSystem.Factory(base_file_system,
                                                    object_store_creator)
    self._file_system = TestFileSystem(_TEST_DATA_NEW)
    self._patched_factory = CompiledFileSystem.Factory(self._file_system,
                                                       object_store_creator)
    self._chained_factory = ChainedCompiledFileSystem.Factory(
        [(self._patched_factory, self._file_system),
         (self._base_factory, base_file_system)])
    self._base_compiled_fs = self._base_factory.Create(identity, TestFileSystem)
    self._chained_compiled_fs = self._chained_factory.Create(
        identity, TestFileSystem)

  def testGetFromFile(self):
    self.assertEqual(self._chained_compiled_fs.GetFromFile('a.txt'),
                     self._base_compiled_fs.GetFromFile('a.txt'))
    self.assertEqual(self._chained_compiled_fs.GetFromFile('new.txt'),
                     'a new file')
    self.assertEqual(self._chained_compiled_fs.GetFromFile('dir/new.txt'),
                     'new file in dir')
    self._file_system.IncrementStat('a.txt')
    self.assertNotEqual(self._chained_compiled_fs.GetFromFile('a.txt'),
                        self._base_compiled_fs.GetFromFile('a.txt'))
    self.assertEqual(self._chained_compiled_fs.GetFromFile('a.txt'),
                     self._file_system.ReadSingle('a.txt'))

  def testGetFromFileListing(self):
    self.assertEqual(self._chained_compiled_fs.GetFromFile('dir/'),
                     self._base_compiled_fs.GetFromFile('dir/'))
    self._file_system.IncrementStat('dir/')
    self.assertNotEqual(self._chained_compiled_fs.GetFromFileListing('dir/'),
                        self._base_compiled_fs.GetFromFileListing('dir/'))
    self.assertEqual(self._chained_compiled_fs.GetFromFileListing('dir/'),
                     self._file_system.ReadSingle('dir/'))

if __name__ == '__main__':
  unittest.main()
