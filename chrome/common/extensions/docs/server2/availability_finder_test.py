#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

from availability_finder import AvailabilityFinder
from branch_utility import BranchUtility
from compiled_file_system import CompiledFileSystem
from fake_url_fetcher import FakeUrlFetcher
from object_store_creator import ObjectStoreCreator
from test_file_system import TestFileSystem
from test_data.canned_data import (CANNED_API_FILE_SYSTEM_DATA, CANNED_BRANCHES)

def _CreateCannedFileSystem(version):
  branch = CANNED_BRANCHES[version]
  return TestFileSystem(CANNED_API_FILE_SYSTEM_DATA[str(branch)])

class AvailabilityFinderTest(unittest.TestCase):
  def setUp(self):
    self._avail_ds_factory = AvailabilityFinder.Factory(
        ObjectStoreCreator.ForTest(),
        CompiledFileSystem.Factory(
            TestFileSystem(CANNED_API_FILE_SYSTEM_DATA['trunk']),
            ObjectStoreCreator.ForTest()),
        BranchUtility(
            os.path.join('branch_utility', 'first.json'),
            os.path.join('branch_utility', 'second.json'),
            FakeUrlFetcher(os.path.join(sys.path[0], 'test_data')),
            ObjectStoreCreator.ForTest()),
        _CreateCannedFileSystem)
    self._avail_ds = self._avail_ds_factory.Create()

  def testGetApiAvailability(self):
    # Key: Using 'channel' (i.e. 'beta') to represent an availability listing
    # for an API in a _features.json file, and using |channel| (i.e. |dev|) to
    # represent the development channel, or phase of development, where an API's
    # availability is being checked.

    # Testing the predetermined APIs found in
    # templates/json/api_availabilities.json.
    self.assertEqual('stable',
        self._avail_ds.GetApiAvailability('jsonAPI1').channel)
    self.assertEqual(10,
        self._avail_ds.GetApiAvailability('jsonAPI1').version)
    self.assertEqual('trunk',
        self._avail_ds.GetApiAvailability('jsonAPI2').channel)
    self.assertEqual(None,
        self._avail_ds.GetApiAvailability('jsonAPI2').version)
    self.assertEqual('dev',
        self._avail_ds.GetApiAvailability('jsonAPI3').channel)
    self.assertEqual(None,
        self._avail_ds.GetApiAvailability('jsonAPI3').version)

    # Testing APIs found only by checking file system existence.
    self.assertEquals('stable',
        self._avail_ds.GetApiAvailability('windows').channel)
    self.assertEquals(23,
        self._avail_ds.GetApiAvailability('windows').version)
    self.assertEquals('stable',
        self._avail_ds.GetApiAvailability('tabs').channel)
    self.assertEquals(18,
        self._avail_ds.GetApiAvailability('tabs').version)
    self.assertEquals('stable',
        self._avail_ds.GetApiAvailability('input.ime').channel)
    self.assertEquals(18,
        self._avail_ds.GetApiAvailability('input.ime').version)

    # Testing API channel existence for _api_features.json.
    # Listed as 'dev' on |beta|, 'dev' on |dev|.
    self.assertEquals('dev',
        self._avail_ds.GetApiAvailability('systemInfo.stuff').channel)
    self.assertEquals(28,
        self._avail_ds.GetApiAvailability('systemInfo.stuff').version)
    # Listed as 'stable' on |beta|.
    self.assertEquals('beta',
        self._avail_ds.GetApiAvailability('systemInfo.cpu').channel)
    self.assertEquals(27,
        self._avail_ds.GetApiAvailability('systemInfo.cpu').version)

    # Testing API channel existence for _manifest_features.json.
    # Listed as 'trunk' on all channels.
    self.assertEquals('trunk',
        self._avail_ds.GetApiAvailability('sync').channel)
    self.assertEquals('trunk',
        self._avail_ds.GetApiAvailability('sync').version)
    # No records of API until |trunk|.
    self.assertEquals('trunk',
        self._avail_ds.GetApiAvailability('history').channel)
    self.assertEquals('trunk',
        self._avail_ds.GetApiAvailability('history').version)
    # Listed as 'dev' on |dev|.
    self.assertEquals('dev',
        self._avail_ds.GetApiAvailability('storage').channel)
    self.assertEquals(28,
        self._avail_ds.GetApiAvailability('storage').version)
    # Stable in _manifest_features and into pre-18 versions.
    self.assertEquals('stable',
        self._avail_ds.GetApiAvailability('pageAction').channel)
    self.assertEquals(8,
        self._avail_ds.GetApiAvailability('pageAction').version)

    # Testing API channel existence for _permission_features.json.
    # Listed as 'beta' on |trunk|.
    self.assertEquals('trunk',
        self._avail_ds.GetApiAvailability('falseBetaAPI').version)
    self.assertEquals('trunk',
        self._avail_ds.GetApiAvailability('falseBetaAPI').version)
    # Listed as 'trunk' on |trunk|.
    self.assertEquals('trunk',
        self._avail_ds.GetApiAvailability('trunkAPI').channel)
    self.assertEquals('trunk',
        self._avail_ds.GetApiAvailability('trunkAPI').version)
    # Listed as 'trunk' on all development channels.
    self.assertEquals('trunk',
        self._avail_ds.GetApiAvailability('declarativeContent').channel)
    self.assertEquals('trunk',
        self._avail_ds.GetApiAvailability('declarativeContent').version)
    # Listed as 'dev' on all development channels.
    self.assertEquals('dev',
        self._avail_ds.GetApiAvailability('bluetooth').channel)
    self.assertEquals(28,
        self._avail_ds.GetApiAvailability('bluetooth').version)
    # Listed as 'dev' on |dev|.
    self.assertEquals('dev',
        self._avail_ds.GetApiAvailability('cookies').channel)
    self.assertEquals(28,
        self._avail_ds.GetApiAvailability('cookies').version)
    # Treated as 'stable' APIs.
    self.assertEquals('stable',
        self._avail_ds.GetApiAvailability('alarms').channel)
    self.assertEquals(24,
        self._avail_ds.GetApiAvailability('alarms').version)
    self.assertEquals('stable',
        self._avail_ds.GetApiAvailability('bookmarks').channel)
    self.assertEquals(21,
        self._avail_ds.GetApiAvailability('bookmarks').version)

    # Testing older API existence using extension_api.json.
    self.assertEquals('stable',
        self._avail_ds.GetApiAvailability('menus').channel)
    self.assertEquals(6,
        self._avail_ds.GetApiAvailability('menus').version)
    self.assertEquals('stable',
        self._avail_ds.GetApiAvailability('idle').channel)
    self.assertEquals(5,
        self._avail_ds.GetApiAvailability('idle').version)

    # Switches between _features.json files across branches.
    # Listed as 'trunk' on all channels, in _api, _permission, or _manifest.
    self.assertEquals('trunk',
        self._avail_ds.GetApiAvailability('contextMenus').channel)
    self.assertEquals('trunk',
        self._avail_ds.GetApiAvailability('contextMenus').version)
    # Moves between _permission and _manifest as file system is traversed.
    self.assertEquals('stable',
        self._avail_ds.GetApiAvailability('systemInfo.display').channel)
    self.assertEquals(23,
        self._avail_ds.GetApiAvailability('systemInfo.display').version)
    self.assertEquals('stable',
        self._avail_ds.GetApiAvailability('webRequest').channel)
    self.assertEquals(17,
        self._avail_ds.GetApiAvailability('webRequest').version)

    # Mid-upgrade cases:
    # Listed as 'dev' on |beta| and 'beta' on |dev|.
    self.assertEquals('dev',
        self._avail_ds.GetApiAvailability('notifications').channel)
    self.assertEquals(28,
        self._avail_ds.GetApiAvailability('notifications').version)
    # Listed as 'beta' on |stable|, 'dev' on |beta| ... until |stable| on trunk.
    self.assertEquals('trunk',
        self._avail_ds.GetApiAvailability('events').channel)
    self.assertEquals('trunk',
        self._avail_ds.GetApiAvailability('events').version)

if __name__ == '__main__':
  unittest.main()
