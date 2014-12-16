# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import sys

from pylib import constants

sys.path.append(os.path.join(
    constants.DIR_SOURCE_ROOT, 'third_party', 'requests', 'src'))
sys.path.append(os.path.join(
    constants.DIR_SOURCE_ROOT, 'third_party', 'appurify-python', 'src'))
handlers_before = list(logging.getLogger().handlers)

import appurify.api
import appurify.utils

handlers_after = list(logging.getLogger().handlers)
new_handler = list(set(handlers_after) - set(handlers_before))
while new_handler:
  logging.info("Removing logging handler.")
  logging.getLogger().removeHandler(new_handler.pop())

api = appurify.api
utils = appurify.utils
