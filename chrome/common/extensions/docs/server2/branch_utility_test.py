#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from in_memory_object_store import InMemoryObjectStore
from branch_utility import BranchUtility
from fake_url_fetcher import FakeUrlFetcher
import unittest

class BranchUtilityTest(unittest.TestCase):
  def setUp(self):
    self._branch_util = BranchUtility(
        'branch_utility/first.json',
        { 'extensions': 'stable', 'apps': 'trunk' },
        FakeUrlFetcher('test_data'),
        InMemoryObjectStore(''))

  def testSplitChannelNameFromPath(self):
    self.assertEquals(('dev', 'extensions/stuff.html'),
                      self._branch_util.SplitChannelNameFromPath(
                      'dev/extensions/stuff.html'))
    self.assertEquals(('beta', 'extensions/stuff.html'),
                      self._branch_util.SplitChannelNameFromPath(
                      'beta/extensions/stuff.html'))
    self.assertEquals(('trunk', 'extensions/stuff.html'),
                      self._branch_util.SplitChannelNameFromPath(
                      'trunk/extensions/stuff.html'))
    self.assertEquals(('stable', 'extensions/stuff.html'),
                      self._branch_util.SplitChannelNameFromPath(
                      'extensions/stuff.html'))
    self.assertEquals(('trunk', 'apps/stuff.html'),
                      self._branch_util.SplitChannelNameFromPath(
                      'apps/stuff.html'))
    self.assertEquals(('stable', 'extensions/dev/stuff.html'),
                      self._branch_util.SplitChannelNameFromPath(
                      'extensions/dev/stuff.html'))

  def testGetBranchNumberForChannelName(self):
    self.assertEquals('1132',
                      self._branch_util.GetBranchNumberForChannelName('dev'))
    self.assertEquals('1084',
                      self._branch_util.GetBranchNumberForChannelName('beta'))
    self.assertEquals('1234',
                      self._branch_util.GetBranchNumberForChannelName('stable'))
    self.assertEquals('trunk',
                      self._branch_util.GetBranchNumberForChannelName('trunk'))

if __name__ == '__main__':
  unittest.main()
