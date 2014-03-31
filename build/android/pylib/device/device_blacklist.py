# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os

from pylib import constants
_BLACKLIST_JSON = os.path.join(
    constants.DIR_SOURCE_ROOT,
    os.environ.get('CHROMIUM_OUT_DIR', 'out'),
    'bad_devices.json')

def ReadBlacklist():
  """Reads the blacklist from the _BLACKLIST_JSON file.

  Returns:
    A list containing bad devices.
  """
  if not os.path.exists(_BLACKLIST_JSON):
    return []

  with open(_BLACKLIST_JSON, 'r') as f:
    return json.load(f)


def WriteBlacklist(blacklist):
  """Writes the provided blacklist to the _BLACKLIST_JSON file.

  Args:
    blacklist: list of bad devices to write to the _BLACKLIST_JSON file.
  """
  with open(_BLACKLIST_JSON, 'w') as f:
    json.dump(list(set(blacklist)), f)


def ExtendBlacklist(devices):
  """Adds devices to _BLACKLIST_JSON file.

  Args:
    devices: list of bad devices to be added to the _BLACKLIST_JSON file.
  """
  blacklist = ReadBlacklist()
  blacklist.extend(devices)
  WriteBlacklist(blacklist)


def ResetBlacklist():
  """Erases the _BLACKLIST_JSON file if it exists."""
  if os.path.exists(_BLACKLIST_JSON):
    os.remove(_BLACKLIST_JSON)

