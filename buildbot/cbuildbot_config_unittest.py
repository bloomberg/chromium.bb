#!/usr/bin/python

# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for config.  Needs to be run inside of chroot for mox."""

import mox
import sys
import unittest

import constants
sys.path.append(constants.SOURCE_ROOT)
import chromite.buildbot.cbuildbot_config as config

class CBuildBotTest(mox.MoxTestBase):

  def setUp(self):
    mox.MoxTestBase.setUp(self)

  def testConfigsKeysMismatch(self):
    """Verify that all configs contain exactly the default keys.

       This checks for mispelled keys, or keys that are somehow removed.
    """

    expected_keys = set(config.default.keys())

    for x in config.config:
      config_keys = set(config.config[x].keys())

      extra_keys = config_keys.difference(expected_keys)
      self.assertFalse(extra_keys, ('Config %s has extra values %s' %
                                    (x, list(extra_keys))))

      missing_keys = expected_keys.difference(config_keys)
      self.assertFalse(missing_keys, ('Config %s is missing values %s' %
                                      (x, list(missing_keys))))

  def testConfigUseflags(self):
    """ Useflags must be lists.
        Strings are interpreted as arrays of characters for this, which is not
        useful.
    """

    for x in config.config:
      useflags = config.config[x].get('useflags')
      if not useflags is None:
        self.assertTrue(
          isinstance(useflags, list),
          'Config %s: useflags should be a list.' % x)

  def testBoardFlag(self):
    """Verify 'board' is explicitly set for every config."""

    for x in config.config:
      self.assertTrue(config.config[x]['board'],
                      "Config %s doesn't have the board set." % x)

  def testOverlaySettings(self):
    """Verify overlays and push_overlays have legal values."""

    for x in config.config:
      overlays = config.config[x]['overlays']
      push_overlays = config.config[x]['push_overlays']

      self.assertTrue(overlays in [None, 'public', 'private', 'both'],
                      'Config %s: has unexpected overlays value.' % x)

      self.assertTrue(push_overlays in [None, 'public', 'private', 'both'],
                      'Config %s: has unexpected push_overlays value.' % x)

      if overlays == None:
        subset = [None]
      elif overlays == 'public':
        subset = [None, 'public']
      elif overlays == 'private':
        subset = [None, 'private']
      elif overlays == 'both':
        subset = [None, 'public', 'private', 'both']

      msg = 'Config %s: push_overlays should be a subset of overlays.' % x
      self.assertTrue(push_overlays in subset, msg)

  def testChromeRev(self):
    """Verify chrome_rev has an expected value"""

    for x in config.config:
      chrome_rev = config.config[x]['chrome_rev']

      self.assertTrue(chrome_rev in [None, 'tot', 'stable_release'],
                      'Config %s: has unexpected chrome_rev value.' % x)

  def testBuildType(self):
    """Verify build_type has an expected value."""

    for x in config.config:
      build_type = config.config[x]['build_type']
      self.assertTrue(build_type in ['binary', 'full', 'chrome', 'chroot'],
                      'Config %s: has unexpected build_type value.' % x)

  def testGccDependancy(self):
    """Verify we don't set gcc_46 without also setting latest_toolchain."""

    for x in config.config:
      self.assertFalse(config.config[x]['gcc_46'] and
                       not config.config[x]['latest_toolchain'],
                       'Config %s: has gcc_46 without latest_toolchain.' % x)

if __name__ == '__main__':
  unittest.main()
