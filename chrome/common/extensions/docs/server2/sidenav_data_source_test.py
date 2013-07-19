#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import sys
import unittest

from compiled_file_system import CompiledFileSystem
from local_file_system import LocalFileSystem
from object_store_creator import ObjectStoreCreator
from sidenav_data_source import SidenavDataSource

class SamplesDataSourceTest(unittest.TestCase):
  def setUp(self):
    self._json_path = 'docs/server2/test_data/sidenav_data_source'
    self._compiled_fs_factory = CompiledFileSystem.Factory(
        LocalFileSystem.Create(),
        ObjectStoreCreator.ForTest())

  def _CheckLevels(self, items, level=2):
    for item in items:
      self.assertEqual(level, item['level'])
      if 'items' in item:
        self._CheckLevels(item['items'], level=level + 1)

  def _ReadLocalFile(self, filename):
    with open(os.path.join(sys.path[0],
                           'test_data',
                           'sidenav_data_source',
                           filename), 'r') as f:
      return f.read()

  def testLevels(self):
    sidenav_data_source = SidenavDataSource.Factory(self._compiled_fs_factory,
                                                    self._json_path).Create('')
    sidenav_json = sidenav_data_source.get('test')
    self._CheckLevels(sidenav_json)

  def testSelected(self):
    sidenav_data_source = SidenavDataSource.Factory(
        self._compiled_fs_factory,
        self._json_path).Create('www.b.com')
    sidenav_json = sidenav_data_source.get('test')
    # This will be prettier once JSON is loaded with an OrderedDict.
    for item in sidenav_json:
      if item['title'] == 'Jim':
        self.assertTrue(item.get('child_selected', False))
        for next_item in item['items']:
          if next_item['title'] == 'B':
            self.assertTrue(next_item.get('selected', False))
            return
    # If we didn't return already, we should fail.
    self.fail()

  def testAbsolutePath(self):
    sidenav_data_source = SidenavDataSource.Factory(
        self._compiled_fs_factory,
        self._json_path).Create('absolute_path/test.html')
    sidenav_json = sidenav_data_source.get('absolute_path')
    self.assertEqual(
        sidenav_json,
        json.loads(self._ReadLocalFile('absolute_path_sidenav_expected.json')))

if __name__ == '__main__':
  unittest.main()
