# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import operator

from appengine_url_fetcher import AppEngineUrlFetcher
import url_constants

class BranchUtility(object):
  def __init__(self, fetch_url, fetcher, object_store_creator):
    self._fetch_url = fetch_url
    self._fetcher = fetcher
    # BranchUtility is obviously cross-channel, so set the channel to None.
    self._object_store = object_store_creator.Create(BranchUtility,
                                                     channel=None)

  @staticmethod
  def Create(object_store_creator):
    return BranchUtility(url_constants.OMAHA_PROXY_URL,
                         AppEngineUrlFetcher(),
                         object_store_creator)

  @staticmethod
  def GetAllChannelNames():
    return ['stable', 'beta', 'dev', 'trunk']

  @staticmethod
  def SplitChannelNameFromPath(path):
    """Splits the channel name out of |path|, returning the tuple
    (channel_name, real_path). If the channel cannot be determined then returns
    (None, path).
    """
    if '/' in path:
      first, second = path.split('/', 1)
    else:
      first, second = (path, '')
    if first in ['trunk', 'dev', 'beta', 'stable']:
      return (first, second)
    return (None, path)

  def GetBranchForChannel(self, channel_name):
    """Returns the branch number for a channel name.
    """
    if channel_name == 'trunk':
      return 'trunk'

    branch_number = self._object_store.Get(channel_name).Get()
    if branch_number is not None:
      return branch_number

    try:
      version_json = json.loads(self._fetcher.Fetch(self._fetch_url).content)
    except Exception as e:
      # This can happen if omahaproxy is misbehaving, which we've seen before.
      # Quick hack fix: just serve from trunk until it's fixed.
      logging.error('Failed to fetch or parse branch from omahaproxy: %s! '
                    'Falling back to "trunk".' % e)
      return 'trunk'

    branch_numbers = {}
    for entry in version_json:
      if entry['os'] not in ['win', 'linux', 'mac', 'cros']:
        continue
      for version in entry['versions']:
        if version['channel'] != channel_name:
          continue
        branch = version['version'].split('.')[2]
        if branch not in branch_numbers:
          branch_numbers[branch] = 0
        else:
          branch_numbers[branch] += 1

    sorted_branches = sorted(branch_numbers.iteritems(),
                             None,
                             operator.itemgetter(1),
                             True)
    self._object_store.Set(channel_name, sorted_branches[0][0])

    return sorted_branches[0][0]
