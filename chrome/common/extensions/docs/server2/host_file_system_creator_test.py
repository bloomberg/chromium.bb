#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from caching_file_system import CachingFileSystem
from file_system import FileNotFoundError
from host_file_system_creator import HostFileSystemCreator
from object_store_creator import ObjectStoreCreator
from offline_file_system import OfflineFileSystem
from test_data.canned_data import CANNED_API_FILE_SYSTEM_DATA
from test_file_system import TestFileSystem

def ConstructorForTest(branch, revision=None):
  return TestFileSystem(CANNED_API_FILE_SYSTEM_DATA[str(branch)])

class HostFileSystemCreatorTest(unittest.TestCase):
  def setUp(self):
    self._idle_path = 'api/idle.json'

  def testWithCaching(self):
    creator = HostFileSystemCreator(ObjectStoreCreator.ForTest(),
                                    constructor_for_test=ConstructorForTest)

    fs = creator.Create('trunk')
    firstRead = fs.ReadSingle(self._idle_path)
    CANNED_API_FILE_SYSTEM_DATA['trunk']['api']['idle.json'] = 'blah blah blah'
    secondRead = fs.ReadSingle(self._idle_path)

    self.assertEqual(firstRead, secondRead)

  def testWithOffline(self):
    creator = HostFileSystemCreator(ObjectStoreCreator.ForTest(),
                                    offline=True,
                                    constructor_for_test=ConstructorForTest)

    fs = creator.Create('trunk')
    # Offline file system should raise a FileNotFoundError if read is attempted.
    self.assertRaises(FileNotFoundError, fs.ReadSingle, self._idle_path)

if __name__ == '__main__':
  unittest.main()
