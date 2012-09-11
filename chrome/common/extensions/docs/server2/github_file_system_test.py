#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import sys
import unittest

from appengine_blobstore import AppEngineBlobstore
from appengine_url_fetcher import AppEngineUrlFetcher
from appengine_wrappers import files
from fake_fetchers import ConfigureFakeFetchers
from github_file_system import GithubFileSystem
from in_memory_object_store import InMemoryObjectStore
import url_constants

class GithubFileSystemTest(unittest.TestCase):
  def setUp(self):
    ConfigureFakeFetchers(sys.path[0])
    self._base_path = os.path.join(sys.path[0],
                                   'test_data',
                                   'github_file_system')
    self._file_system = GithubFileSystem(
        AppEngineUrlFetcher(url_constants.GITHUB_URL),
        InMemoryObjectStore('github'),
        AppEngineBlobstore())

  def _ReadLocalFile(self, filename):
    with open(os.path.join(self._base_path, filename), 'r') as f:
      return f.read()

  def testList(self):
    self.assertEqual(json.loads(self._ReadLocalFile('expected_list.json')),
                     self._file_system.Read(['/']).Get())

  def testRead(self):
   self.assertEqual(self._ReadLocalFile('expected_read.txt'),
                    self._file_system.ReadSingle('/analytics/launch.js'))

  def testStat(self):
    self.assertEqual(0, self._file_system.Stat('zipball').version)

  def testKeyGeneration(self):
    self.assertEqual(0, len(files.GetBlobKeys()))
    self._file_system.ReadSingle('/analytics/launch.js')
    self.assertEqual(1, len(files.GetBlobKeys()))
    self._file_system.ReadSingle('/analytics/main.css')
    self.assertEqual(1, len(files.GetBlobKeys()))

if __name__ == '__main__':
  unittest.main()
