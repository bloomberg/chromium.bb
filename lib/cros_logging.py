# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Logging module to be used by all scripts.

cros_logging is a wrapper around logging with additional support for NOTICE
level. This is to be used instead of the default logging module. The new
logging level can only be used from here.
"""

from __future__ import print_function

# pylint: disable=unused-wildcard-import, wildcard-import
from logging import *
# pylint: enable=unused-wildcard-import, wildcard-import

# Have to import shutdown explicitly from logging because it is not included
# in logging's __all__.
# pylint: disable=unused-import
from logging import shutdown
# pylint: enable=unused-import


# Notice Level.
NOTICE = 25
addLevelName(NOTICE, 'NOTICE')


# Notice implementation.
def notice(message, *args, **kwargs):
  """Log 'msg % args' with severity 'NOTICE'."""
  log(NOTICE, message, *args, **kwargs)
