# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from branch_utility import BranchUtility

class TestBranchUtility(object):
  '''Mimics BranchUtility to return valid-ish data without needing omahaproxy
  data.
  '''
  def GetBranchForChannel(self, channel_name):
    return channel_name
