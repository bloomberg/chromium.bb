# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library for loading / using build configuration.

*** UPDATE NOTICE ***

Not finding what you're expecting here? Please see generate_chromeos_config.py
for Chrome OS build configuration.
"""

from __future__ import print_function

import json
import os

# These symbols are imported to be exported (for now).
# pylint: disable=unused-import
# pylint: disable=wildcard-import
# pylint: disable=unused-wildcard-import

from chromite.cbuildbot import constants
from chromite.cbuildbot.config_lib import *
from chromite.lib import factory

# In the Json, this special build config holds the default values for all
# other configs.
DEFAULT_BUILD_CONFIG = '_default'

from chromite.cbuildbot.generate_chromeos_config import (
    # Exported method for searching/modifying config information.
    FindCanonicalConfigForBoard,
    FindFullConfigsForBoard,
    GetCanariesForChromeLKGM,
    GetDisplayPosition,
    GetSlavesForMaster,
    OverrideConfigForTrybot,
    )


def _DecodeList(data):
  """Convert a JSON result list from unicode to utf-8."""
  rv = []
  for item in data:
    if isinstance(item, unicode):
      item = item.encode('utf-8')
    elif isinstance(item, list):
      item = _DecodeList(item)
    elif isinstance(item, dict):
      item = _DecodeDict(item)

    # Other types (None, int, float, etc) are stored unmodified.
    rv.append(item)
  return rv


def _DecodeDict(data):
  """Convert a JSON result dict from unicode to utf-8."""
  rv = {}
  for key, value in data.iteritems():
    if isinstance(key, unicode):
      key = key.encode('utf-8')

    if isinstance(value, unicode):
      value = value.encode('utf-8')
    elif isinstance(value, list):
      value = _DecodeList(value)
    elif isinstance(value, dict):
      value = _DecodeDict(value)

    # Other types (None, int, float, etc) are stored unmodified.
    rv[key] = value
  return rv


def _CreateHwTestConfig(jsonString):
  """Create a HWTestConfig object from a JSON string."""
  # Each HW Test is dumped as a json string embedded in json.
  hw_test_config = json.loads(jsonString, object_hook=_DecodeDict)
  return HWTestConfig(**hw_test_config)


def _CreateBuildConfig(default, build_dict):
  """Create a BuildConfig object from it's parsed JSON dictionary encoding."""
  # These build config values need special handling.
  hwtests = build_dict.pop('hw_tests', None)
  child_configs = build_dict.pop('child_configs', None)

  result = default.derive(**build_dict)

  if hwtests is not None:
    result['hw_tests'] = [_CreateHwTestConfig(hwtest) for hwtest in hwtests]

  if child_configs is not None:
    result['child_configs'] = [_CreateBuildConfig(default, child)
                               for child in child_configs]

  return result


@factory.CachedFunctionCall
def _LoadConfig():
  """Load a cbuildbot config from it's JSON encoding."""
  # TODO(dgarrett): This constant will become a cbuildbot argument.
  config_file = os.path.join(constants.CHROMITE_DIR, 'cbuildbot',
                             'config_dump.json')

  with open(config_file) as fh:
    config_dict = json.load(fh, object_hook=_DecodeDict)

  # defaults is a dictionary of default build configuration values.
  defaults = config_dict.pop(DEFAULT_BUILD_CONFIG)

  defaultBuildConfig = BuildConfig(**defaults)

  builds = {n: _CreateBuildConfig(defaultBuildConfig, v)
            for n, v in config_dict.iteritems()}

  # config is the struct that holds the complete cbuildbot config.
  result = Config(defaults=defaults)
  result.update(builds)

  return result


def GetConfig():
  """Fetch the global cbuildbot config."""
  return _LoadConfig()


def GetDefault():
  """Fetch the global default BuildConfig."""
  config = _LoadConfig()
  return config.GetDefault()
