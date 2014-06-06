#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

from branch_utility import BranchUtility, ChannelInfo
from fake_url_fetcher import FakeUrlFetcher
from object_store_creator import ObjectStoreCreator
from test_util import Server2Path


class BranchUtilityTest(unittest.TestCase):

  def setUp(self):
    self._branch_util = BranchUtility(
        os.path.join('branch_utility', 'first.json'),
        os.path.join('branch_utility', 'second.json'),
        FakeUrlFetcher(Server2Path('test_data')),
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

  def testNewer(self):
    oldest_stable_info = ChannelInfo('stable', '963', 17)
    older_stable_info = ChannelInfo('stable', '1025', 18)
    old_stable_info = ChannelInfo('stable', '1084', 19)
    sort_of_old_stable_info = ChannelInfo('stable', '1364', 25)
    stable_info = ChannelInfo('stable', '1410', 26)
    beta_info = ChannelInfo('beta', '1453', 27)
    dev_info = ChannelInfo('dev', '1500', 28)
    trunk_info = ChannelInfo('trunk', 'trunk', 'trunk')

    self.assertEquals(older_stable_info,
                      self._branch_util.Newer(oldest_stable_info))
    self.assertEquals(old_stable_info,
                      self._branch_util.Newer(older_stable_info))
    self.assertEquals(stable_info,
                      self._branch_util.Newer(sort_of_old_stable_info))
    self.assertEquals(beta_info, self._branch_util.Newer(stable_info))
    self.assertEquals(dev_info, self._branch_util.Newer(beta_info))
    self.assertEquals(trunk_info, self._branch_util.Newer(dev_info))
    # Test the upper limit.
    self.assertEquals(None, self._branch_util.Newer(trunk_info))


  def testOlder(self):
    trunk_info = ChannelInfo('trunk', 'trunk', 'trunk')
    dev_info = ChannelInfo('dev', '1500', 28)
    beta_info = ChannelInfo('beta', '1453', 27)
    stable_info = ChannelInfo('stable', '1410', 26)
    old_stable_info = ChannelInfo('stable', '1364', 25)
    older_stable_info = ChannelInfo('stable', '1312', 24)
    oldest_stable_info = ChannelInfo('stable', '396', 5)

    self.assertEquals(dev_info, self._branch_util.Older(trunk_info))
    self.assertEquals(beta_info, self._branch_util.Older(dev_info))
    self.assertEquals(stable_info, self._branch_util.Older(beta_info))
    self.assertEquals(old_stable_info, self._branch_util.Older(stable_info))
    self.assertEquals(older_stable_info,
                      self._branch_util.Older(old_stable_info))
    # Test the lower limit.
    self.assertEquals(None, self._branch_util.Older(oldest_stable_info))

  def testGetChannelInfo(self):
    trunk_info = ChannelInfo('trunk', 'trunk', 'trunk')
    self.assertEquals(trunk_info, self._branch_util.GetChannelInfo('trunk'))

    dev_info = ChannelInfo('dev', '1500', 28)
    self.assertEquals(dev_info, self._branch_util.GetChannelInfo('dev'))

    beta_info = ChannelInfo('beta', '1453', 27)
    self.assertEquals(beta_info, self._branch_util.GetChannelInfo('beta'))

    stable_info = ChannelInfo('stable', '1410', 26)
    self.assertEquals(stable_info, self._branch_util.GetChannelInfo('stable'))

  def testGetLatestVersionNumber(self):
    self.assertEquals(37, self._branch_util.GetLatestVersionNumber())

  def testGetBranchForVersion(self):
    self.assertEquals('1500',
        self._branch_util.GetBranchForVersion(28))
    self.assertEquals('1453',
        self._branch_util.GetBranchForVersion(27))
    self.assertEquals('1410',
        self._branch_util.GetBranchForVersion(26))
    self.assertEquals('1364',
        self._branch_util.GetBranchForVersion(25))
    self.assertEquals('1312',
        self._branch_util.GetBranchForVersion(24))
    self.assertEquals('1271',
        self._branch_util.GetBranchForVersion(23))
    self.assertEquals('1229',
        self._branch_util.GetBranchForVersion(22))
    self.assertEquals('1180',
        self._branch_util.GetBranchForVersion(21))
    self.assertEquals('1132',
        self._branch_util.GetBranchForVersion(20))
    self.assertEquals('1084',
        self._branch_util.GetBranchForVersion(19))
    self.assertEquals('1025',
        self._branch_util.GetBranchForVersion(18))
    self.assertEquals('963',
        self._branch_util.GetBranchForVersion(17))
    self.assertEquals('696',
        self._branch_util.GetBranchForVersion(11))
    self.assertEquals('396',
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
