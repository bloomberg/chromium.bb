# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

SUBVERSION_URL = 'http://src.chromium.org/viewvc/chrome/'
TRUNK_URL = SUBVERSION_URL + 'trunk/'
BRANCH_URL = SUBVERSION_URL + 'branches/'

class SubversionFetcher(object):
  """Class to fetch code from src.chromium.org.
  """
  def __init__(self, urlfetch):
    self.urlfetch = urlfetch

  def _GetURLFromBranch(self, branch):
    if branch == 'trunk':
      return TRUNK_URL
    return BRANCH_URL + branch + '/'

  def FetchResource(self, branch, path):
    url = self._GetURLFromBranch(branch) + path
    result = self.urlfetch.fetch(url)
    return result
