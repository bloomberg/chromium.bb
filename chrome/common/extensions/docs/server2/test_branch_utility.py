# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from branch_utility import BranchUtility, ChannelInfo
from test_data.canned_data import (CANNED_BRANCHES, CANNED_CHANNELS)

class TestBranchUtility(object):
  '''Mimics BranchUtility to return valid-ish data without needing omahaproxy
  data.
  '''
  def __init__(self, branches, channels):
    ''' Parameters: |branches| is a mapping of versions to branches, and
    |channels| is a mapping of channels to versions.
    '''
    self._branches = branches
    self._channels = channels

  @staticmethod
  def CreateWithCannedData():
    '''Returns a TestBranchUtility that uses 'canned' test data pulled from
    older branches of SVN data.
    '''
    return TestBranchUtility(CANNED_BRANCHES, CANNED_CHANNELS)

  def GetAllChannelInfo(self):
    return [self.GetChannelInfo(channel)
            for channel in BranchUtility.GetAllChannelNames()]

  def GetChannelInfo(self, channel):
    version = self._channels[channel]
    return ChannelInfo(channel, self.GetBranchForVersion(version), version)

  def GetBranchForVersion(self, version):
    return self._branches[version]

  def GetChannelForVersion(self, version):
    for channel in self._channels.iterkeys():
      if self._channels[channel] == version:
        return channel
