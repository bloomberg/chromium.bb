#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

import object_store
from in_memory_object_store import InMemoryObjectStore
from file_system import FileSystem, StatInfo
from future import Future
from local_file_system import LocalFileSystem
from memcache_file_system import MemcacheFileSystem

class _FakeFileSystem(FileSystem):
  def __init__(self):
    self.stat_value = 0
    self._stat_count = 0
    self._read_count = 0

  def CheckAndReset(self, read_count=0, stat_count=0):
    try:
      return (self._read_count == read_count and
              self._stat_count == stat_count)
    finally:
      self._read_count = 0
      self._stat_count = 0

  def Stat(self, path):
    self._stat_count += 1
    children = dict((path.strip('/') + str(i), self.stat_value)
                    for i in range(5))
    if not path.endswith('/'):
      children[path.rsplit('/', 1)[-1]] = self.stat_value
    return StatInfo(self.stat_value, children)

  def Read(self, paths, binary=False):
    self._read_count += 1
    return Future(value=dict((path, path) for path in paths))

class MemcacheFileSystemTest(unittest.TestCase):
  def setUp(self):
    self._object_store = InMemoryObjectStore('')
    self._local_fs = LocalFileSystem(os.path.join(sys.path[0],
                                                  'test_data',
                                                  'file_system'))

  def _SetReadCacheItem(self, key, value, stat):
    self._object_store.Set(key, (value, stat), object_store.FILE_SYSTEM_READ)

  def _SetStatCacheItem(self, key, value):
    self._object_store.Set(key, value, object_store.FILE_SYSTEM_STAT)

  def _DeleteReadCacheItem(self, key):
    self._object_store.Delete(key, object_store.FILE_SYSTEM_READ)

  def _DeleteStatCacheItem(self, key):
    self._object_store.Delete(key, object_store.FILE_SYSTEM_STAT)

  def testReadFiles(self):
    file_system = MemcacheFileSystem(self._local_fs, self._object_store)
    expected = {
      './test1.txt': 'test1\n',
      './test2.txt': 'test2\n',
      './test3.txt': 'test3\n',
    }
    self.assertEqual(
        expected,
        file_system.Read(['./test1.txt', './test2.txt', './test3.txt']).Get())

  def testListDir(self):
    file_system = MemcacheFileSystem(self._local_fs, self._object_store)
    expected = ['dir/']
    for i in range(7):
      expected.append('file%d.html' % i)
    self._SetReadCacheItem('list/', expected, file_system.Stat('list/').version)
    self.assertEqual(expected,
                     sorted(file_system.ReadSingle('list/')))
    expected.remove('file0.html')
    self._SetReadCacheItem('list/', expected, file_system.Stat('list/').version)
    self.assertEqual(expected,
                     sorted(file_system.ReadSingle('list/')))

  def testCaching(self):
    fake_fs = _FakeFileSystem()
    file_system = MemcacheFileSystem(fake_fs, self._object_store)
    self.assertEqual('bob/bob0', file_system.ReadSingle('bob/bob0'))
    self.assertTrue(fake_fs.CheckAndReset(read_count=1, stat_count=1))

    # Resource has been cached, so test resource is not re-fetched.
    self.assertEqual('bob/bob0', file_system.ReadSingle('bob/bob0'))
    self.assertTrue(fake_fs.CheckAndReset())

    # Test if the Stat version is the same the resource is not re-fetched.
    self._DeleteStatCacheItem('bob/bob0')
    self.assertEqual('bob/bob0', file_system.ReadSingle('bob/bob0'))
    self.assertTrue(fake_fs.CheckAndReset(stat_count=1))

    # Test if there is a newer version, the resource is re-fetched.
    self._DeleteStatCacheItem('bob/bob0')
    fake_fs.stat_value += 1
    self.assertEqual('bob/bob0', file_system.ReadSingle('bob/bob0'))
    self.assertTrue(fake_fs.CheckAndReset(read_count=1, stat_count=1))

    # Test directory and subdirectory stats are cached.
    self._DeleteStatCacheItem('bob/bob0')
    self._DeleteReadCacheItem('bob/bob0')
    self._DeleteStatCacheItem('bob/bob1')
    self.assertEqual('bob/bob1', file_system.ReadSingle('bob/bob1'))
    self.assertEqual('bob/bob0', file_system.ReadSingle('bob/bob0'))
    self.assertTrue(fake_fs.CheckAndReset(read_count=2, stat_count=1))
    self.assertEqual('bob/bob1', file_system.ReadSingle('bob/bob1'))
    self.assertTrue(fake_fs.CheckAndReset())

    # Test a more recent parent directory doesn't force a refetch of children.
    self._DeleteReadCacheItem('bob/bob0')
    self._DeleteReadCacheItem('bob/bob1')
    self.assertEqual('bob/bob1', file_system.ReadSingle('bob/bob1'))
    self.assertEqual('bob/bob2', file_system.ReadSingle('bob/bob2'))
    self.assertEqual('bob/bob3', file_system.ReadSingle('bob/bob3'))
    self.assertTrue(fake_fs.CheckAndReset(read_count=3))
    self._SetStatCacheItem('bob/', 10)
    self.assertEqual('bob/bob1', file_system.ReadSingle('bob/bob1'))
    self.assertEqual('bob/bob2', file_system.ReadSingle('bob/bob2'))
    self.assertEqual('bob/bob3', file_system.ReadSingle('bob/bob3'))
    self.assertTrue(fake_fs.CheckAndReset())

    self._DeleteStatCacheItem('bob/bob0')
    self.assertEqual('bob/bob0', file_system.ReadSingle('bob/bob0'))
    self.assertTrue(fake_fs.CheckAndReset(read_count=1, stat_count=1))
    self.assertEqual('bob/bob0', file_system.ReadSingle('bob/bob0'))
    self.assertTrue(fake_fs.CheckAndReset())

if __name__ == '__main__':
  unittest.main()
