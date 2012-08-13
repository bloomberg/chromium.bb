#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import unittest

import appengine_memcache as memcache
from appengine_memcache import AppEngineMemcache
from local_file_system import LocalFileSystem
from memcache_file_system import MemcacheFileSystem

class LocalFileSystemTest(unittest.TestCase):
  def setUp(self):
    self._memcache = AppEngineMemcache('')
    self._file_system = MemcacheFileSystem(
        LocalFileSystem(os.path.join('test_data', 'file_system')),
        self._memcache)

  def testReadFiles(self):
    expected = {
      'test1.txt': 'test1\n',
      'test2.txt': 'test2\n',
      'test3.txt': 'test3\n',
    }
    self.assertEqual(
        expected,
        self._file_system.Read(['test1.txt', 'test2.txt', 'test3.txt']).Get())

  def testListDir(self):
    expected = ['dir/']
    for i in range(7):
      expected.append('file%d.html' % i)
    self.assertEqual(expected,
                     sorted(self._file_system.ReadSingle('list/')))
    expected.remove('file0.html')
    self._memcache.Set('list/',
                       (expected, self._file_system.Stat('list/').version),
                       memcache.MEMCACHE_FILE_SYSTEM_READ)
    self.assertEqual(expected,
                     sorted(self._file_system.ReadSingle('list/')))

if __name__ == '__main__':
  unittest.main()
