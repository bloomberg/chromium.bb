#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

from caching_file_system import CachingFileSystem
from compiled_file_system import CompiledFileSystem
from example_zipper import ExampleZipper
from local_file_system import LocalFileSystem
from object_store_creator import ObjectStoreCreator

class ExampleZipperTest(unittest.TestCase):
  def setUp(self):
    object_store_creator_factory = ObjectStoreCreator.TestFactory()
    self._file_system = CachingFileSystem(
        LocalFileSystem(os.path.join(sys.path[0], 'test_data')),
        object_store_creator_factory)
    self._example_zipper = ExampleZipper(
        CompiledFileSystem.Factory(self._file_system,
                                   object_store_creator_factory),
        'example_zipper')

  def testCreateZip(self):
    # Cache manifest.json as unicode and make sure ExampleZipper doesn't error.
    self._file_system.ReadSingle('example_zipper/basic/manifest.json')
    self.assertTrue(len(self._example_zipper.Create('basic')) > 0)

if __name__ == '__main__':
  unittest.main()
