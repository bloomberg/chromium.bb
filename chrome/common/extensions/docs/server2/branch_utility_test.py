#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import branch_utility
import unittest
import test_urlfetch

class BranchUtilityTest(unittest.TestCase):
  def testGetChannelNameFromPath(self):
    self.assertEquals('dev', branch_utility.GetChannelNameFromPath(
        'dev/hello/stuff.html'))
    self.assertEquals('beta', branch_utility.GetChannelNameFromPath(
        'beta/hello/stuff.html'))
    self.assertEquals('trunk', branch_utility.GetChannelNameFromPath(
        'trunk/hello/stuff.html'))
    self.assertEquals('stable', branch_utility.GetChannelNameFromPath(
        'hello/stuff.html'))
    self.assertEquals('stable', branch_utility.GetChannelNameFromPath(
        'hello/dev/stuff.html'))

  def testGetBranchNumberForChannelName(self):
    base_path = 'branch_utility/first.json'
    self.assertEquals('1132',
        branch_utility.GetBranchNumberForChannelName('dev',
                                                     test_urlfetch,
                                                     base_path))
    self.assertEquals('1084',
        branch_utility.GetBranchNumberForChannelName('beta',
                                                     test_urlfetch,
                                                     base_path))
    self.assertEquals('1234',
        branch_utility.GetBranchNumberForChannelName('stable',
                                                     test_urlfetch,
                                                     base_path))
    self.assertEquals('trunk',
        branch_utility.GetBranchNumberForChannelName('trunk',
                                                     test_urlfetch,
                                                     base_path))

if __name__ == '__main__':
  unittest.main()
