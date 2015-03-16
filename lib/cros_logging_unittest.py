# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for cros_logging."""

from __future__ import print_function

import sys

from chromite.lib import cros_logging as logging
from chromite.lib import cros_test_lib


class CrosloggingTest(cros_test_lib.OutputTestCase):
  """Test logging works as expected."""

  def testNotice(self):
    """Test logging.notice works and is between INFO and WARNING."""
    logger = logging.getLogger()
    sh = logging.StreamHandler(sys.stdout)
    logger.addHandler(sh)

    msg = 'notice message'

    logger.setLevel(logging.INFO)
    with self.OutputCapturer():
      logging.notice(msg)
    self.AssertOutputContainsLine(msg)

    logger.setLevel(logging.WARNING)
    with self.OutputCapturer():
      logging.notice(msg)
    self.AssertOutputContainsLine(msg, invert=True)
