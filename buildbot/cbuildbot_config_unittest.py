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
from chromite.buildbot import cbuildbot_config

# pylint: disable=W0212,R0904
class CBuildBotTest(mox.MoxTestBase):

  def setUp(self):
    mox.MoxTestBase.setUp(self)

  def testConfigsKeysMismatch(self):
    """Verify that all configs contain exactly the default keys.

       This checks for mispelled keys, or keys that are somehow removed.
    """

    expected_keys = set(cbuildbot_config._default.keys())
    for build_name, config in cbuildbot_config.config.iteritems():
      config_keys = set(config.keys())

      extra_keys = config_keys.difference(expected_keys)
      self.assertFalse(extra_keys, ('Config %s has extra values %s' %
                                    (build_name, list(extra_keys))))

      missing_keys = expected_keys.difference(config_keys)
      self.assertFalse(missing_keys, ('Config %s is missing values %s' %
                                      (build_name, list(missing_keys))))

  def testConfigUseflags(self):
    """ Useflags must be lists.
        Strings are interpreted as arrays of characters for this, which is not
        useful.
    """

    for build_name, config in cbuildbot_config.config.iteritems():
      useflags = config.get('useflags')
      if not useflags is None:
        self.assertTrue(
          isinstance(useflags, list),
          'Config %s: useflags should be a list.' % build_name)

  def testBoardFlag(self):
    """Verify 'board' is explicitly set for every config."""

    for build_name, config in cbuildbot_config.config.iteritems():
      self.assertTrue(config['board'],
                      "Config %s doesn't have the board set." % build_name)

  def testOverlaySettings(self):
    """Verify overlays and push_overlays have legal values."""

    for build_name, config in cbuildbot_config.config.iteritems():
      overlays = config['overlays']
      push_overlays = config['push_overlays']

      self.assertTrue(overlays in [None, 'public', 'private', 'both'],
                      'Config %s: has unexpected overlays value.' % build_name)
      self.assertTrue(
          push_overlays in [None, 'public', 'private', 'both'],
          'Config %s: has unexpected push_overlays value.' % build_name)

      if overlays == None:
        subset = [None]
      elif overlays == 'public':
        subset = [None, 'public']
      elif overlays == 'private':
        subset = [None, 'private']
      elif overlays == 'both':
        subset = [None, 'public', 'private', 'both']

      self.assertTrue(
          push_overlays in subset,
          'Config %s: push_overlays should be a subset of overlays.' %
              build_name)

  def testChromeRev(self):
    """Verify chrome_rev has an expected value"""

    for build_name, config in cbuildbot_config.config.iteritems():
      self.assertTrue(
          config['chrome_rev'] in constants.VALID_CHROME_REVISIONS + [None],
          'Config %s: has unexpected chrome_rev value.' % build_name)
      self.assertFalse(
          config['chrome_rev'] == constants.CHROME_REV_LOCAL,
          'Config %s: has unexpected chrome_rev_local value.' % build_name)

  def testValidVMTestType(self):
    """Verify vm_tests has an expected value"""

    for build_name, config in cbuildbot_config.config.iteritems():
      self.assertTrue(
          config['vm_tests'] in constants.VALID_AU_TEST_TYPES + [None],
          'Config %s: has unexpected vm test type value.' % build_name)

  def testBuildType(self):
    """Verifies that all configs use valid build types."""
    for build_name, config in cbuildbot_config.config.iteritems():
      self.assertTrue(
          config['build_type'] in constants.VALID_BUILD_TYPES,
          'Config %s: has unexpected build_type value.' % build_name)

  def testGccDependancy(self):
    """Verify we don't set gcc_46 without also setting latest_toolchain."""

    for build_name, config in cbuildbot_config.config.iteritems():
      self.assertFalse(
          config['gcc_46'] and not config['latest_toolchain'],
          'Config %s: has gcc_46 without latest_toolchain.' % build_name)

  def testBuildToRun(self):
    """Verify we don't try to run tests without building them."""

    for build_name, config in cbuildbot_config.config.iteritems():
      self.assertFalse(
          isinstance(config['useflags'], list) and
          '-build_tests' in config['useflags'] and config['chrome_tests'],
          'Config %s: has chrome_tests and use -build_tests.' % build_name)

  def testARMNoVMTest(self):
    """Verify ARM builds don't get VMTests turned on by accident"""

    for build_name, config in cbuildbot_config.config.iteritems():
      if build_name.startswith('arm-'):
        self.assertTrue(config['vm_tests'] is None,
                        "ARM builder %s can't run vm tests!" % build_name)

  def testHWTestNeedsVMTest(self):
    """Verify VMTest is a prerequisite for HWTest"""

    for build_name, config in cbuildbot_config.config.iteritems():
      if config['hw_tests']:
        self.assertTrue(config['vm_tests'],
                        'Config %s: HWTest depend on VMTest' % build_name)


if __name__ == '__main__':
  unittest.main()
