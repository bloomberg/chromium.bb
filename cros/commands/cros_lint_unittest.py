#!/usr/bin/python
# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module tests the cros lint command."""

from __future__ import print_function

import os
import sys

sys.path.insert(0, os.path.abspath('%s/../../..' % os.path.dirname(__file__)))
from chromite.cros.commands import cros_lint
from chromite.lib import cros_test_lib


class LintCommandTest(cros_test_lib.TestCase):
  """Test class for our LintCommand class."""

  def testOutputArgument(self):
    """Tests that the --output argument mapping for cpplint is complete."""
    self.assertEqual(
        set(cros_lint.LintCommand.OUTPUT_FORMATS),
        set(cros_lint.CPPLINT_OUTPUT_FORMAT_MAP.keys() + ['default']))


if __name__ == '__main__':
  cros_test_lib.main()
