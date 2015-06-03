# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library for loading / using build configuration.

*** UPDATE NOTICE ***

Not finding what you're expecting here? Please see chromeos_config.py
for Chrome OS build configuration.
"""

from __future__ import print_function

from chromite.cbuildbot import config_lib
from chromite.cbuildbot import chromeos_config
from chromite.lib import factory


@factory.CachedFunctionCall
def GetConfig():
  """Fetch the global cbuildbot config."""
  return config_lib.CreateConfigFromFile(chromeos_config.CONFIG_FILE)
