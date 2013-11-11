#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
from cStringIO import StringIO
import unittest
from zipfile import ZipFile

from caching_file_system import CachingFileSystem
from file_system import FileNotFoundError, StatInfo
from fake_url_fetcher import FakeURLFSFetcher
from local_file_system import LocalFileSystem
from new_github_file_system import GithubFileSystem
from object_store_creator import ObjectStoreCreator
from test_file_system import TestFileSystem

# A made up sha hash value.
FAKE_HASH = '7becb9f554dec76bd0fc12c1d32dbaff1d134a4d'


def _ZipFromFiles(file_dict):
  string = StringIO()
  zipfile = ZipFile(string, 'w')
  for filename, contents in file_dict.iteritems():
    zipfile.writestr(filename, contents)
  zipfile.close()

  return string.getvalue()


class TestGithubFileSystem(unittest.TestCase):
  def setUp(self):
    self._gfs = GithubFileSystem.ForTest('repo', FakeURLFSFetcher.CreateLocal())
    # Start and finish the repository load.
    self._cgfs = CachingFileSystem(self._gfs, ObjectStoreCreator.ForTest())

  def testReadDirectory(self):
    self._gfs.Refresh().Get()
    self.assertEqual(
        sorted(['requirements.txt', '.gitignore', 'README.md', 'src/']),
        sorted(self._gfs.ReadSingle('').Get()))
    self.assertEqual(
        sorted(['__init__.notpy', 'hello.notpy']),
        sorted(self._gfs.ReadSingle('src/').Get()))

  def testReadFile(self):
    self._gfs.Refresh().Get()
    expected = (
      '# Compiled Python files\n'
      '*.pyc\n'
    )
    self.assertEqual(expected, self._gfs.ReadSingle('.gitignore').Get())

  def testMultipleReads(self):
    self._gfs.Refresh().Get()
    self.assertEqual(
        self._gfs.ReadSingle('requirements.txt').Get(),
        self._gfs.ReadSingle('requirements.txt').Get())

  def testReads(self):
    self._gfs.Refresh().Get()
    expected = {
        'src/': sorted(['hello.notpy', '__init__.notpy']),
        '': sorted(['requirements.txt', '.gitignore', 'README.md', 'src/'])
    }

    read = self._gfs.Read(['', 'src/']).Get()
    self.assertEqual(expected['src/'], sorted(read['src/']))
    self.assertEqual(expected[''], sorted(read['']))

  def testStat(self):
    self._gfs.Refresh().Get()
    dir_stat = StatInfo(FAKE_HASH, {
      'hello.notpy': StatInfo(FAKE_HASH),
      '__init__.notpy': StatInfo(FAKE_HASH)
    })

    self.assertEqual(StatInfo(FAKE_HASH), self._gfs.Stat('README.md'))
    self.assertEqual(StatInfo(FAKE_HASH), self._gfs.Stat('src/hello.notpy'))
    self.assertEqual(dir_stat, self._gfs.Stat('src/'))

  def testBadReads(self):
    self._gfs.Refresh().Get()
    self.assertRaises(FileNotFoundError, self._gfs.Stat, 'DONT_README.md')
    self.assertRaises(FileNotFoundError,
                      self._gfs.ReadSingle('DONT_README.md').Get)

  def testCachingFileSystem(self):
    self._cgfs.Refresh().Get()
    initial_cgfs_read_one = self._cgfs.ReadSingle('src/hello.notpy').Get()

    self.assertEqual(initial_cgfs_read_one,
                     self._gfs.ReadSingle('src/hello.notpy').Get())
    self.assertEqual(initial_cgfs_read_one,
                     self._cgfs.ReadSingle('src/hello.notpy').Get())

    initial_cgfs_read_two = self._cgfs.Read(
        ['README.md', 'requirements.txt']).Get()

    self.assertEqual(
        initial_cgfs_read_two,
        self._gfs.Read(['README.md', 'requirements.txt']).Get())
    self.assertEqual(
        initial_cgfs_read_two,
        self._cgfs.Read(['README.md', 'requirements.txt']).Get())

  def testWithoutRefresh(self):
    # Without refreshing it will still read the content from blobstore, and it
    # does this via the magic of the FakeURLFSFetcher.
    self.assertEqual(['__init__.notpy', 'hello.notpy'],
                     sorted(self._gfs.ReadSingle('src/').Get()))

  def testRefresh(self):
    def make_sha_json(hash_value):
      from copy import deepcopy
      commit_json = json.loads(deepcopy(LocalFileSystem('').ReadSingle(
          'test_data/github_file_system/test_owner/repo/commits/HEAD').Get()))
      commit_json['commit']['tree']['sha'] = 'hash' + FAKE_HASH[4:]

      return json.dumps(commit_json)

    files = {
      'zipfile/': '',
      'zipfile/hello.txt': 'world',
      'zipfile/readme': 'test zip',
      'zipfile/dir/file1': 'contents',
      'zipfile/dir/file2': 'more contents'
    }

    string = _ZipFromFiles(files)

    test_files = {
      'test_owner': {
        'changing-repo': {
          'commits': {
            'HEAD': make_sha_json(FAKE_HASH)
          },
          'zipball': string
        }
      }
    }

    test_file_system = TestFileSystem(test_files)

    gfs = GithubFileSystem.ForTest(
        'changing-repo', FakeURLFSFetcher.Create(test_file_system), path='')

    gfs.Refresh().Get()
    initial_dir_read = sorted(gfs.ReadSingle('').Get())
    initial_file_read = gfs.ReadSingle('dir/file1').Get()

    # Change the underlying data.
    files['zipfile/hello.txt'] = 'hello world again'
    files['zipfile/new-file'] = 'a new file'
    files['zipfile/dir/file1'] = 'changed file'
    test_files['test_owner']['changing-repo']['zipball'] = _ZipFromFiles(files)
    test_files['test_owner']['changing-repo']['commits']['HEAD'] = (
        make_sha_json(FAKE_HASH + 'hash'))

    # Check that changes have not effected the file system yet.
    self.assertEqual(initial_dir_read, sorted(gfs.ReadSingle('').Get()))
    self.assertEqual(initial_file_read, gfs.ReadSingle('dir/file1').Get())

    gfs.Refresh().Get()

    # Check that the changes have effected the file system.
    self.assertTrue('new-file' in gfs.ReadSingle('').Get())
    self.assertEqual(files['zipfile/dir/file1'],
                     gfs.ReadSingle('dir/file1').Get())


if __name__ == '__main__':
  unittest.main()
