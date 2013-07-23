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

class FakeHostFileSystemCreator(object):
  def Create(self, branch):
    return TestFileSystem(CANNED_API_FILE_SYSTEM_DATA[str(branch)])

class AvailabilityFinderTest(unittest.TestCase):
  def setUp(self):
    self._avail_finder_factory = AvailabilityFinder.Factory(
        ObjectStoreCreator.ForTest(),
        CompiledFileSystem.Factory(
            TestFileSystem(CANNED_API_FILE_SYSTEM_DATA['trunk']),
            ObjectStoreCreator.ForTest()),
        BranchUtility(
            os.path.join('branch_utility', 'first.json'),
            os.path.join('branch_utility', 'second.json'),
            FakeUrlFetcher(os.path.join(sys.path[0], 'test_data')),
            ObjectStoreCreator.ForTest()),
        FakeHostFileSystemCreator())
    self._avail_finder = self._avail_finder_factory.Create()

  def testGetApiAvailability(self):
    # Key: Using 'channel' (i.e. 'beta') to represent an availability listing
    # for an API in a _features.json file, and using |channel| (i.e. |dev|) to
    # represent the development channel, or phase of development, where an API's
    # availability is being checked.

    # Testing the predetermined APIs found in
    # templates/json/api_availabilities.json.
    self.assertEqual('stable',
        self._avail_finder.GetApiAvailability('jsonAPI1').channel)
    self.assertEqual(10,
        self._avail_finder.GetApiAvailability('jsonAPI1').version)
    self.assertEqual('trunk',
        self._avail_finder.GetApiAvailability('jsonAPI2').channel)
    self.assertEqual('trunk',
        self._avail_finder.GetApiAvailability('jsonAPI2').version)
    self.assertEqual('dev',
        self._avail_finder.GetApiAvailability('jsonAPI3').channel)
    self.assertEqual(28,
        self._avail_finder.GetApiAvailability('jsonAPI3').version)

    # Testing whitelisted API
    self.assertEquals('beta',
        self._avail_finder.GetApiAvailability('declarativeWebRequest').channel)
    self.assertEquals(27,
        self._avail_finder.GetApiAvailability('declarativeWebRequest').version)

    # Testing APIs found only by checking file system existence.
    self.assertEquals('stable',
        self._avail_finder.GetApiAvailability('windows').channel)
    self.assertEquals(23,
        self._avail_finder.GetApiAvailability('windows').version)
    self.assertEquals('stable',
        self._avail_finder.GetApiAvailability('tabs').channel)
    self.assertEquals(18,
        self._avail_finder.GetApiAvailability('tabs').version)
    self.assertEquals('stable',
        self._avail_finder.GetApiAvailability('input.ime').channel)
    self.assertEquals(18,
        self._avail_finder.GetApiAvailability('input.ime').version)

    # Testing API channel existence for _api_features.json.
    # Listed as 'dev' on |beta|, 'dev' on |dev|.
    self.assertEquals('dev',
        self._avail_finder.GetApiAvailability('systemInfo.stuff').channel)
    self.assertEquals(28,
        self._avail_finder.GetApiAvailability('systemInfo.stuff').version)
    # Listed as 'stable' on |beta|.
    self.assertEquals('beta',
        self._avail_finder.GetApiAvailability('systemInfo.cpu').channel)
    self.assertEquals(27,
        self._avail_finder.GetApiAvailability('systemInfo.cpu').version)

    # Testing API channel existence for _manifest_features.json.
    # Listed as 'trunk' on all channels.
    self.assertEquals('trunk',
        self._avail_finder.GetApiAvailability('sync').channel)
    self.assertEquals('trunk',
        self._avail_finder.GetApiAvailability('sync').version)
    # No records of API until |trunk|.
    self.assertEquals('trunk',
        self._avail_finder.GetApiAvailability('history').channel)
    self.assertEquals('trunk',
        self._avail_finder.GetApiAvailability('history').version)
    # Listed as 'dev' on |dev|.
    self.assertEquals('dev',
        self._avail_finder.GetApiAvailability('storage').channel)
    self.assertEquals(28,
        self._avail_finder.GetApiAvailability('storage').version)
    # Stable in _manifest_features and into pre-18 versions.
    self.assertEquals('stable',
        self._avail_finder.GetApiAvailability('pageAction').channel)
    self.assertEquals(8,
        self._avail_finder.GetApiAvailability('pageAction').version)

    # Testing API channel existence for _permission_features.json.
    # Listed as 'beta' on |trunk|.
    self.assertEquals('trunk',
        self._avail_finder.GetApiAvailability('falseBetaAPI').version)
    self.assertEquals('trunk',
        self._avail_finder.GetApiAvailability('falseBetaAPI').version)
    # Listed as 'trunk' on |trunk|.
    self.assertEquals('trunk',
        self._avail_finder.GetApiAvailability('trunkAPI').channel)
    self.assertEquals('trunk',
        self._avail_finder.GetApiAvailability('trunkAPI').version)
    # Listed as 'trunk' on all development channels.
    self.assertEquals('trunk',
        self._avail_finder.GetApiAvailability('declarativeContent').channel)
    self.assertEquals('trunk',
        self._avail_finder.GetApiAvailability('declarativeContent').version)
    # Listed as 'dev' on all development channels.
    self.assertEquals('dev',
        self._avail_finder.GetApiAvailability('bluetooth').channel)
    self.assertEquals(28,
        self._avail_finder.GetApiAvailability('bluetooth').version)
    # Listed as 'dev' on |dev|.
    self.assertEquals('dev',
        self._avail_finder.GetApiAvailability('cookies').channel)
    self.assertEquals(28,
        self._avail_finder.GetApiAvailability('cookies').version)
    # Treated as 'stable' APIs.
    self.assertEquals('stable',
        self._avail_finder.GetApiAvailability('alarms').channel)
    self.assertEquals(24,
        self._avail_finder.GetApiAvailability('alarms').version)
    self.assertEquals('stable',
        self._avail_finder.GetApiAvailability('bookmarks').channel)
    self.assertEquals(21,
        self._avail_finder.GetApiAvailability('bookmarks').version)

    # Testing older API existence using extension_api.json.
    self.assertEquals('stable',
        self._avail_finder.GetApiAvailability('menus').channel)
    self.assertEquals(6,
        self._avail_finder.GetApiAvailability('menus').version)
    self.assertEquals('stable',
        self._avail_finder.GetApiAvailability('idle').channel)
    self.assertEquals(5,
        self._avail_finder.GetApiAvailability('idle').version)

    # Switches between _features.json files across branches.
    # Listed as 'trunk' on all channels, in _api, _permission, or _manifest.
    self.assertEquals('trunk',
        self._avail_finder.GetApiAvailability('contextMenus').channel)
    self.assertEquals('trunk',
        self._avail_finder.GetApiAvailability('contextMenus').version)
    # Moves between _permission and _manifest as file system is traversed.
    self.assertEquals('stable',
        self._avail_finder.GetApiAvailability('systemInfo.display').channel)
    self.assertEquals(23,
        self._avail_finder.GetApiAvailability('systemInfo.display').version)
    self.assertEquals('stable',
        self._avail_finder.GetApiAvailability('webRequest').channel)
    self.assertEquals(17,
        self._avail_finder.GetApiAvailability('webRequest').version)

    # Mid-upgrade cases:
    # Listed as 'dev' on |beta| and 'beta' on |dev|.
    self.assertEquals('dev',
        self._avail_finder.GetApiAvailability('notifications').channel)
    self.assertEquals(28,
        self._avail_finder.GetApiAvailability('notifications').version)
    # Listed as 'beta' on |stable|, 'dev' on |beta| ... until |stable| on trunk.
    self.assertEquals('trunk',
        self._avail_finder.GetApiAvailability('events').channel)
    self.assertEquals('trunk',
        self._avail_finder.GetApiAvailability('events').version)

if __name__ == '__main__':
  unittest.main()
