#!/usr/bin/python

# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for config.  Needs to be run inside of chroot for mox."""

import json
import mox
import os
import re
import subprocess
import sys
import unittest
import urllib

import constants
sys.path.insert(0, constants.SOURCE_ROOT)
from chromite.buildbot import cbuildbot_config
from chromite.lib import cros_build_lib

CHROMIUM_WATCHING_URL = ("http://src.chromium.org/viewvc/" +
    "chrome/trunk/tools/build/masters/" +
    "master.chromium.chromiumos" + "/" +
    "master_chromiumos_cros_cfg.py")

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

  def testConfigsHaveName(self):
    """ Configs must have names set."""
    for build_name, config in cbuildbot_config.config.iteritems():
      self.assertTrue(build_name == config['name'])

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

  def testBoards(self):
    """Verify 'boards' is explicitly set for every config."""

    for build_name, config in cbuildbot_config.config.iteritems():
      self.assertTrue(isinstance(config['boards'], list),
                      "Config %s doesn't have a list of boards." % build_name)
      self.assertEqual(len(set(config['boards'])), len(config['boards']),
                       'Config %s has duplicate boards.' % build_name)
      self.assertTrue(config['boards'],
                      'Config %s has at least one board.' % build_name)

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

  def testOverlayMaster(self):
    """Verify that only one master is pushing uprevs for each overlay."""
    masters = {}
    for build_name, config in cbuildbot_config.config.iteritems():
      overlays = config['overlays']
      push_overlays = config['push_overlays']
      if (overlays and push_overlays and config['uprev'] and config['master']
          and not config['branch'] and not config['unified_manifest_version']):
        other_master = masters.get(push_overlays)
        err_msg = 'Found two masters for push_overlays=%s: %s and %s'
        self.assertFalse(other_master,
            err_msg % (push_overlays, build_name, other_master))
        masters[push_overlays] = build_name

    if 'both' in masters:
      self.assertEquals(len(masters), 1, 'Found too many masters.')

  def testChromeRev(self):
    """Verify chrome_rev has an expected value"""

    for build_name, config in cbuildbot_config.config.iteritems():
      self.assertTrue(
          config['chrome_rev'] in constants.VALID_CHROME_REVISIONS + [None],
          'Config %s: has unexpected chrome_rev value.' % build_name)
      self.assertFalse(
          config['chrome_rev'] == constants.CHROME_REV_LOCAL,
          'Config %s: has unexpected chrome_rev_local value.' % build_name)
      if config['chrome_rev']:
        self.assertTrue(cbuildbot_config.IsPFQType(config['build_type']),
          'Config %s: has chrome_rev but is not a PFQ.' % build_name)

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
          config['build_type'] in cbuildbot_config.BUILD_TYPE_DUMP_ORDER,
          'Config %s: has unexpected build_type value.' % build_name)

  def testGCCGitHash(self):
    """Verifies that gcc_githash is not set without setting latest_toolchain."""
    for build_name, config in cbuildbot_config.config.iteritems():
      if config['gcc_githash']:
        self.assertTrue(
            config['latest_toolchain'],
            'Config %s: has gcc_githash but not latest_toolchain.' % build_name)

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
      if build_name.startswith('arm-') or config['arm']:
        self.assertTrue(config['vm_tests'] is None,
                        "ARM builder %s can't run vm tests!" % build_name)
        self.assertTrue(config['chrome_tests'] is False,
                        "ARM builder %s can't run chrome tests!" % build_name)

  def testImportantMattersToChrome(self):
    # TODO(ferringb): Decorate this as a network test.
    namefinder = re.compile(r" *params='([^' ]*)[ ']")
    req = urllib.urlopen(CHROMIUM_WATCHING_URL)
    watched_configs = []
    for m in [namefinder.match(line) for line in req.read().splitlines()]:
      if m:
        watched_configs.append(m.group(1))

    watched_boards = []
    for config in watched_configs:
      watched_boards.extend(cbuildbot_config.config[config]['boards'])

    watched_boards = set(watched_boards)

    for build_name, config in cbuildbot_config.config.iteritems():
      if (config['important'] and
          config['chrome_rev'] == constants.CHROME_REV_LATEST and
          config['overlays'] == constants.PUBLIC_OVERLAYS and
          config['build_type'] == constants.CHROME_PFQ_TYPE):
        boards = set(config['boards'])
        self.assertTrue(boards.issubset(watched_boards),
                        'Config %s: boards %r are not watched on Chromium' %
                        (build_name, list(boards - watched_boards)))

  #TODO: Add test for compare functionality
  def testJSONDumpLoadable(self):
    """Make sure config export functionality works."""
    cwd = os.path.dirname(os.path.abspath(__file__))
    output = subprocess.Popen(['./cbuildbot_config.py', '--dump'],
                              stdout=subprocess.PIPE, cwd=cwd).communicate()[0]
    configs = json.loads(output)
    self.assertFalse(not configs)


  def testJSONBuildbotDumpHasOrder(self):
    """Make sure config export functionality works."""
    cwd = os.path.dirname(os.path.abspath(__file__))
    output = subprocess.Popen(['./cbuildbot_config.py', '--dump',
                               '--for-buildbot'],
                              stdout=subprocess.PIPE, cwd=cwd).communicate()[0]
    configs = json.loads(output)
    for cfg in configs.itervalues():
      self.assertTrue(cfg['display_position'] is not None)

    self.assertFalse(not configs)

  def testHWTestsIFFArchivingHWTestArtifacts(self):
    """Make sure all configs upload artifacts that need them for hw testing."""
    for build_name, config in cbuildbot_config.config.iteritems():
      if config['hw_tests']:
        self.assertTrue(
            config['upload_hw_test_artifacts'],
            "%s is trying to run hw tests without uploading payloads." %
            build_name)

  def testValidUnifiedMasterConfig(self):
    """Make sure any unified master configurations are valid."""
    for build_name, config in cbuildbot_config.config.iteritems():
      error = 'Unified config for %s has invalid values' % build_name
      # Unified masters must be internal and must rev both overlays.
      if config['unified_manifest_version'] and config['master']:
        self.assertTrue(
            config['internal'] and config['manifest_version'], error)
        self.assertEqual(config['overlays'], constants.BOTH_OVERLAYS)
      elif config['unified_manifest_version'] and not config['master']:
        # Unified slaves can rev either public or both depending on whether
        # they are internal or not.
        self.assertTrue(config['manifest_version'], error)
        if config['internal']:
          self.assertEqual(config['overlays'], constants.BOTH_OVERLAYS, error)
        else:
          self.assertEqual(config['overlays'], constants.PUBLIC_OVERLAYS, error)

  def testFactoryFirmwareValidity(self):
    """Ensures that firmware/factory branches have at least 1 valid name."""
    tracking_branch = cros_build_lib.GetChromiteTrackingBranch()
    for branch in ['firmware', 'factory']:
      if tracking_branch.startswith(branch):
        saw_config_for_branch = False
        for build_name in cbuildbot_config.config:
          if build_name.endswith('-%s' % branch):
            self.assertFalse('release' in build_name,
                             'Factory|Firmware release builders should not '
                             'contain release in their name.')
            saw_config_for_branch = True

        self.assertTrue(
            saw_config_for_branch, 'No config found for %s branch. '
            'As this is the %s branch, all release configs that are being used '
            'must end in %s.' % (branch, tracking_branch, branch))

  def testBuildTests(self):
    """Verify that we don't try to use tests without building them."""

    for build_name, config in cbuildbot_config.config.iteritems():
      if not config['build_tests']:
        self.assertFalse('factory_test' in config['images'],
            'Config %s builds factory_test without build_tests.' % build_name)
        self.assertFalse('test' in config['images'],
            'Config %s builds test image without build_tests.' % build_name)
        for flag in ('vm_tests', 'hw_tests', 'upload_hw_test_artifacts'):
          self.assertFalse(config[flag],
              'Config %s set %s without build_tests.' % (build_name, flag))

  def testChromeTestsImpliesVMTests(self):
    """Verify that all bots with Chrome tests also have VM tests."""

    for build_name, config in cbuildbot_config.config.iteritems():
      if not config['vm_tests']:
        self.assertFalse(config['chrome_tests'],
           'chrome_tests is enabled for %s without vm_tests' % (build_name,))

  def testChromePFQsNeedChromeOSPFQs(self):
    """Make sure every Chrome PFQ has a matching ChromeOS PFQ for prebuilts.

    See http://crosbug.com/31695 for details on why.

    Note: This check isn't perfect (as we can't check to see if we have
    any ChromeOS PFQs actually running), but it at least makes sure people
    realize we need to have the configs in sync."""

    # Figure out all the boards our Chrome and ChromeOS PFQs are using.
    cr_pfqs = set()
    cros_pfqs = set()
    for config in cbuildbot_config.config.itervalues():
      if config['build_type'] == constants.CHROME_PFQ_TYPE:
        cr_pfqs.update(config['boards'])
      elif config['build_type'] == constants.PALADIN_TYPE:
        cros_pfqs.update(config['boards'])

    # Then make sure the ChromeOS PFQ set is a superset of the Chrome PFQ set.
    missing_pfqs = cr_pfqs.difference(cros_pfqs)
    if missing_pfqs:
      self.fail('Chrome PFQs are using boards that are missing ChromeOS PFQs:'
                '\n\t' + ' '.join(missing_pfqs))


if __name__ == '__main__':
  cros_build_lib.SetupBasicLogging()
  unittest.main()
