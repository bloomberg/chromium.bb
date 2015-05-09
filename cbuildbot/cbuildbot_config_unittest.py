# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for config."""

from __future__ import print_function

from chromite.cbuildbot import cbuildbot_config
from chromite.cbuildbot import generate_chromeos_config
from chromite.lib import cros_test_lib

class LoadChromeOsConfigTest(cros_test_lib.TestCase):
  """Tests that load the production Chrome OS config."""

  def testGeneralLoad(self):
    """This test compares generate_chromeos_config to config_dump.json."""
    # If there is a test failure, the diff will be big.
    self.maxDiff = None

    self.assertDictEqual(generate_chromeos_config.GetDefault(),
                         cbuildbot_config.GetDefault())

    new = cbuildbot_config.GetConfig()
    src = generate_chromeos_config.GetConfig()

    #
    # BUG ALERT ON TEST FAILURE
    #
    # assertDictEqual can correctly compare these structs for equivalence, but
    # has a bug when displaying differences on failure. The embedded
    # HWTestConfig values are correctly compared, but ALWAYS display as
    # different, if something else triggers a failure.
    #

    # This for loop is to make differences easier to find/read.
    for name in src.iterkeys():
      self.assertDictEqual(new[name], src[name])

    # This confirms they are exactly the same.
    self.assertDictEqual(new, src)
