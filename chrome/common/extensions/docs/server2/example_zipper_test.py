#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

from compiled_file_system import CompiledFileSystem
from example_zipper import ExampleZipper
from in_memory_object_store import InMemoryObjectStore
from local_file_system import LocalFileSystem
from memcache_file_system import MemcacheFileSystem

class ExampleZipperTest(unittest.TestCase):
  def setUp(self):
    object_store = InMemoryObjectStore('')
    self._file_system = MemcacheFileSystem(
        LocalFileSystem(os.path.join(sys.path[0], 'test_data')),
        object_store)
    self._example_zipper = ExampleZipper(
        self._file_system,
        CompiledFileSystem.Factory(self._file_system, object_store),
        'example_zipper')

  def testCreateZip(self):
    # Cache manifest.json as unicode and make sure ExampleZipper doesn't error.
    self._file_system.ReadSingle('example_zipper/basic/manifest.json')
    self.assertTrue(len(self._example_zipper.Create('basic')) > 0)

if __name__ == '__main__':
  unittest.main()
