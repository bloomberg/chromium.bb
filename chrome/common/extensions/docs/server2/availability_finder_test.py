#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import sys
import unittest

import api_schema_graph
from availability_finder import AvailabilityFinder
from branch_utility import BranchUtility, ChannelInfo
from compiled_file_system import CompiledFileSystem
from fake_host_file_system_provider import FakeHostFileSystemProvider
from fake_url_fetcher import FakeUrlFetcher
from host_file_system_iterator import HostFileSystemIterator
from mock_function import MockFunction
from object_store_creator import ObjectStoreCreator
from test_data.canned_data import (CANNED_API_FILE_SYSTEM_DATA, CANNED_BRANCHES)
from test_data.object_level_availability.tabs import TABS_SCHEMA_BRANCHES
from test_util import Server2Path


TABS_UNMODIFIED_VERSIONS = (16, 20, 23, 24)

class AvailabilityFinderTest(unittest.TestCase):

  def setUp(self):
    self._branch_utility = BranchUtility(
        os.path.join('branch_utility', 'first.json'),
        os.path.join('branch_utility', 'second.json'),
        FakeUrlFetcher(Server2Path('test_data')),
        ObjectStoreCreator.ForTest())
    api_fs_creator = FakeHostFileSystemProvider(CANNED_API_FILE_SYSTEM_DATA)
    self._node_fs_creator = FakeHostFileSystemProvider(TABS_SCHEMA_BRANCHES)

    def create_availability_finder(host_fs_creator):
      test_object_store = ObjectStoreCreator.ForTest()
      return AvailabilityFinder(
          self._branch_utility,
          CompiledFileSystem.Factory(test_object_store),
          HostFileSystemIterator(host_fs_creator,
                                 self._branch_utility),
          host_fs_creator.GetTrunk(),
          test_object_store)

    self._avail_finder = create_availability_finder(api_fs_creator)
    self._node_avail_finder = create_availability_finder(self._node_fs_creator)

    # Imitate the actual SVN file system by incrementing the stats for paths
    # where an API schema has changed.
    last_stat = type('last_stat', (object,), {'val': 0})

    def stat_paths(file_system, channel_info):
      if channel_info.version not in TABS_UNMODIFIED_VERSIONS:
        last_stat.val += 1
      # HACK: |file_system| is a MockFileSystem backed by a TestFileSystem.
      # Increment the TestFileSystem stat count.
      file_system._file_system.IncrementStat(by=last_stat.val)
      # Continue looping. The iterator will stop after 'trunk' automatically.
      return True

    # Use the HostFileSystemIterator created above to change global stat values
    # for the TestFileSystems that it creates.
    self._node_avail_finder._file_system_iterator.Ascending(
        # The earliest version represented with the tabs' test data is 13.
        self._branch_utility.GetStableChannelInfo(13),
        stat_paths)

  def testGraphOptimization(self):
    # Keep track of how many times the APISchemaGraph constructor is called.
    original_constructor = api_schema_graph.APISchemaGraph
    mock_constructor = MockFunction(original_constructor)
    api_schema_graph.APISchemaGraph = mock_constructor

    try:
      # The test data includes an extra branch where the API does not exist.
      num_versions = len(TABS_SCHEMA_BRANCHES) - 1
      # We expect an APISchemaGraph to be created only when an API schema file
      # has different stat data from the previous version's schema file.
      num_graphs_created = num_versions - len(TABS_UNMODIFIED_VERSIONS)

      # Run the logic for object-level availability for an API.
      self._node_avail_finder.GetApiNodeAvailability('tabs')

      self.assertTrue(*api_schema_graph.APISchemaGraph.CheckAndReset(
          num_graphs_created))
    finally:
      # Ensure that the APISchemaGraph constructor is reset to be the original
      # constructor.
      api_schema_graph.APISchemaGraph = original_constructor

  def testGetApiAvailability(self):
    # Key: Using 'channel' (i.e. 'beta') to represent an availability listing
    # for an API in a _features.json file, and using |channel| (i.e. |dev|) to
    # represent the development channel, or phase of development, where an API's
    # availability is being checked.

    # Testing APIs with predetermined availability.
    self.assertEqual(
        ChannelInfo('trunk', 'trunk', 'trunk'),
        self._avail_finder.GetApiAvailability('jsonTrunkAPI'))
    self.assertEqual(
        ChannelInfo('dev', CANNED_BRANCHES[28], 28),
        self._avail_finder.GetApiAvailability('jsonDevAPI'))
    self.assertEqual(
        ChannelInfo('beta', CANNED_BRANCHES[27], 27),
        self._avail_finder.GetApiAvailability('jsonBetaAPI'))
    self.assertEqual(
        ChannelInfo('stable', CANNED_BRANCHES[20], 20),
        self._avail_finder.GetApiAvailability('jsonStableAPI'))

    # Testing a whitelisted API.
    self.assertEquals(
        ChannelInfo('beta', CANNED_BRANCHES[27], 27),
        self._avail_finder.GetApiAvailability('declarativeWebRequest'))

    # Testing APIs found only by checking file system existence.
    self.assertEquals(
        ChannelInfo('stable', CANNED_BRANCHES[23], 23),
        self._avail_finder.GetApiAvailability('windows'))
    self.assertEquals(
        ChannelInfo('stable', CANNED_BRANCHES[18], 18),
        self._avail_finder.GetApiAvailability('tabs'))
    self.assertEquals(
        ChannelInfo('stable', CANNED_BRANCHES[18], 18),
        self._avail_finder.GetApiAvailability('input.ime'))

    # Testing API channel existence for _api_features.json.
    # Listed as 'dev' on |beta|, 'dev' on |dev|.
    self.assertEquals(
        ChannelInfo('dev', CANNED_BRANCHES[28], 28),
        self._avail_finder.GetApiAvailability('systemInfo.stuff'))
    # Listed as 'stable' on |beta|.
    self.assertEquals(
        ChannelInfo('beta', CANNED_BRANCHES[27], 27),
        self._avail_finder.GetApiAvailability('systemInfo.cpu'))

    # Testing API channel existence for _manifest_features.json.
    # Listed as 'trunk' on all channels.
    self.assertEquals(
        ChannelInfo('trunk', 'trunk', 'trunk'),
        self._avail_finder.GetApiAvailability('sync'))
    # No records of API until |trunk|.
    self.assertEquals(
        ChannelInfo('trunk', 'trunk', 'trunk'),
        self._avail_finder.GetApiAvailability('history'))
    # Listed as 'dev' on |dev|.
    self.assertEquals(
        ChannelInfo('dev', CANNED_BRANCHES[28], 28),
        self._avail_finder.GetApiAvailability('storage'))
    # Stable in _manifest_features and into pre-18 versions.
    self.assertEquals(
        ChannelInfo('stable', CANNED_BRANCHES[8], 8),
        self._avail_finder.GetApiAvailability('pageAction'))

    # Testing API channel existence for _permission_features.json.
    # Listed as 'beta' on |trunk|.
    self.assertEquals(
        ChannelInfo('trunk', 'trunk', 'trunk'),
        self._avail_finder.GetApiAvailability('falseBetaAPI'))
    # Listed as 'trunk' on |trunk|.
    self.assertEquals(
        ChannelInfo('trunk', 'trunk', 'trunk'),
        self._avail_finder.GetApiAvailability('trunkAPI'))
    # Listed as 'trunk' on all development channels.
    self.assertEquals(
        ChannelInfo('trunk', 'trunk', 'trunk'),
        self._avail_finder.GetApiAvailability('declarativeContent'))
    # Listed as 'dev' on all development channels.
    self.assertEquals(
        ChannelInfo('dev', CANNED_BRANCHES[28], 28),
        self._avail_finder.GetApiAvailability('bluetooth'))
    # Listed as 'dev' on |dev|.
    self.assertEquals(
        ChannelInfo('dev', CANNED_BRANCHES[28], 28),
        self._avail_finder.GetApiAvailability('cookies'))
    # Treated as 'stable' APIs.
    self.assertEquals(
        ChannelInfo('stable', CANNED_BRANCHES[24], 24),
        self._avail_finder.GetApiAvailability('alarms'))
    self.assertEquals(
        ChannelInfo('stable', CANNED_BRANCHES[21], 21),
        self._avail_finder.GetApiAvailability('bookmarks'))

    # Testing older API existence using extension_api.json.
    self.assertEquals(
        ChannelInfo('stable', CANNED_BRANCHES[6], 6),
        self._avail_finder.GetApiAvailability('menus'))
    self.assertEquals(
        ChannelInfo('stable', CANNED_BRANCHES[5], 5),
        self._avail_finder.GetApiAvailability('idle'))

    # Switches between _features.json files across branches.
    # Listed as 'trunk' on all channels, in _api, _permission, or _manifest.
    self.assertEquals(
        ChannelInfo('trunk', 'trunk', 'trunk'),
        self._avail_finder.GetApiAvailability('contextMenus'))
    # Moves between _permission and _manifest as file system is traversed.
    self.assertEquals(
        ChannelInfo('stable', CANNED_BRANCHES[23], 23),
        self._avail_finder.GetApiAvailability('systemInfo.display'))
    self.assertEquals(
        ChannelInfo('stable', CANNED_BRANCHES[17], 17),
        self._avail_finder.GetApiAvailability('webRequest'))

    # Mid-upgrade cases:
    # Listed as 'dev' on |beta| and 'beta' on |dev|.
    self.assertEquals(
        ChannelInfo('dev', CANNED_BRANCHES[28], 28),
        self._avail_finder.GetApiAvailability('notifications'))
    # Listed as 'beta' on |stable|, 'dev' on |beta| ... until |stable| on trunk.
    self.assertEquals(
        ChannelInfo('trunk', 'trunk', 'trunk'),
        self._avail_finder.GetApiAvailability('events'))

  def testGetApiNodeAvailability(self):
    # Allow the LookupResult constructions below to take just one line.
    lookup_result = api_schema_graph.LookupResult
    availability_graph = self._node_avail_finder.GetApiNodeAvailability('tabs')

    self.assertEquals(
        lookup_result(True, self._branch_utility.GetChannelInfo('trunk')),
        availability_graph.Lookup('tabs', 'properties',
                                  'fakeTabsProperty3'))
    self.assertEquals(
        lookup_result(True, self._branch_utility.GetChannelInfo('dev')),
        availability_graph.Lookup('tabs', 'events', 'onActivated',
                                  'parameters', 'activeInfo', 'properties',
                                  'windowId'))
    self.assertEquals(
        lookup_result(True, self._branch_utility.GetChannelInfo('dev')),
        availability_graph.Lookup('tabs', 'events', 'onUpdated', 'parameters',
                                  'tab'))
    self.assertEquals(
        lookup_result(True, self._branch_utility.GetChannelInfo('beta')),
        availability_graph.Lookup('tabs', 'events','onActivated'))
    self.assertEquals(
        lookup_result(True, self._branch_utility.GetChannelInfo('beta')),
        availability_graph.Lookup('tabs', 'functions', 'get', 'parameters',
                                  'tabId'))
    self.assertEquals(
        lookup_result(True, self._branch_utility.GetChannelInfo('stable')),
        availability_graph.Lookup('tabs', 'types', 'InjectDetails',
                                  'properties', 'code'))
    self.assertEquals(
        lookup_result(True, self._branch_utility.GetChannelInfo('stable')),
        availability_graph.Lookup('tabs', 'types', 'InjectDetails',
                                  'properties', 'file'))
    self.assertEquals(
        lookup_result(True, self._branch_utility.GetStableChannelInfo(25)),
        availability_graph.Lookup('tabs', 'types', 'InjectDetails'))

    # Nothing new in version 24 or 23.

    self.assertEquals(
        lookup_result(True, self._branch_utility.GetStableChannelInfo(22)),
        availability_graph.Lookup('tabs', 'types', 'Tab', 'properties',
                                  'windowId'))
    self.assertEquals(
        lookup_result(True, self._branch_utility.GetStableChannelInfo(21)),
        availability_graph.Lookup('tabs', 'types', 'Tab', 'properties',
                                  'selected'))

    # Nothing new in version 20.

    self.assertEquals(
        lookup_result(True, self._branch_utility.GetStableChannelInfo(19)),
        availability_graph.Lookup('tabs', 'functions', 'getCurrent'))
    self.assertEquals(
        lookup_result(True, self._branch_utility.GetStableChannelInfo(18)),
        availability_graph.Lookup('tabs', 'types', 'Tab', 'properties',
                                  'index'))
    self.assertEquals(
        lookup_result(True, self._branch_utility.GetStableChannelInfo(17)),
        availability_graph.Lookup('tabs', 'events', 'onUpdated', 'parameters',
                                  'changeInfo'))

    # Nothing new in version 16.

    self.assertEquals(
        lookup_result(True, self._branch_utility.GetStableChannelInfo(15)),
        availability_graph.Lookup('tabs', 'properties',
                                  'fakeTabsProperty2'))

    # Everything else is available at the API's release, version 14 here.
    self.assertEquals(
        lookup_result(True, self._branch_utility.GetStableChannelInfo(14)),
        availability_graph.Lookup('tabs', 'types', 'Tab'))
    self.assertEquals(
        lookup_result(True, self._branch_utility.GetStableChannelInfo(14)),
        availability_graph.Lookup('tabs', 'types', 'Tab',
                                  'properties', 'url'))
    self.assertEquals(
        lookup_result(True, self._branch_utility.GetStableChannelInfo(14)),
        availability_graph.Lookup('tabs', 'properties',
                                  'fakeTabsProperty1'))
    self.assertEquals(
        lookup_result(True, self._branch_utility.GetStableChannelInfo(14)),
        availability_graph.Lookup('tabs', 'functions', 'get', 'parameters',
                                  'callback'))
    self.assertEquals(
        lookup_result(True, self._branch_utility.GetStableChannelInfo(14)),
        availability_graph.Lookup('tabs', 'events', 'onUpdated'))

    # Test things that aren't available.
    self.assertEqual(lookup_result(False, None),
                     availability_graph.Lookup('tabs', 'types',
                                               'UpdateInfo'))
    self.assertEqual(lookup_result(False, None),
                     availability_graph.Lookup('tabs', 'functions', 'get',
                                               'parameters', 'callback',
                                               'parameters', 'tab', 'id'))
    self.assertEqual(lookup_result(False, None),
                     availability_graph.Lookup('functions'))
    self.assertEqual(lookup_result(False, None),
                     availability_graph.Lookup('events', 'onActivated',
                                               'parameters', 'activeInfo',
                                               'tabId'))


if __name__ == '__main__':
  unittest.main()
