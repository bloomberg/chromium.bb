# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import json

OMAHA_PROXY_URL = 'http://omahaproxy.appspot.com/json'

def GetChannelNameFromPath(path):
  first_part = path.split('/')[0]
  if first_part in ['trunk', 'dev', 'beta', 'stable', 'local']:
    return first_part
  else:
    return 'stable'

def GetBranchNumberForChannelName(channel_name,
                                  urlfetch,
                                  base_path=OMAHA_PROXY_URL):
  """Returns an empty string if the branch number cannot be found.
  Throws exception on network errors.
  """
  if channel_name == 'trunk' or channel_name == 'local':
    return channel_name

  fetch_data = urlfetch.fetch(base_path)
  if fetch_data.content == '':
    raise Exception('Fetch returned zero results.')

  version_json = json.loads(fetch_data.content)
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

  sorted_list = [x for x in branch_numbers.iteritems()]
  sorted_list.sort(key = lambda x: x[1])
  sorted_list.reverse()

  branch_number, _ = sorted_list[0]

  return branch_number
