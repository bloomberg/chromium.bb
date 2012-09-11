#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import sys
import unittest

from api_data_source import APIDataSource
from in_memory_object_store import InMemoryObjectStore
from file_system import FileNotFoundError
from compiled_file_system import CompiledFileSystem
from local_file_system import LocalFileSystem

class FakeSamplesDataSource:
  def Create(self, request):
    return {}

class APIDataSourceTest(unittest.TestCase):
  def setUp(self):
    self._base_path = os.path.join(sys.path[0], 'test_data', 'test_json')

  def _ReadLocalFile(self, filename):
    with open(os.path.join(self._base_path, filename), 'r') as f:
      return f.read()

  def DISABLED_testSimple(self):
    cache_factory = CompiledFileSystem.Factory(
        LocalFileSystem(self._base_path),
        InMemoryObjectStore('fake_branch'))
    data_source_factory = APIDataSource.Factory(cache_factory,
                                                '.',
                                                FakeSamplesDataSource())
    data_source = data_source_factory.Create({})

    # Take the dict out of the list.
    expected = json.loads(self._ReadLocalFile('expected_test_file.json'))
    expected['permissions'] = None
    test1 = data_source['test_file']
    test1.pop('samples')
    self.assertEqual(expected, test1)
    test2 = data_source['testFile']
    test2.pop('samples')
    self.assertEqual(expected, test2)
    test3 = data_source['testFile.html']
    test3.pop('samples')
    self.assertEqual(expected, test3)
    self.assertRaises(FileNotFoundError, data_source.get, 'junk')

if __name__ == '__main__':
  unittest.main()
