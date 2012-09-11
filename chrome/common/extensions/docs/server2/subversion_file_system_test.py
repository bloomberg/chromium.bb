#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import sys
import unittest

from fake_url_fetcher import FakeUrlFetcher
from subversion_file_system import SubversionFileSystem

class SubversionFileSystemTest(unittest.TestCase):
  def setUp(self):
    self._base_path = os.path.join(sys.path[0], 'test_data', 'file_system')
    fetcher = FakeUrlFetcher(self._base_path)
    self._file_system = SubversionFileSystem(fetcher, fetcher)

  def _ReadLocalFile(self, filename):
    with open(os.path.join(self._base_path, filename), 'r') as f:
      return f.read()

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

  def testStat(self):
    stat_info = self._file_system.Stat('stat/')
    self.assertEquals('151113', stat_info.version)
    self.assertEquals(json.loads(self._ReadLocalFile('stat_result.json')),
                      stat_info.child_versions)

if __name__ == '__main__':
  unittest.main()
