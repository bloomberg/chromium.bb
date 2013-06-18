# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from branch_utility import BranchUtility, ChannelInfo

class TestBranchUtility(object):
  '''Mimics BranchUtility to return valid-ish data without needing omahaproxy
  data.
  '''
  def GetAllChannelInfo(self):
    return [self.GetChannelInfo(channel)
            for channel in BranchUtility.GetAllChannelNames()]

  def GetChannelInfo(self, channel):
    return ChannelInfo(channel,
                       'fakebranch-%s' % channel,
                       'fakeversion-%s' % channel)

  def GetBranchForVersion(self, version):
    return 'fakebranch-%s' % version
