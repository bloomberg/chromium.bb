#!/usr/bin/python
# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for clactions methods."""

from __future__ import print_function

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))

from chromite.lib import cros_test_lib

class CLActionTest(cros_test_lib.TestCase):
  """Placeholder for clactions unit tests."""
  def runTest(self):
    pass


if __name__ == '__main__':
  cros_test_lib.main()
