# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This file is heavily based off of LUCI test_support/test_env.py."""

from __future__ import print_function

import os
import sys

# /appengine/
ROOT_DIR = os.path.dirname(
    os.path.dirname(os.path.realpath(os.path.abspath(__file__))))

_INITIALIZED = False


def setup_test_env():
  """Sets up test environment."""
  global _INITIALIZED  # pylint: disable=global-statement
  if _INITIALIZED:
    raise Exception('Do not call test_env.setup_test_env() twice.')
  _INITIALIZED = True

  # For 'from components import ...' and 'from test_support import ...'.
  sys.path.insert(0, ROOT_DIR)
  sys.path.insert(0, os.path.join(ROOT_DIR, '..', 'third_party_local'))

  # pylint: disable=import-error
  import utils
  utils.fix_protobuf_package()
