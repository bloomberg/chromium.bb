#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import unittest

import appengine_memcache as memcache
from appengine_memcache import AppEngineMemcache
from file_system import FileSystem
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
    return self.StatInfo(
        self.stat_value,
        dict((path + str(i), self.stat_value) for i in range(5)))

  def Read(self, paths, binary=False):
    self._read_count += 1
    return Future(value=dict((path, path) for path in paths))

class MemcacheFileSystemTest(unittest.TestCase):
  def setUp(self):
    self._memcache = AppEngineMemcache('')
    self._local_fs = LocalFileSystem(os.path.join('test_data', 'file_system'))

  def _SetReadCacheItem(self, key, value, stat):
    self._memcache.Set(key, (value, stat), memcache.MEMCACHE_FILE_SYSTEM_READ)

  def _SetStatCacheItem(self, key, value):
    self._memcache.Set(key, value, memcache.MEMCACHE_FILE_SYSTEM_STAT)

  def _DeleteReadCacheItem(self, key):
    self._memcache.Delete(key, memcache.MEMCACHE_FILE_SYSTEM_READ)

  def _DeleteStatCacheItem(self, key):
    self._memcache.Delete(key, memcache.MEMCACHE_FILE_SYSTEM_STAT)

  def testReadFiles(self):
    file_system = MemcacheFileSystem(self._local_fs, self._memcache)
    expected = {
      'test1.txt': 'test1\n',
      'test2.txt': 'test2\n',
      'test3.txt': 'test3\n',
    }
    self.assertEqual(
        expected,
        file_system.Read(['test1.txt', 'test2.txt', 'test3.txt']).Get())

  def testListDir(self):
    file_system = MemcacheFileSystem(self._local_fs, self._memcache)
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
    file_system = MemcacheFileSystem(fake_fs, self._memcache)
    self.assertEqual('bob', file_system.ReadSingle('bob'))
    self.assertTrue(fake_fs.CheckAndReset(read_count=1, stat_count=1))

    # Resource has been memcached, so test resource is not re-fetched.
    self.assertEqual('bob', file_system.ReadSingle('bob'))
    self.assertTrue(fake_fs.CheckAndReset())

    # Test if the Stat version is the same the resource is not re-fetched.
    self._DeleteStatCacheItem('bob')
    self.assertEqual('bob', file_system.ReadSingle('bob'))
    self.assertTrue(fake_fs.CheckAndReset(stat_count=1))

    # Test if there is a newer version, the resource is re-fetched.
    self._DeleteStatCacheItem('bob')
    fake_fs.stat_value += 1
    self.assertEqual('bob', file_system.ReadSingle('bob'))
    self.assertTrue(fake_fs.CheckAndReset(read_count=1, stat_count=1))

    # Test directory and subdirectory stats are cached.
    self.assertEqual('bob/', file_system.ReadSingle('bob/'))
    self.assertEqual('bob/bob0', file_system.ReadSingle('bob/bob0'))
    self.assertTrue(fake_fs.CheckAndReset(read_count=2, stat_count=1))
    self.assertEqual('bob/', file_system.ReadSingle('bob/'))
    self.assertTrue(fake_fs.CheckAndReset())

    # Test a more recent parent directory doesn't force a refetch of children.
    self.assertEqual('bob/bob1', file_system.ReadSingle('bob/bob1'))
    self.assertEqual('bob/bob2', file_system.ReadSingle('bob/bob2'))
    self.assertEqual('bob/bob3', file_system.ReadSingle('bob/bob3'))
    self.assertTrue(fake_fs.CheckAndReset(read_count=3))
    self._SetStatCacheItem('bob/', 10)
    self.assertEqual('bob/bob0', file_system.ReadSingle('bob/bob0'))
    self.assertEqual('bob/bob1', file_system.ReadSingle('bob/bob1'))
    self.assertEqual('bob/bob2', file_system.ReadSingle('bob/bob2'))
    self.assertTrue(fake_fs.CheckAndReset())

    self._DeleteStatCacheItem('bob/bob0')
    self.assertEqual('bob/bob0', file_system.ReadSingle('bob/bob0'))
    self.assertTrue(fake_fs.CheckAndReset(stat_count=1))
    self.assertEqual('bob/bob0', file_system.ReadSingle('bob/bob0'))
    self.assertTrue(fake_fs.CheckAndReset())

if __name__ == '__main__':
  unittest.main()
