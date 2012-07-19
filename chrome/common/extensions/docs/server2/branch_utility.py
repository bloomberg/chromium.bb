# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json

import appengine_memcache as memcache
import operator

class BranchUtility(object):
  def __init__(self, base_path, default_branch, fetcher, memcache):
    self._base_path = base_path
    self._default_branch = default_branch
    self._fetcher = fetcher
    self._memcache = memcache

  def SplitChannelNameFromPath(self, path):
    try:
      first, second = path.split('/', 1)
    except ValueError:
      first = path
      second = ''
    if first in ['trunk', 'dev', 'beta', 'stable']:
      return (first, second)
    else:
      return (self._default_branch, path)

  def GetBranchNumberForChannelName(self, channel_name):
    """Returns an empty string if the branch number cannot be found.
    Throws exception on network errors.
    """
    if channel_name in ['trunk', 'local']:
      return channel_name

    branch_number = self._memcache.Get(channel_name + '.' + self._base_path,
                                       memcache.MEMCACHE_BRANCH_UTILITY)
    if branch_number is not None:
      return branch_number

    fetch_data = self._fetcher.Fetch(self._base_path).content
    version_json = json.loads(fetch_data)
    branch_numbers = {}
    for entry in version_json:
      if entry['os'] not in ['win', 'linux', 'mac', 'cros']:
        continue
      for version in entry['versions']:
        if version['channel'] != channel_name:
          continue
        if version['true_branch'] not in branch_numbers:
          branch_numbers[version['true_branch']] = 0
        else:
          branch_numbers[version['true_branch']] += 1

    sorted_branches = sorted(branch_numbers.iteritems(),
                             None,
                             operator.itemgetter(1),
                             True)
    # Cache for 24 hours.
    self._memcache.Set(channel_name + '.' + self._base_path,
                       sorted_branches[0][0],
                       memcache.MEMCACHE_BRANCH_UTILITY,
                       86400)

    return sorted_branches[0][0]
