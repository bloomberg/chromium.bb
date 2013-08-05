#!/usr/bin/env python
#
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs content_browsertests."""

import logging
import os
import sys

from pylib import cmd_helper


if __name__ == '__main__':
  args = [os.path.join(os.path.dirname(__file__), 'test_runner.py'),
          'gtest', '-s', 'content_browsertests'] + sys.argv[1:]
  logging.warning('*' * 80)
  logging.warning('This script is deprecated and will be removed soon.')
  logging.warning('Use the following instead: %s', ' '.join(args))
  logging.warning('*' * 80)
  sys.exit(1)
