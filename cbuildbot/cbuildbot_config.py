# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library for loading / using build configuration.

*** UPDATE NOTICE ***

Not finding what you're expecting here? Please see generate_chromeos_config.py
for Chrome OS build configuration.
"""

from __future__ import print_function

# These symbols are imported to be exported (for now).
# pylint: disable=unused-import
# pylint: disable=wildcard-import
# pylint: disable=unused-wildcard-import

from chromite.cbuildbot import constants
from chromite.cbuildbot.config_lib import *
from chromite.cbuildbot import generate_chromeos_config
from chromite.lib import factory

from chromite.cbuildbot.generate_chromeos_config import (
    # Exported method for searching/modifying config information.
    OverrideConfigForTrybot,
    )


@factory.CachedFunctionCall
def GetConfig():
  """Fetch the global cbuildbot config."""
  return CreateConfigFromFile(generate_chromeos_config.CONFIG_FILE)


def GetDefault():
  """Fetch the global default BuildConfig."""
  config = GetConfig()
  return config.GetDefault()
