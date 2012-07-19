#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import unittest

from file_system_cache import FileSystemCache
from local_file_system import LocalFileSystem
from api_data_source import APIDataSource

class APIDataSourceTest(unittest.TestCase):
  def setUp(self):
    self._base_path = os.path.join('test_data', 'test_json')

  def _ReadLocalFile(self, filename):
    with open(os.path.join(self._base_path, filename), 'r') as f:
      return f.read()

  def testSimple(self):
    cache_builder = FileSystemCache.Builder(LocalFileSystem(self._base_path))
    data_source = APIDataSource(cache_builder, './')

    # Take the dict out of the list.
    expected = json.loads(self._ReadLocalFile('expected_test_file.json'))
    self.assertEqual(expected, data_source['test_file'])
    self.assertEqual(expected, data_source['testFile'])
    self.assertEqual(expected, data_source['testFile.html'])

    self.assertEqual(None, data_source['junk'])

if __name__ == '__main__':
  unittest.main()
