# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Declares a number of site-dependent variables for use by scripts.
"""

import os
from twisted.spread import banana

# DatabaseSetup was moved. Import it for backward compatibility
from common.chromium_utils import DatabaseSetup  # pylint: disable=W0611
from config_bootstrap import config_private # pylint: disable=W0403,W0611
from config_bootstrap import Master # pylint: disable=W0403,W0611

SITE_CONFIG_PATH = os.path.abspath(os.path.dirname(__file__))

# By default, the banana's string size limit is 640kb, which is unsufficient
# when passing diff's around. Raise it to 100megs. Do this here since the limit
# is enforced on both the server and the client so both need to raise the
# limit.
banana.SIZE_LIMIT = 100 * 1024 * 1024


def SiteConfigPath():
  return SITE_CONFIG_PATH






