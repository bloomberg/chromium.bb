#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import unittest

from extensions_paths import SERVER2
from fake_url_fetcher import FakeUrlFetcher
from file_system import FileSystemError, StatInfo
from subversion_file_system import SubversionFileSystem
from test_util import ReadFile, Server2Path


_SHARED_FILE_SYSTEM_TEST_DATA = Server2Path('test_data', 'file_system')
_SUBVERSION_FILE_SYSTEM_TEST_DATA = Server2Path(
    'test_data', 'subversion_file_system')


def _CreateSubversionFileSystem(path):
  fetcher = FakeUrlFetcher(path)
  return SubversionFileSystem(fetcher, fetcher, path), fetcher


class SubversionFileSystemTest(unittest.TestCase):
  def testReadFiles(self):
    expected = {
      'test1.txt': 'test1\n',
      'test2.txt': 'test2\n',
      'test3.txt': 'test3\n',
    }
    file_system, fetcher = _CreateSubversionFileSystem(
        _SHARED_FILE_SYSTEM_TEST_DATA)
    read_future = file_system.Read(['test1.txt', 'test2.txt', 'test3.txt'])
    self.assertTrue(*fetcher.CheckAndReset(async_count=3))
    self.assertEqual(expected, read_future.Get())
    self.assertTrue(*fetcher.CheckAndReset(async_resolve_count=3))

  def testListDir(self):
    expected = ['dir/'] + ['file%d.html' % i for i in range(7)]
    file_system, fetcher = _CreateSubversionFileSystem(
        _SHARED_FILE_SYSTEM_TEST_DATA)
    list_future = file_system.ReadSingle('list/')
    self.assertTrue(*fetcher.CheckAndReset(async_count=1))
    self.assertEqual(expected, sorted(list_future.Get()))
    self.assertTrue(*fetcher.CheckAndReset(async_resolve_count=1))

  def testListSubDir(self):
    expected = ['empty.txt'] + ['file%d.html' % i for i in range(3)]
    file_system, fetcher = _CreateSubversionFileSystem(
        _SHARED_FILE_SYSTEM_TEST_DATA)
    list_future = file_system.ReadSingle('list/dir/')
    self.assertTrue(*fetcher.CheckAndReset(async_count=1))
    self.assertEqual(expected, sorted(list_future.Get()))
    self.assertTrue(*fetcher.CheckAndReset(async_resolve_count=1))

  def testDirStat(self):
    file_system, fetcher = _CreateSubversionFileSystem(
        _SHARED_FILE_SYSTEM_TEST_DATA)
    stat_info = file_system.Stat('stat/')
    self.assertTrue(*fetcher.CheckAndReset(async_count=1,
                                           async_resolve_count=1))
    expected = StatInfo(
      '151113',
      child_versions=json.loads(ReadFile(
          SERVER2, 'test_data', 'file_system', 'stat_result.json')))
    self.assertEqual(expected, stat_info)

  def testFileStat(self):
    file_system, fetcher = _CreateSubversionFileSystem(
        _SHARED_FILE_SYSTEM_TEST_DATA)
    stat_info = file_system.Stat('stat/extension_api.h')
    self.assertTrue(*fetcher.CheckAndReset(async_count=1,
                                           async_resolve_count=1))
    self.assertEqual(StatInfo('146163'), stat_info)

  def testRevisions(self):
    # This is a super hacky test. Record the path that was fetched then exit the
    # test. Compare.
    class ValueErrorFetcher(object):
      def __init__(self):
        self.last_fetched = None

      def FetchAsync(self, path):
        class ThrowsValueError(object):
          def Get(self): raise ValueError()
        self.last_fetched = path
        return ThrowsValueError()

      def Fetch(self, path, **kwargs):
        self.last_fetched = path
        raise ValueError()

    file_fetcher = ValueErrorFetcher()
    stat_fetcher = ValueErrorFetcher()
    svn_path = 'svn:'

    svn_file_system = SubversionFileSystem(file_fetcher,
                                           stat_fetcher,
                                           svn_path,
                                           revision=42)

    self.assertRaises(FileSystemError,
                      svn_file_system.ReadSingle('dir/file').Get)
    self.assertEqual('dir/file?p=42', file_fetcher.last_fetched)
    # Stat() will always stat directories.
    self.assertRaises(FileSystemError, svn_file_system.Stat, 'dir/file')
    self.assertEqual('dir?pathrev=42', stat_fetcher.last_fetched)

    self.assertRaises(FileSystemError,
                      svn_file_system.ReadSingle('dir/').Get)
    self.assertEqual('dir/?p=42', file_fetcher.last_fetched)
    self.assertRaises(FileSystemError, svn_file_system.Stat, 'dir/')
    self.assertEqual('dir?pathrev=42', stat_fetcher.last_fetched)

  def testDirectoryVersionOnDeletion(self):
    '''Tests the case when the most recent operation on a directory is the
    deletion of a file. Here it is not enough to take the maximum version of all
    files in the directory, as we used to, for obvious reasons.
    '''
    file_system, _ = _CreateSubversionFileSystem(
        _SUBVERSION_FILE_SYSTEM_TEST_DATA)
    dir_stat = file_system.Stat('docs_public_extensions_214898/')
    self.assertEqual('214692', dir_stat.version)

  def testEmptyDirectory(self):
    file_system, _ = _CreateSubversionFileSystem(
        _SUBVERSION_FILE_SYSTEM_TEST_DATA)
    dir_stat = file_system.Stat('api_icons_214898/')
    self.assertEqual('193838', dir_stat.version)
    self.assertEqual({}, dir_stat.child_versions)

    def testSkipNotFound(self):
      file_system, _ = _CreateSubversionFileSystem(
          _SUBVERSION_FILE_SYSTEM_TEST_DATA)
      self.assertEqual({}, file_system.Read(('fakefile',),
                                            skip_not_found=True).Get())


if __name__ == '__main__':
  unittest.main()
