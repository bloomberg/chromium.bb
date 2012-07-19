#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from branch_utility import BranchUtility
from fake_url_fetcher import FakeUrlFetcher
from in_memory_memcache import InMemoryMemcache
import unittest

class BranchUtilityTest(unittest.TestCase):
  def setUp(self):
    self._branch_util = BranchUtility('branch_utility/first.json',
                                      'stable',
                                      FakeUrlFetcher('test_data'),
                                      InMemoryMemcache())

  def testSplitChannelNameFromPath(self):
    self.assertEquals(('dev', 'hello/stuff.html'),
                      self._branch_util.SplitChannelNameFromPath(
                      'dev/hello/stuff.html'))
    self.assertEquals(('beta', 'hello/stuff.html'),
                      self._branch_util.SplitChannelNameFromPath(
                      'beta/hello/stuff.html'))
    self.assertEquals(('trunk', 'hello/stuff.html'),
                      self._branch_util.SplitChannelNameFromPath(
                      'trunk/hello/stuff.html'))
    self.assertEquals(('stable', 'hello/stuff.html'),
                      self._branch_util.SplitChannelNameFromPath(
                      'hello/stuff.html'))
    self.assertEquals(('stable', 'hello/dev/stuff.html'),
                      self._branch_util.SplitChannelNameFromPath(
                      'hello/dev/stuff.html'))

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
