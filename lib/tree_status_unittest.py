# -*- coding: utf-8 -*-
# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test suite for tree_status.py"""

from __future__ import print_function

from chromite.lib import cros_test_lib
from chromite.lib import tree_status


class TestUrlConstruction(cros_test_lib.TestCase):
  """Tests functions that create URLs."""

  def testConstructMiloBuildURL(self):
    """Tests generating Legoland build URLs."""
    output = tree_status.ConstructMiloBuildURL('bbid')
    expected = 'https://ci.chromium.org/b/bbid'

    self.assertEqual(output, expected)
