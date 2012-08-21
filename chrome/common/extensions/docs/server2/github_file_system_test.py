#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import unittest

from fake_url_fetcher import FakeUrlFetcher
from github_file_system import GithubFileSystem
from in_memory_object_store import InMemoryObjectStore

class FakeBlobstore(object):
  def Set(self, blob, key, version):
    return None

  def Get(self, key, version):
    return None

  def Delete(self, key, version):
    return None

class FakeGithubFetcher(FakeUrlFetcher):
  class _Response(object):
    def __init__(self, content):
      self.content = content

  def Fetch(self, path):
    if path == 'zipball':
      return super(FakeGithubFetcher, self).Fetch('file_system.zip')
    return self._Response('{ "commit": { "tree": { "sha": 0 } } }')

class GithubFileSystemTest(unittest.TestCase):
  def setUp(self):
    self._file_system = GithubFileSystem(FakeGithubFetcher('test_data'),
                                         InMemoryObjectStore('test'),
                                         FakeBlobstore())

  def testReadFiles(self):
    expected = {
      '/test1.txt': 'test1\n',
      '/test2.txt': 'test2\n',
      '/test3.txt': 'test3\n',
    }
    self.assertEqual(
        expected,
        self._file_system.Read(
            ['/test1.txt', '/test2.txt', '/test3.txt']).Get())

def testListDir(self):
  expected = ['dir/']
  for i in range(7):
    expected.append('file%d.html' % i)
  self.assertEqual(expected,
                   sorted(self._file_system.ReadSingle('/list/')))

if __name__ == '__main__':
  unittest.main()
