#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

from caching_file_system import CachingFileSystem
from file_system import FileSystem, StatInfo
from future import Future
from local_file_system import LocalFileSystem
from object_store_creator import ObjectStoreCreator
from test_file_system import TestFileSystem
from test_object_store import TestObjectStore

def _CreateLocalFs():
  return LocalFileSystem(
      os.path.join(sys.path[0], 'test_data', 'file_system'))

class CachingFileSystemTest(unittest.TestCase):
  def setUp(self):
    # Use this to make sure that every time _CreateCachingFileSystem is called
    # the underlying object store data is the same, within each test.
    self._object_store_dbs = {}

  def _CreateCachingFileSystem(self, fs, use_existing_values=False):
    def object_store_constructor(namespace, start_empty=False):
      if namespace not in self._object_store_dbs:
        self._object_store_dbs[namespace] = {}
      db = self._object_store_dbs[namespace]
      if start_empty:
        db.clear()
      return TestObjectStore(namespace, init=db)
    return CachingFileSystem(
        fs,
        ObjectStoreCreator.TestFactory(store_type=object_store_constructor),
        use_existing_values=use_existing_values)

  def testReadFiles(self):
    file_system = CachingFileSystem(_CreateLocalFs(),
                                    ObjectStoreCreator.TestFactory())
    expected = {
      './test1.txt': 'test1\n',
      './test2.txt': 'test2\n',
      './test3.txt': 'test3\n',
    }
    self.assertEqual(
        expected,
        file_system.Read(['./test1.txt', './test2.txt', './test3.txt']).Get())

  def testListDir(self):
    file_system = CachingFileSystem(_CreateLocalFs(),
                                    ObjectStoreCreator.TestFactory())
    expected = ['dir/'] + ['file%d.html' % i for i in range(7)]
    file_system._read_object_store.Set(
        'list/',
        (expected, file_system.Stat('list/').version))
    self.assertEqual(expected, sorted(file_system.ReadSingle('list/')))

    expected.remove('file0.html')
    file_system._read_object_store.Set(
        'list/',
        (expected, file_system.Stat('list/').version))
    self.assertEqual(expected, sorted(file_system.ReadSingle('list/')))

  def testCaching(self):
    fake_fs = TestFileSystem({
      'bob': {
        'bob0': 'bob/bob0 contents',
        'bob1': 'bob/bob1 contents',
        'bob2': 'bob/bob2 contents',
        'bob3': 'bob/bob3 contents',
      }
    })
    file_system = self._CreateCachingFileSystem(fake_fs)

    self.assertEqual('bob/bob0 contents', file_system.ReadSingle('bob/bob0'))
    self.assertTrue(*fake_fs.CheckAndReset(read_count=1, stat_count=1))

    # Resource has been cached, so test resource is not re-fetched.
    self.assertEqual('bob/bob0 contents', file_system.ReadSingle('bob/bob0'))
    self.assertTrue(*fake_fs.CheckAndReset())

    # Test if the Stat version is the same the resource is not re-fetched.
    file_system = self._CreateCachingFileSystem(fake_fs)
    self.assertEqual('bob/bob0 contents', file_system.ReadSingle('bob/bob0'))
    self.assertTrue(*fake_fs.CheckAndReset(stat_count=1))

    # Test if there is a newer version, the resource is re-fetched.
    file_system = self._CreateCachingFileSystem(fake_fs)
    fake_fs.IncrementStat();
    self.assertEqual('bob/bob0 contents', file_system.ReadSingle('bob/bob0'))
    self.assertTrue(*fake_fs.CheckAndReset(read_count=1, stat_count=1))

    # Test directory and subdirectory stats are cached.
    file_system = self._CreateCachingFileSystem(fake_fs)
    file_system._stat_object_store.Del('bob/bob0')
    file_system._read_object_store.Del('bob/bob0')
    file_system._stat_object_store.Del('bob/bob1')
    fake_fs.IncrementStat();
    self.assertEqual('bob/bob1 contents', file_system.ReadSingle('bob/bob1'))
    self.assertEqual('bob/bob0 contents', file_system.ReadSingle('bob/bob0'))
    self.assertTrue(*fake_fs.CheckAndReset(read_count=2, stat_count=1))
    self.assertEqual('bob/bob1 contents', file_system.ReadSingle('bob/bob1'))
    self.assertTrue(*fake_fs.CheckAndReset())

    # Test a more recent parent directory doesn't force a refetch of children.
    file_system = self._CreateCachingFileSystem(fake_fs)
    file_system._read_object_store.Del('bob/bob0')
    file_system._read_object_store.Del('bob/bob1')
    self.assertEqual('bob/bob1 contents', file_system.ReadSingle('bob/bob1'))
    self.assertEqual('bob/bob2 contents', file_system.ReadSingle('bob/bob2'))
    self.assertEqual('bob/bob3 contents', file_system.ReadSingle('bob/bob3'))
    self.assertTrue(*fake_fs.CheckAndReset(read_count=3, stat_count=1))

    fake_fs.IncrementStat(path='bob/')
    file_system = self._CreateCachingFileSystem(fake_fs)
    self.assertEqual('bob/bob1 contents', file_system.ReadSingle('bob/bob1'))
    self.assertEqual('bob/bob2 contents', file_system.ReadSingle('bob/bob2'))
    self.assertEqual('bob/bob3 contents', file_system.ReadSingle('bob/bob3'))
    self.assertTrue(*fake_fs.CheckAndReset(stat_count=1))

    file_system = self._CreateCachingFileSystem(fake_fs)
    file_system._stat_object_store.Del('bob/bob0')
    self.assertEqual('bob/bob0 contents', file_system.ReadSingle('bob/bob0'))
    self.assertTrue(*fake_fs.CheckAndReset(read_count=1, stat_count=1))
    self.assertEqual('bob/bob0 contents', file_system.ReadSingle('bob/bob0'))
    self.assertTrue(*fake_fs.CheckAndReset())

  def testCachedStat(self):
    fake_fs = TestFileSystem({
      'bob': {
        'bob0': 'bob/bob0 contents',
        'bob1': 'bob/bob1 contents'
      }
    })
    file_system = self._CreateCachingFileSystem(fake_fs,
                                                use_existing_values=True)

    self.assertEqual(StatInfo('0'), file_system.Stat('bob/bob0'))
    self.assertTrue(*fake_fs.CheckAndReset(stat_count=1))
    self.assertEqual(StatInfo('0'), file_system.Stat('bob/bob0'))
    self.assertTrue(*fake_fs.CheckAndReset())

    # Caching happens on a directory basis, so reading other files from that
    # directory won't result in a stat.
    self.assertEqual(StatInfo('0'), file_system.Stat('bob/bob1'))
    self.assertEqual(
        StatInfo('0', child_versions={'bob0': '0', 'bob1': '0'}),
        file_system.Stat('bob/'))
    self.assertTrue(*fake_fs.CheckAndReset())

    # Even though the stat is bumped, the object store still has it cached so
    # this won't update.
    fake_fs.IncrementStat()
    self.assertEqual(StatInfo('0'), file_system.Stat('bob/bob0'))
    self.assertEqual(StatInfo('0'), file_system.Stat('bob/bob1'))
    self.assertEqual(
        StatInfo('0', child_versions={'bob0': '0', 'bob1': '0'}),
        file_system.Stat('bob/'))
    self.assertTrue(*fake_fs.CheckAndReset())

  def testFreshStat(self):
    fake_fs = TestFileSystem({
      'bob': {
        'bob0': 'bob/bob0 contents',
        'bob1': 'bob/bob1 contents'
      }
    })

    def run_expecting_stat(stat):
      def run():
        file_system = self._CreateCachingFileSystem(fake_fs)
        self.assertEqual(
            StatInfo(stat, child_versions={'bob0': stat, 'bob1': stat}),
            file_system.Stat('bob/'))
        self.assertTrue(*fake_fs.CheckAndReset(stat_count=1))
        self.assertEqual(StatInfo(stat), file_system.Stat('bob/bob0'))
        self.assertEqual(StatInfo(stat), file_system.Stat('bob/bob0'))
        self.assertTrue(*fake_fs.CheckAndReset())
      run()
      run()

    run_expecting_stat('0')
    fake_fs.IncrementStat()
    run_expecting_stat('1')

if __name__ == '__main__':
  unittest.main()
