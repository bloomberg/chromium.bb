#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import sys
import unittest

from fake_url_fetcher import FakeUrlFetcher
from file_system import StatInfo
from future import Future
from subversion_file_system import SubversionFileSystem

class SubversionFileSystemTest(unittest.TestCase):
  def setUp(self):
    self._base_path = os.path.join(sys.path[0], 'test_data', 'file_system')

  def _CreateSubversionFileSystem(self):
    fetcher = FakeUrlFetcher(self._base_path)
    return SubversionFileSystem(fetcher, fetcher, self._base_path)

  def _ReadLocalFile(self, filename):
    with open(os.path.join(self._base_path, filename), 'r') as f:
      return f.read()

  def testReadFiles(self):
    expected = {
      'test1.txt': 'test1\n',
      'test2.txt': 'test2\n',
      'test3.txt': 'test3\n',
    }
    file_system = self._CreateSubversionFileSystem()
    self.assertEqual(
        expected,
        file_system.Read(['test1.txt', 'test2.txt', 'test3.txt']).Get())

  def testListDir(self):
    expected = ['dir/'] + ['file%d.html' % i for i in range(7)]
    file_system = self._CreateSubversionFileSystem()
    self.assertEqual(expected, sorted(file_system.ReadSingle('list/')))

  def testListSubDir(self):
    expected = ['empty.txt'] + ['file%d.html' % i for i in range(3)]
    file_system = self._CreateSubversionFileSystem()
    self.assertEqual(expected, sorted(file_system.ReadSingle('list/dir/')))

  def testDirStat(self):
    file_system = self._CreateSubversionFileSystem()
    stat_info = file_system.Stat('stat/')
    expected = StatInfo(
      '151113',
      child_versions=json.loads(self._ReadLocalFile('stat_result.json')))
    self.assertEqual(expected, stat_info)

  def testFileStat(self):
    file_system = self._CreateSubversionFileSystem()
    stat_info = file_system.Stat('stat/extension_api.h')
    self.assertEqual(StatInfo('146163'), stat_info)

  def testRevisions(self):
    # This is a super hacky test. Record the path that was fetched then exit the
    # test. Compare.
    class ValueErrorFetcher(object):
      def __init__(self):
        self.last_fetched = None

      def FetchAsync(self, path):
        self.last_fetched = path
        raise ValueError()

      def Fetch(self, path):
        self.last_fetched = path
        raise ValueError()

    file_fetcher = ValueErrorFetcher()
    stat_fetcher = ValueErrorFetcher()
    svn_path = 'svn:'

    svn_file_system = SubversionFileSystem(file_fetcher,
                                           stat_fetcher,
                                           svn_path,
                                           revision=42)

    self.assertRaises(ValueError, svn_file_system.ReadSingle, 'dir/file')
    self.assertEqual('dir/file?p=42', file_fetcher.last_fetched)
    # Stat() will always stat directories.
    self.assertRaises(ValueError, svn_file_system.Stat, 'dir/file')
    self.assertEqual('dir/?pathrev=42', stat_fetcher.last_fetched)

    self.assertRaises(ValueError, svn_file_system.ReadSingle, 'dir/')
    self.assertEqual('dir/?p=42', file_fetcher.last_fetched)
    self.assertRaises(ValueError, svn_file_system.Stat, 'dir/')
    self.assertEqual('dir/?pathrev=42', stat_fetcher.last_fetched)

if __name__ == '__main__':
  unittest.main()
