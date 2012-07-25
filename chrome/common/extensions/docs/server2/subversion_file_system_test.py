#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import unittest

from fake_url_fetcher import FakeUrlFetcher
from subversion_file_system import SubversionFileSystem

class SubversionFileSystemTest(unittest.TestCase):
  def setUp(self):
    fetcher = FakeUrlFetcher(os.path.join('test_data', 'file_system'))
    self._file_system = SubversionFileSystem(fetcher)

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
    # Value is hard-coded into FakeUrlFetcher.
    self.assertEqual(0, self._file_system.Stat('list/dir/').version)

if __name__ == '__main__':
  unittest.main()
