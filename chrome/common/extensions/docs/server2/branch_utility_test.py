#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

from branch_utility import BranchUtility
from fake_url_fetcher import FakeUrlFetcher
from object_store_creator import ObjectStoreCreator

class BranchUtilityTest(unittest.TestCase):
  def setUp(self):
    self._branch_util = BranchUtility(
        os.path.join('branch_utility', 'first.json'),
        os.path.join('branch_utility', 'second.json'),
        FakeUrlFetcher(os.path.join(sys.path[0], 'test_data')),
        ObjectStoreCreator.ForTest())

  def testSplitChannelNameFromPath(self):
    self.assertEquals(('stable', 'extensions/stuff.html'),
                      self._branch_util.SplitChannelNameFromPath(
                      'stable/extensions/stuff.html'))
    self.assertEquals(('dev', 'extensions/stuff.html'),
                      self._branch_util.SplitChannelNameFromPath(
                      'dev/extensions/stuff.html'))
    self.assertEquals(('beta', 'extensions/stuff.html'),
                      self._branch_util.SplitChannelNameFromPath(
                      'beta/extensions/stuff.html'))
    self.assertEquals(('trunk', 'extensions/stuff.html'),
                      self._branch_util.SplitChannelNameFromPath(
                      'trunk/extensions/stuff.html'))
    self.assertEquals((None, 'extensions/stuff.html'),
                      self._branch_util.SplitChannelNameFromPath(
                      'extensions/stuff.html'))
    self.assertEquals((None, 'apps/stuff.html'),
                      self._branch_util.SplitChannelNameFromPath(
                      'apps/stuff.html'))
    self.assertEquals((None, 'extensions/dev/stuff.html'),
                      self._branch_util.SplitChannelNameFromPath(
                      'extensions/dev/stuff.html'))
    self.assertEquals((None, 'stuff.html'),
                      self._branch_util.SplitChannelNameFromPath(
                      'stuff.html'))

  def testNewestChannel(self):
    self.assertEquals('trunk',
        self._branch_util.NewestChannel(('trunk', 'dev', 'beta', 'stable')))
    self.assertEquals('trunk',
        self._branch_util.NewestChannel(('stable', 'beta', 'dev', 'trunk')))
    self.assertEquals('dev',
        self._branch_util.NewestChannel(('stable', 'beta', 'dev')))
    self.assertEquals('dev',
        self._branch_util.NewestChannel(('dev', 'beta', 'stable')))
    self.assertEquals('beta',
        self._branch_util.NewestChannel(('beta', 'stable')))
    self.assertEquals('beta',
        self._branch_util.NewestChannel(('stable', 'beta')))
    self.assertEquals('stable', self._branch_util.NewestChannel(('stable',)))
    self.assertEquals('beta', self._branch_util.NewestChannel(('beta',)))
    self.assertEquals('dev', self._branch_util.NewestChannel(('dev',)))
    self.assertEquals('trunk', self._branch_util.NewestChannel(('trunk',)))

  def testGetChannelInfo(self):
    self.assertEquals('trunk',
      self._branch_util.GetChannelInfo('trunk').channel)
    self.assertEquals('trunk',
      self._branch_util.GetChannelInfo('trunk').branch)
    self.assertEquals('trunk',
      self._branch_util.GetChannelInfo('trunk').version)
    self.assertEquals('dev',
      self._branch_util.GetChannelInfo('dev').channel)
    self.assertEquals(1500,
      self._branch_util.GetChannelInfo('dev').branch)
    self.assertEquals(28,
      self._branch_util.GetChannelInfo('dev').version)
    self.assertEquals('beta',
      self._branch_util.GetChannelInfo('beta').channel)
    self.assertEquals(1453,
      self._branch_util.GetChannelInfo('beta').branch)
    self.assertEquals(27,
      self._branch_util.GetChannelInfo('beta').version)
    self.assertEquals('stable',
      self._branch_util.GetChannelInfo('stable').channel)
    self.assertEquals(1410,
      self._branch_util.GetChannelInfo('stable').branch)
    self.assertEquals(26,
      self._branch_util.GetChannelInfo('stable').version)

  def testGetLatestVersionNumber(self):
    self.assertEquals(28, self._branch_util.GetLatestVersionNumber())

  def testGetBranchForVersion(self):
    self.assertEquals(1500,
        self._branch_util.GetBranchForVersion(28))
    self.assertEquals(1453,
        self._branch_util.GetBranchForVersion(27))
    self.assertEquals(1410,
        self._branch_util.GetBranchForVersion(26))
    self.assertEquals(1364,
        self._branch_util.GetBranchForVersion(25))
    self.assertEquals(1312,
        self._branch_util.GetBranchForVersion(24))
    self.assertEquals(1271,
        self._branch_util.GetBranchForVersion(23))
    self.assertEquals(1229,
        self._branch_util.GetBranchForVersion(22))
    self.assertEquals(1180,
        self._branch_util.GetBranchForVersion(21))
    self.assertEquals(1132,
        self._branch_util.GetBranchForVersion(20))
    self.assertEquals(1084,
        self._branch_util.GetBranchForVersion(19))
    self.assertEquals(1025,
        self._branch_util.GetBranchForVersion(18))
    self.assertEquals(963,
        self._branch_util.GetBranchForVersion(17))
    self.assertEquals(696,
        self._branch_util.GetBranchForVersion(11))
    self.assertEquals(396,
        self._branch_util.GetBranchForVersion(5))

  def testGetChannelForVersion(self):
    self.assertEquals('trunk',
        self._branch_util.GetChannelForVersion('trunk'))
    self.assertEquals('dev',
        self._branch_util.GetChannelForVersion(28))
    self.assertEquals('beta',
        self._branch_util.GetChannelForVersion(27))
    self.assertEquals('stable',
        self._branch_util.GetChannelForVersion(26))
    self.assertEquals('stable',
        self._branch_util.GetChannelForVersion(22))
    self.assertEquals('stable',
        self._branch_util.GetChannelForVersion(18))
    self.assertEquals('stable',
        self._branch_util.GetChannelForVersion(14))
    self.assertEquals(None,
        self._branch_util.GetChannelForVersion(30))
    self.assertEquals(None,
        self._branch_util.GetChannelForVersion(42))


if __name__ == '__main__':
  unittest.main()
