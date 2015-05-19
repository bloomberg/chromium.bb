# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library for loading / using build configuration.

*** UPDATE NOTICE ***

Not finding what you're expecting here? Please see generate_chromeos_config.py
for Chrome OS build configuration.
"""

from __future__ import print_function

import os

# These symbols are imported to be exported (for now).
# pylint: disable=unused-import
# pylint: disable=wildcard-import
# pylint: disable=unused-wildcard-import

from chromite.cbuildbot import constants
from chromite.cbuildbot.config_lib import *
from chromite.lib import factory

from chromite.cbuildbot.generate_chromeos_config import (
    # Exported method for searching/modifying config information.
    FindCanonicalConfigForBoard,
    FindFullConfigsForBoard,
    GetCanariesForChromeLKGM,
    GetDisplayPosition,
    GetSlavesForMaster,
    OverrideConfigForTrybot,
    )


@factory.CachedFunctionCall
def GetConfig():
  """Fetch the global cbuildbot config."""
  # TODO(dgarrett): This constant will become a cbuildbot argument.
  config_file = os.path.join(constants.CHROMITE_DIR, 'cbuildbot',
                             'config_dump.json')
  return CreateConfigFromFile(config_file)


def GetDefault():
  """Fetch the global default BuildConfig."""
  config = GetConfig()
  return config.GetDefault()
