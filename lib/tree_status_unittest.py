# -*- coding: utf-8 -*-
# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test suite for tree_status.py"""

from __future__ import print_function

import mock
import urllib

from chromite.lib import cros_test_lib
from chromite.lib import tree_status


# pylint: disable=protected-access


class TestGettingGardenerEmails(cros_test_lib.MockTestCase):
  """Tests functions related to retrieving the gardener's email address."""

  def _SetEmails(self, emails):
    gardener_json = ('{"updated_unix_timestamp":1547254144,'
                     '"emails":[%s]}' % emails)
    response = mock.MagicMock(json=gardener_json, getcode=lambda: 200,
                              read=lambda: gardener_json)
    self.PatchObject(urllib, 'urlopen', autospec=True,
                     side_effect=[response])

  def testParsingGardenerEmails(self):
    self._SetEmails('"gardener@google.com"')
    self.assertEqual(tree_status.GetGardenerEmailAddresses(),
                     ['gardener@google.com'])

    # Test multiple gardeners.
    self._SetEmails('"gardener@google.com", "gardener2@chromium.org"')
    self.assertEqual(tree_status.GetGardenerEmailAddresses(),
                     ['gardener@google.com', 'gardener2@chromium.org'])


class TestUrlConstruction(cros_test_lib.TestCase):
  """Tests functions that create URLs."""

  def testConstructMiloBuildURL(self):
    """Tests generating Legoland build URLs."""
    output = tree_status.ConstructMiloBuildURL('bbid')
    expected = 'https://ci.chromium.org/b/bbid'

    self.assertEqual(output, expected)
