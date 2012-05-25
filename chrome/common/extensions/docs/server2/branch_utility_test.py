#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
import test_urlfetch

from branch_utility import BranchUtility

class BranchUtilityTest(unittest.TestCase):
  def testGetChannelNameFromPath(self):
    b_util = BranchUtility(test_urlfetch)
    self.assertEquals('dev', b_util.GetChannelNameFromPath(
        'dev/hello/stuff.html'))
    self.assertEquals('beta', b_util.GetChannelNameFromPath(
        'beta/hello/stuff.html'))
    self.assertEquals('trunk', b_util.GetChannelNameFromPath(
        'trunk/hello/stuff.html'))
    self.assertEquals('stable', b_util.GetChannelNameFromPath(
        'hello/stuff.html'))
    self.assertEquals('stable', b_util.GetChannelNameFromPath(
        'hello/dev/stuff.html'))

  def testGetBranchNumberForChannelName(self):
    b_util = BranchUtility(test_urlfetch)
    b_util.SetURL('branch_utility/first.json')
    self.assertEquals('1132',
        b_util.GetBranchNumberForChannelName('dev'))
    self.assertEquals('1084',
        b_util.GetBranchNumberForChannelName('beta'))
    self.assertEquals('1234',
        b_util.GetBranchNumberForChannelName('stable'))
    self.assertEquals('trunk',
        b_util.GetBranchNumberForChannelName('trunk'))

if __name__ == '__main__':
  unittest.main()
