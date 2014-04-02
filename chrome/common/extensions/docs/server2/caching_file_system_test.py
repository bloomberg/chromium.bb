#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

from caching_file_system import CachingFileSystem
from extensions_paths import SERVER2
from file_system import  StatInfo
from local_file_system import LocalFileSystem
from mock_file_system import MockFileSystem
from object_store_creator import ObjectStoreCreator
from test_file_system import TestFileSystem
from test_object_store import TestObjectStore


def _CreateLocalFs():
  return LocalFileSystem.Create(SERVER2, 'test_data', 'file_system/')


class CachingFileSystemTest(unittest.TestCase):
  def setUp(self):
    # Use this to make sure that every time _CreateCachingFileSystem is called
    # the underlying object store data is the same, within each test.
    self._object_store_dbs = {}

  def _CreateCachingFileSystem(self, fs, start_empty=False):
    def store_type_constructor(namespace, start_empty=False):
      '''Returns an ObjectStore backed onto test-lifetime-persistent objects
      in |_object_store_dbs|.
      '''
      if namespace not in self._object_store_dbs:
        self._object_store_dbs[namespace] = {}
      db = self._object_store_dbs[namespace]
      if start_empty:
        db.clear()
      return TestObjectStore(namespace, init=db)
    object_store_creator = ObjectStoreCreator(start_empty=start_empty,
                                              store_type=store_type_constructor)
    return CachingFileSystem(fs, object_store_creator)

  def testReadFiles(self):
    file_system = self._CreateCachingFileSystem(
        _CreateLocalFs(), start_empty=False)
    expected = {
      './test1.txt': 'test1\n',
      './test2.txt': 'test2\n',
      './test3.txt': 'test3\n',
    }
    self.assertEqual(
        expected,
        file_system.Read(['./test1.txt', './test2.txt', './test3.txt']).Get())

  def testListDir(self):
    file_system = self._CreateCachingFileSystem(
        _CreateLocalFs(), start_empty=False)
    expected = ['dir/'] + ['file%d.html' % i for i in range(7)]
    file_system._read_object_store.Set(
        'list/',
        (expected, file_system.Stat('list/').version))
    self.assertEqual(expected, sorted(file_system.ReadSingle('list/').Get()))

    expected.remove('file0.html')
    file_system._read_object_store.Set(
        'list/',
        (expected, file_system.Stat('list/').version))
    self.assertEqual(expected, sorted(file_system.ReadSingle('list/').Get()))

  def testCaching(self):
    test_fs = TestFileSystem({
      'bob': {
        'bob0': 'bob/bob0 contents',
        'bob1': 'bob/bob1 contents',
        'bob2': 'bob/bob2 contents',
        'bob3': 'bob/bob3 contents',
      }
    })
    mock_fs = MockFileSystem(test_fs)
    def create_empty_caching_fs():
      return self._CreateCachingFileSystem(mock_fs, start_empty=True)

    file_system = create_empty_caching_fs()

    # The stat/read should happen before resolving the Future, and resolving
    # the future shouldn't do any additional work.
    get_future = file_system.ReadSingle('bob/bob0')
    self.assertTrue(*mock_fs.CheckAndReset(read_count=1))
    self.assertEqual('bob/bob0 contents', get_future.Get())
    self.assertTrue(*mock_fs.CheckAndReset(read_resolve_count=1, stat_count=1))

    # Resource has been cached, so test resource is not re-fetched.
    self.assertEqual('bob/bob0 contents',
                     file_system.ReadSingle('bob/bob0').Get())
    self.assertTrue(*mock_fs.CheckAndReset())

    # Test if the Stat version is the same the resource is not re-fetched.
    file_system = create_empty_caching_fs()
    self.assertEqual('bob/bob0 contents',
                     file_system.ReadSingle('bob/bob0').Get())
    self.assertTrue(*mock_fs.CheckAndReset(stat_count=1))

    # Test if there is a newer version, the resource is re-fetched.
    file_system = create_empty_caching_fs()
    test_fs.IncrementStat();
    future = file_system.ReadSingle('bob/bob0')
    self.assertTrue(*mock_fs.CheckAndReset(read_count=1, stat_count=1))
    self.assertEqual('bob/bob0 contents', future.Get())
    self.assertTrue(*mock_fs.CheckAndReset(read_resolve_count=1))

    # Test directory and subdirectory stats are cached.
    file_system = create_empty_caching_fs()
    file_system._stat_object_store.Del('bob/bob0')
    file_system._read_object_store.Del('bob/bob0')
    file_system._stat_object_store.Del('bob/bob1')
    test_fs.IncrementStat();
    futures = (file_system.ReadSingle('bob/bob1'),
               file_system.ReadSingle('bob/bob0'))
    self.assertTrue(*mock_fs.CheckAndReset(read_count=2))
    self.assertEqual(('bob/bob1 contents', 'bob/bob0 contents'),
                     tuple(future.Get() for future in futures))
    self.assertTrue(*mock_fs.CheckAndReset(read_resolve_count=2, stat_count=1))
    self.assertEqual('bob/bob1 contents',
                     file_system.ReadSingle('bob/bob1').Get())
    self.assertTrue(*mock_fs.CheckAndReset())

    # Test a more recent parent directory doesn't force a refetch of children.
    file_system = create_empty_caching_fs()
    file_system._read_object_store.Del('bob/bob0')
    file_system._read_object_store.Del('bob/bob1')
    futures = (file_system.ReadSingle('bob/bob1'),
               file_system.ReadSingle('bob/bob2'),
               file_system.ReadSingle('bob/bob3'))
    self.assertTrue(*mock_fs.CheckAndReset(read_count=3))
    self.assertEqual(
        ('bob/bob1 contents', 'bob/bob2 contents', 'bob/bob3 contents'),
        tuple(future.Get() for future in futures))
    self.assertTrue(*mock_fs.CheckAndReset(read_resolve_count=3, stat_count=1))

    test_fs.IncrementStat(path='bob/bob0')
    file_system = create_empty_caching_fs()
    self.assertEqual('bob/bob1 contents',
                     file_system.ReadSingle('bob/bob1').Get())
    self.assertEqual('bob/bob2 contents',
                     file_system.ReadSingle('bob/bob2').Get())
    self.assertEqual('bob/bob3 contents',
                     file_system.ReadSingle('bob/bob3').Get())
    self.assertTrue(*mock_fs.CheckAndReset(stat_count=1))

    file_system = create_empty_caching_fs()
    file_system._stat_object_store.Del('bob/bob0')
    future = file_system.ReadSingle('bob/bob0')
    self.assertTrue(*mock_fs.CheckAndReset(read_count=1))
    self.assertEqual('bob/bob0 contents', future.Get())
    self.assertTrue(*mock_fs.CheckAndReset(read_resolve_count=1, stat_count=1))
    self.assertEqual('bob/bob0 contents',
                     file_system.ReadSingle('bob/bob0').Get())
    self.assertTrue(*mock_fs.CheckAndReset())

  def testCachedStat(self):
    test_fs = TestFileSystem({
      'bob': {
        'bob0': 'bob/bob0 contents',
        'bob1': 'bob/bob1 contents'
      }
    })
    mock_fs = MockFileSystem(test_fs)

    file_system = self._CreateCachingFileSystem(mock_fs, start_empty=False)

    self.assertEqual(StatInfo('0'), file_system.Stat('bob/bob0'))
    self.assertTrue(*mock_fs.CheckAndReset(stat_count=1))
    self.assertEqual(StatInfo('0'), file_system.Stat('bob/bob0'))
    self.assertTrue(*mock_fs.CheckAndReset())

    # Caching happens on a directory basis, so reading other files from that
    # directory won't result in a stat.
    self.assertEqual(StatInfo('0'), file_system.Stat('bob/bob1'))
    self.assertEqual(
        StatInfo('0', child_versions={'bob0': '0', 'bob1': '0'}),
        file_system.Stat('bob/'))
    self.assertTrue(*mock_fs.CheckAndReset())

    # Even though the stat is bumped, the object store still has it cached so
    # this won't update.
    test_fs.IncrementStat()
    self.assertEqual(StatInfo('0'), file_system.Stat('bob/bob0'))
    self.assertEqual(StatInfo('0'), file_system.Stat('bob/bob1'))
    self.assertEqual(
        StatInfo('0', child_versions={'bob0': '0', 'bob1': '0'}),
        file_system.Stat('bob/'))
    self.assertTrue(*mock_fs.CheckAndReset())

  def testFreshStat(self):
    test_fs = TestFileSystem({
      'bob': {
        'bob0': 'bob/bob0 contents',
        'bob1': 'bob/bob1 contents'
      }
    })
    mock_fs = MockFileSystem(test_fs)

    def run_expecting_stat(stat):
      def run():
        file_system = self._CreateCachingFileSystem(mock_fs, start_empty=True)
        self.assertEqual(
            StatInfo(stat, child_versions={'bob0': stat, 'bob1': stat}),
            file_system.Stat('bob/'))
        self.assertTrue(*mock_fs.CheckAndReset(stat_count=1))
        self.assertEqual(StatInfo(stat), file_system.Stat('bob/bob0'))
        self.assertEqual(StatInfo(stat), file_system.Stat('bob/bob0'))
        self.assertTrue(*mock_fs.CheckAndReset())
      run()
      run()

    run_expecting_stat('0')
    test_fs.IncrementStat()
    run_expecting_stat('1')

    def testSkipNotFound(self):
      caching_fs = self._CreateCachingFileSystem(TestFileSystem({
        'bob': {
          'bob0': 'bob/bob0 contents',
          'bob1': 'bob/bob1 contents'
        }
      }))
      def read_skip_not_found(paths):
        return caching_fs.Read(paths, skip_not_found=True).Get()
      self.assertEqual({}, read_skip_not_found(('grub',)))
      self.assertEqual({}, read_skip_not_found(('bob/bob2',)))
      self.assertEqual({
        'bob/bob0': 'bob/bob0 contents',
      }, read_skip_not_found(('bob/bob0', 'bob/bob2')))


if __name__ == '__main__':
  unittest.main()
