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

from chromite.cbuildbot import constants
from chromite.lib import factory

# In the Json, this special build config holds the default values for all
# other configs.
DEFAULT_BUILD_CONFIG = '_default'

# These symbols are imported to be exported, not for use.
# pylint: disable=unused-import

from chromite.cbuildbot.generate_chromeos_config import (
    # Classes used to generate build configs.
    BuildConfig,
    HWTestConfig,

    # General constant.
    GS_PATH_DEFAULT,

    # Define config types.
    CONFIG_TYPE_PRECQ,
    CONFIG_TYPE_PALADIN,
    CONFIG_TYPE_RELEASE,
    CONFIG_TYPE_FULL,
    CONFIG_TYPE_FIRMWARE,
    CONFIG_TYPE_FACTORY,
    CONFIG_TYPE_RELEASE_AFDO,

    # Contains the valid build config suffixes in the order that they are
    # dumped.
    CONFIG_TYPE_DUMP_ORDER,

    # Exported methods for searching config information.
    FindCanonicalConfigForBoard,
    FindFullConfigsForBoard,
    GetCanariesForChromeLKGM,
    GetDisplayPosition,
    GetSlavesForMaster,

    # Tests for config type categories.
    IsCanaryType,
    IsCQType,
    IsPFQType,
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


def _LoadHwTestConfig(jsonString):
  """Load a HWTestConfig object from it's JSON encoding."""
  # Each HW Test is dumped as a json string embedded in json.
  hw_test_config = json.loads(jsonString, object_hook=_DecodeDict)
  return HWTestConfig(**hw_test_config)


def _LoadBuildConfig(default, parsedJson):
  """Load a BuildConfig object from it's JSON encoding."""
  # These build config values need special handling.
  hwtests = parsedJson.pop('hw_tests', None)
  child_configs = parsedJson.pop('child_configs', None)

  result = default.derive(**parsedJson)

  if hwtests is not None:
    result['hw_tests'] = [_LoadHwTestConfig(hwtest) for hwtest in hwtests]

  if child_configs is not None:
    result['child_configs'] = [_LoadBuildConfig(default, child)
                               for child in child_configs]

  return result


@factory.CachedFunctionCall
def _LoadConfig():
  """Load a cbuildbot config from it's JSON encoding."""
  # TODO(dgarrett): This constant will become a cbuildbot argument.
  config_file = os.path.join(constants.CHROMITE_DIR, 'cbuildbot',
                             'config_dump.json')

  with open(config_file) as fh:
    config = json.load(fh, object_hook=_DecodeDict)

  # _DEFAULT is a dictionary of default build configuration values.
  default = config.pop(DEFAULT_BUILD_CONFIG)

  defaultBuildConfig = BuildConfig(**default)

  # _CONFIG is a dictionary mapping build name to BuildConfigs.
  config = {n: _LoadBuildConfig(defaultBuildConfig, v)
            for n, v in config.iteritems()}

  return config, default


def GetConfig():
  """Fetch the global cbuildbot config."""
  config, _ = _LoadConfig()
  return config


def GetDefault():
  """Fetch the global default BuildConfig."""
  _, default = _LoadConfig()
  return default
