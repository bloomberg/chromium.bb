# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for config."""

from __future__ import print_function

import mock
import re
import cPickle

from chromite.cbuildbot import builders
from chromite.cbuildbot import chromeos_config
from chromite.cbuildbot import config_lib
from chromite.cbuildbot import constants
from chromite.cbuildbot.builders import generic_builders
from chromite.lib import cros_test_lib
from chromite.lib import git
from chromite.lib import osutils

# pylint: disable=protected-access

CHROMIUM_WATCHING_URL = (
    'http://src.chromium.org/chrome/trunk/tools/build/masters/'
    'master.chromium.chromiumos/master_chromiumos_cros_cfg.py'
)


class GenerateChromeosConfigTestBase(cros_test_lib.TestCase):
  """Base class for tests of chromeos_config.."""

  def setUp(self):
    self.all_configs = chromeos_config.GetConfig()


class ConfigDumpTest(GenerateChromeosConfigTestBase):
  """Tests related to config_dump.json & chromeos_config.py"""

  def testDump(self):
    """Make sure the json & config are kept in sync"""
    new_dump = self.all_configs.SaveConfigToString()
    old_dump = osutils.ReadFile(config_lib.CONFIG_FILE).rstrip()

    self.assertTrue(
        new_dump == old_dump, 'config_dump.json does not match the '
        'configs defined in chromeos_config.py. Run '
        'bin/cbuildbot_view_config > cbuildbot/config_dump.json')

  def testSaveLoadReload(self):
    """Make sure that loading and reloading the config is a no-op."""
    dump = self.all_configs.SaveConfigToString()
    loaded = config_lib.LoadConfigFromString(dump)
    self.assertEqual(self.all_configs, loaded)


class ConfigPickleTest(GenerateChromeosConfigTestBase):
  """Test that a config object is pickleable."""

  def testPickle(self):
    bc1 = self.all_configs['x86-mario-paladin']
    bc2 = cPickle.loads(cPickle.dumps(bc1))

    self.assertEquals(bc1.boards, bc2.boards)
    self.assertEquals(bc1.name, bc2.name)


class ConfigClassTest(GenerateChromeosConfigTestBase):
  """Tests of the config class itself."""

  def testAppendUseflags(self):
    base_config = config_lib.BuildConfig()
    inherited_config_1 = base_config.derive(
        useflags=chromeos_config.append_useflags(
            ['foo', 'bar', '-baz']))
    inherited_config_2 = inherited_config_1.derive(
        useflags=chromeos_config.append_useflags(['-bar', 'baz']))
    self.assertEqual(inherited_config_1.useflags, ['-baz', 'bar', 'foo'])
    self.assertEqual(inherited_config_2.useflags, ['-bar', 'baz', 'foo'])


class CBuildBotTest(GenerateChromeosConfigTestBase):
  """General tests of chromeos_config."""

  def testConfigsKeysMismatch(self):
    """Verify that all configs contain exactly the default keys.

    This checks for mispelled keys, or keys that are somehow removed.
    """
    expected_keys = set(self.all_configs.GetDefault().iterkeys())
    for build_name, config in self.all_configs.iteritems():
      config_keys = set(config.keys())

      extra_keys = config_keys.difference(expected_keys)
      self.assertFalse(extra_keys, ('Config %s has extra values %s' %
                                    (build_name, list(extra_keys))))

      missing_keys = expected_keys.difference(config_keys)
      self.assertFalse(missing_keys, ('Config %s is missing values %s' %
                                      (build_name, list(missing_keys))))

  def testConfigsHaveName(self):
    """Configs must have names set."""
    for build_name, config in self.all_configs.iteritems():
      self.assertTrue(build_name == config['name'])

  def testConfigUseflags(self):
    """Useflags must be lists.

    Strings are interpreted as arrays of characters for this, which is not
    useful.
    """
    for build_name, config in self.all_configs.iteritems():
      useflags = config.get('useflags')
      if not useflags is None:
        self.assertTrue(
            isinstance(useflags, list),
            'Config %s: useflags should be a list.' % build_name)

  def testBoards(self):
    """Verify 'boards' is explicitly set for every config."""
    for build_name, config in self.all_configs.iteritems():
      self.assertTrue(isinstance(config['boards'], (tuple, list)),
                      "Config %s doesn't have a list of boards." % build_name)
      self.assertEqual(len(set(config['boards'])), len(config['boards']),
                       'Config %s has duplicate boards.' % build_name)
      self.assertTrue(config['boards'] is not None,
                      'Config %s defines a list of boards.' % build_name)

  def testOverlaySettings(self):
    """Verify overlays and push_overlays have legal values."""
    for build_name, config in self.all_configs.iteritems():
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
          ('Config %s: push_overlays should be a subset of overlays.' %
           build_name))

  def testOverlayMaster(self):
    """Verify that only one master is pushing uprevs for each overlay."""
    masters = {}
    for build_name, config in self.all_configs.iteritems():
      overlays = config['overlays']
      push_overlays = config['push_overlays']
      if (overlays and push_overlays and config['uprev'] and config['master']
          and not config['branch']):
        other_master = masters.get(push_overlays)
        err_msg = 'Found two masters for push_overlays=%s: %s and %s'
        self.assertFalse(
            other_master, err_msg % (push_overlays, build_name, other_master))
        masters[push_overlays] = build_name

    if 'both' in masters:
      self.assertEquals(len(masters), 1, 'Found too many masters.')

  def testChromeRev(self):
    """Verify chrome_rev has an expected value"""
    for build_name, config in self.all_configs.iteritems():
      self.assertTrue(
          config['chrome_rev'] in constants.VALID_CHROME_REVISIONS + [None],
          'Config %s: has unexpected chrome_rev value.' % build_name)
      self.assertFalse(
          config['chrome_rev'] == constants.CHROME_REV_LOCAL,
          'Config %s: has unexpected chrome_rev_local value.' % build_name)
      if config['chrome_rev']:
        self.assertTrue(
            config_lib.IsPFQType(config['build_type']),
            'Config %s: has chrome_rev but is not a PFQ.' % build_name)

  def testValidVMTestType(self):
    """Verify vm_tests has an expected value"""
    for build_name, config in self.all_configs.iteritems():
      if config['vm_tests'] is None:
        continue
      for test_type in config['vm_tests']:
        self.assertTrue(
            test_type in constants.VALID_VM_TEST_TYPES,
            'Config %s: has unexpected vm test type value.' % build_name)

  def testImageTestMustHaveBaseImage(self):
    """Verify image_test build is only enabled with 'base' in images."""
    for build_name, config in self.all_configs.iteritems():
      if config.get('image_test', False):
        self.assertTrue(
            'base' in config['images'],
            'Build %s runs image_test but does not have base image' %
            build_name)

  def testBuildType(self):
    """Verifies that all configs use valid build types."""
    for build_name, config in self.all_configs.iteritems():
      # For builders that have explicit classes, this check doesn't make sense.
      if config['builder_class_name']:
        continue
      self.assertIn(config['build_type'], constants.VALID_BUILD_TYPES,
                    'Config %s: has unexpected build_type value.' % build_name)

  def testGCCGitHash(self):
    """Verifies that gcc_githash is not set without setting latest_toolchain."""
    for build_name, config in self.all_configs.iteritems():
      if config['gcc_githash']:
        self.assertTrue(
            config['latest_toolchain'],
            'Config %s: has gcc_githash but not latest_toolchain.' % build_name)

  def testBuildToRun(self):
    """Verify we don't try to run tests without building them."""
    for build_name, config in self.all_configs.iteritems():
      self.assertFalse(
          isinstance(config['useflags'], list) and
          '-build_tests' in config['useflags'] and config['vm_tests'],
          'Config %s: has vm_tests and use -build_tests.' % build_name)

  def testSyncToChromeSdk(self):
    """Verify none of the configs build chrome sdk but don't sync chrome."""
    for build_name, config in self.all_configs.iteritems():
      if config['sync_chrome'] is not None and not config['sync_chrome']:
        self.assertFalse(
            config['chrome_sdk'],
            'Config %s: has chrome_sdk but not sync_chrome.' % build_name)

  def testOverrideVmTestsOnly(self):
    """VM/unit tests listed should also be supported."""
    for build_name, config in self.all_configs.iteritems():
      if config.vm_tests_override is not None:
        for test in config.vm_tests:
          self.assertIn(
              test, config.vm_tests_override,
              'Config %s: has %s VM test, not in override (%s, %s).' % \
              (build_name, test, config.vm_tests, config.vm_tests_override))

  def testHWTestsIFFArchivingHWTestArtifacts(self):
    """Make sure all configs upload artifacts that need them for hw testing."""
    for build_name, config in self.all_configs.iteritems():
      if config['hw_tests']:
        self.assertTrue(
            config['upload_hw_test_artifacts'],
            "%s is trying to run hw tests without uploading payloads." %
            build_name)

  def testValidUnifiedMasterConfig(self):
    """Make sure any unified master configurations are valid."""
    for build_name, config in self.all_configs.iteritems():
      error = 'Unified config for %s has invalid values' % build_name
      # Unified masters must be internal and must rev both overlays.
      if config['master']:
        self.assertTrue(
            config['internal'] and config['manifest_version'], error)
      elif not config['master'] and config['manifest_version']:
        # Unified slaves can rev either public or both depending on whether
        # they are internal or not.
        if not config['internal']:
          self.assertEqual(config['overlays'], constants.PUBLIC_OVERLAYS, error)
        elif config_lib.IsCQType(config['build_type']):
          self.assertEqual(config['overlays'], constants.BOTH_OVERLAYS, error)

  def testGetSlaves(self):
    """Make sure every master has a sane list of slaves"""
    for build_name, config in self.all_configs.iteritems():
      if config.master:
        configs = self.all_configs.GetSlavesForMaster(config)
        self.assertEqual(
            len(map(repr, configs)), len(set(map(repr, configs))),
            'Duplicate board in slaves of %s will cause upload prebuilts'
            ' failures' % build_name)

        # Our logic for calculating what slaves have completed their critical
        # stages will break if the master is considered a slave of itself,
        # because db.GetSlaveStages(...) doesn't include master stages.
        if config.build_type == constants.PALADIN_TYPE:
          self.assertEquals(
              config.boards, [],
              'Master paladin %s cannot have boards.' % build_name)
          self.assertNotIn(
              build_name, [x.name for x in configs],
              'Master paladin %s cannot be a slave of itself.' % build_name)

  def testGetSlavesOnTrybot(self):
    """Make sure every master has a sane list of slaves"""
    mock_options = mock.Mock()
    mock_options.remote_trybot = True
    for _, config in self.all_configs.iteritems():
      if config['master']:
        configs = self.all_configs.GetSlavesForMaster(config, mock_options)
        self.assertEqual([], configs)

  def testFactoryFirmwareValidity(self):
    """Ensures that firmware/factory branches have at least 1 valid name."""
    tracking_branch = git.GetChromiteTrackingBranch()
    for branch in ['firmware', 'factory']:
      if tracking_branch.startswith(branch):
        saw_config_for_branch = False
        for build_name in self.all_configs:
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

    for build_name, config in self.all_configs.iteritems():
      if not config['build_tests']:
        for flag in ('factory_toolkit', 'vm_tests', 'hw_tests'):
          self.assertFalse(
              config[flag],
              'Config %s set %s without build_tests.' % (build_name, flag))

  def testAFDOInBackground(self):
    """Verify that we don't try to build or use AFDO data in the background."""
    for build_name, config in self.all_configs.iteritems():
      if config.build_packages_in_background:
        # It is unsupported to use the build_packages_in_background flags with
        # the afdo_generate or afdo_generate_min config options.
        msg = 'Config %s uses build_packages_in_background with afdo_%s'
        self.assertFalse(config.afdo_generate, msg % (build_name, 'generate'))
        self.assertFalse(config.afdo_generate_min, msg % (build_name,
                                                          'generate_min'))

  def testReleaseGroupInBackground(self):
    """Verify build_packages_in_background settings for release groups.

    For each release group, the first builder should be set to run in the
    foreground (to build binary packages), and the remainder of the builders
    should be set to run in parallel (to install the binary packages.)
    """
    for build_name, config in self.all_configs.iteritems():
      if build_name.endswith('-release-group'):
        msg = 'Config %s should not build_packages_in_background'
        self.assertFalse(config.build_packages_in_background, msg % build_name)

        self.assertTrue(
            config.child_configs,
            'Config %s should have child configs' % build_name)
        first_config = config.child_configs[0]
        msg = 'Primary config for %s should not build_packages_in_background'
        self.assertFalse(first_config.build_packages_in_background,
                         msg % build_name)

        msg = 'Child config %s for %s should build_packages_in_background'
        for child_config in config.child_configs[1:]:
          self.assertTrue(child_config.build_packages_in_background,
                          msg % (child_config.name, build_name))

  def testAFDOSameInChildConfigs(self):
    """Verify that 'afdo_use' is the same for all children in a group."""
    msg = ('Child config %s for %s should have same value for afdo_use '
           'as other children')
    for build_name, config in self.all_configs.iteritems():
      if build_name.endswith('-group'):
        prev_value = None
        self.assertTrue(config.child_configs,
                        'Config %s should have child configs' % build_name)
        for child_config in config.child_configs:
          if prev_value is None:
            prev_value = child_config.afdo_use
          else:
            self.assertEqual(child_config.afdo_use, prev_value,
                             msg % (child_config.name, build_name))

  def testReleaseAFDOConfigs(self):
    """Verify that <board>-release-afdo config have generate and use children.

    These configs should have a 'generate' and a 'use' child config. Also,
    any 'generate' and 'use' configs should be children of a release-afdo
    config.
    """
    msg = 'Config %s should have %s as a parent'
    parent_suffix = config_lib.CONFIG_TYPE_RELEASE_AFDO
    generate_suffix = '%s-generate' % parent_suffix
    use_suffix = '%s-use' % parent_suffix
    for build_name, config in self.all_configs.iteritems():
      if build_name.endswith(parent_suffix):
        self.assertEqual(
            len(config.child_configs), 2,
            'Config %s should have 2 child configs' % build_name)
        for child_config in config.child_configs:
          child_name = child_config.name
          self.assertTrue(child_name.endswith(generate_suffix) or
                          child_name.endswith(use_suffix),
                          'Config %s has wrong %s child' %
                          (build_name, child_config))
      if build_name.endswith(generate_suffix):
        parent_config_name = build_name.replace(generate_suffix,
                                                parent_suffix)
        self.assertTrue(parent_config_name in self.all_configs,
                        msg % (build_name, parent_config_name))
      if build_name.endswith(use_suffix):
        parent_config_name = build_name.replace(use_suffix,
                                                parent_suffix)
        self.assertTrue(parent_config_name in self.all_configs,
                        msg % (build_name, parent_config_name))

  def testNoGrandChildConfigs(self):
    """Verify that no child configs have a child config."""
    for build_name, config in self.all_configs.iteritems():
      for child_config in config.child_configs:
        for grandchild_config in child_config.child_configs:
          self.fail('Config %s has grandchild %s' % (build_name,
                                                     grandchild_config.name))

  def testUseChromeLKGMImpliesInternal(self):
    """Currently use_chrome_lkgm refers only to internal manifests."""
    for build_name, config in self.all_configs.iteritems():
      if config['use_chrome_lkgm']:
        self.assertTrue(
            config['internal'],
            'Chrome lkgm currently only works with an internal manifest: %s' % (
                build_name,))

  def _HasValidSuffix(self, config_name, config_types):
    """Given a config_name, see if it has a suffix in config_types.

    Args:
      config_name: Name of config to compare.
      config_types: A tuple/list of config suffixes.

    Returns:
      True, if the config has a suffix matching one of the types.
    """
    for config_type in config_types:
      if config_name.endswith('-' + config_type) or config_name == config_type:
        return True

    return False

  def testNonOverlappingConfigTypes(self):
    """Test that a config can only match one build suffix."""
    # This test belongs in config_lib_unittest, except nobody else cares.
    for config_type in config_lib.CONFIG_TYPE_DUMP_ORDER:
      trimmed_configs = list(config_lib.CONFIG_TYPE_DUMP_ORDER)
      trimmed_configs.remove(config_type)
      self.assertFalse(self._HasValidSuffix(config_type, trimmed_configs))

  def testConfigTypesComplete(self):
    """Verify CONFIG_TYPE_DUMP_ORDER contains all valid config types."""
    for config_name in self.all_configs:
      self.assertTrue(
          self._HasValidSuffix(config_name, config_lib.CONFIG_TYPE_DUMP_ORDER),
          '%s did not match any types in %s' %
          (config_name, 'config_lib.CONFIG_TYPE_DUMP_ORDER'))

  def testCantBeBothTypesOfLKGM(self):
    """Using lkgm and chrome_lkgm doesn't make sense."""
    for config in self.all_configs.values():
      self.assertFalse(config['use_lkgm'] and config['use_chrome_lkgm'])

  def testNoDuplicateSlavePrebuilts(self):
    """Test that no two same-board paladin slaves upload prebuilts."""
    for cfg in self.all_configs.values():
      if cfg['build_type'] == constants.PALADIN_TYPE and cfg['master']:
        slaves = self.all_configs.GetSlavesForMaster(cfg)
        prebuilt_slaves = [s for s in slaves if s['prebuilts']]
        # Dictionary from board name to builder name that uploads prebuilt
        prebuilt_slave_boards = {}
        for slave in prebuilt_slaves:
          for board in slave['boards']:
            self.assertFalse(prebuilt_slave_boards.has_key(board),
                             'Configs %s and %s both upload prebuilts for '
                             'board %s.' % (prebuilt_slave_boards.get(board),
                                            slave['name'],
                                            board))
            prebuilt_slave_boards[board] = slave['name']

  def testNoDuplicateWaterfallNames(self):
    """Tests that no two configs specify same waterfall name."""
    waterfall_names = set()
    for config in self.all_configs.values():
      wn = config['buildbot_waterfall_name']
      if wn is not None:
        self.assertNotIn(wn, waterfall_names,
                         'Duplicate waterfall name %s.' % wn)
        waterfall_names.add(wn)

  def testCantBeBothTypesOfAFDO(self):
    """Using afdo_generate and afdo_use together doesn't work."""
    for config in self.all_configs.values():
      self.assertFalse(config['afdo_use'] and config['afdo_generate'])
      self.assertFalse(config['afdo_use'] and config['afdo_generate_min'])
      self.assertFalse(config['afdo_generate'] and config['afdo_generate_min'])

  def testValidPrebuilts(self):
    """Verify all builders have valid prebuilt values."""
    for build_name, config in self.all_configs.iteritems():
      msg = 'Config %s: has unexpected prebuilts value.' % build_name
      valid_values = (False, constants.PRIVATE, constants.PUBLIC)
      self.assertTrue(config['prebuilts'] in valid_values, msg)

  def testInternalPrebuilts(self):
    for build_name, config in self.all_configs.iteritems():
      if (config['internal'] and
          config['build_type'] != constants.CHROME_PFQ_TYPE):
        msg = 'Config %s is internal but has public prebuilts.' % build_name
        self.assertNotEqual(config['prebuilts'], constants.PUBLIC, msg)

  def testValidHWTestPriority(self):
    """Verify that hw test priority is valid."""
    for build_name, config in self.all_configs.iteritems():
      for test_config in config['hw_tests']:
        self.assertTrue(
            test_config.priority in constants.HWTEST_VALID_PRIORITIES,
            '%s has an invalid hwtest priority.' % build_name)

  def testAllBoardsExist(self):
    """Verifies that all config boards are in _all_boards."""
    for build_name, config in self.all_configs.iteritems():
      for board in config['boards']:
        # pylint: disable=protected-access
        self.assertIn(board, chromeos_config._all_boards,
                      'Config %s has unknown board %s.' %
                      (build_name, board))

  def testPushImagePaygenDependancies(self):
    """Paygen requires PushImage."""
    for build_name, config in self.all_configs.iteritems():

      # paygen can't complete without push_image, except for payloads
      # where --channel arguments meet the requirements.
      if config['paygen']:
        self.assertTrue(config['push_image'] or
                        config['build_type'] == constants.PAYLOADS_TYPE,
                        '%s has paygen without push_image' % build_name)

  def testPaygenTestDependancies(self):
    """paygen testing requires upload_hw_test_artifacts."""
    for build_name, config in self.all_configs.iteritems():

      # This requirement doesn't apply to payloads builds. Payloads are
      # using artifacts from a previous build.
      if build_name.endswith('-payloads'):
        continue

      if config['paygen'] and not config['paygen_skip_testing']:
        self.assertTrue(config['upload_hw_test_artifacts'],
                        '%s is not upload_hw_test_artifacts, but also not'
                        ' paygen_skip_testing' % build_name)

  def testPayloadImageIsBuilt(self):
    for build_name, config in self.all_configs.iteritems():
      if config.payload_image is not None:
        self.assertNotEqual('recovery', config.payload_image,
                            '%s wants to generate payloads from recovery '
                            'images, which is not allowed.' % build_name)
        self.assertIn(config.payload_image, config.images,
                      '%s builds payloads from %s, which is not in images '
                      'list %s' % (build_name, config.payload_image,
                                   config.images))

  def testBuildPackagesForRecoveryImage(self):
    """Tests that we build the packages required for recovery image."""
    for build_name, config in self.all_configs.iteritems():
      if 'recovery' in config.images:
        if not config.packages:
          # No packages are specified. Defaults to build all packages.
          continue

        self.assertIn('chromeos-base/chromeos-initramfs',
                      config.packages,
                      '%s does not build chromeos-initramfs, which is required '
                      'for creating the recovery image' % build_name)

  def testBuildRecoveryImageFlags(self):
    """Ensure the right flags are disabled when building the recovery image."""
    incompatible_flags = ['paygen', 'signer_tests']
    for build_name, config in self.all_configs.iteritems():
      for flag in incompatible_flags:
        if config[flag] and config.build_type != constants.PAYLOADS_TYPE:
          self.assertIn('recovery', config.images,
                        '%s does not build the recovery image, which is '
                        'incompatible with %s=True' % (build_name, flag))

  def testBuildBaseImageForRecoveryImage(self):
    """Tests that we build the packages required for recovery image."""
    for build_name, config in self.all_configs.iteritems():
      if 'recovery' in config.images:
        self.assertIn('base', config.images,
                      '%s does not build the base image, which is required for '
                      'building the recovery image' % build_name)

  def testChildConfigsNotImportantInReleaseGroup(self):
    """Verify that configs in an important group are not important."""
    msg = ('Child config %s for %s should not be important because %s is '
           'already important')
    for build_name, config in self.all_configs.iteritems():
      if build_name.endswith('-release-group') and config['important']:
        for child_config in config.child_configs:
          self.assertFalse(child_config.important,
                           msg % (child_config.name, build_name, build_name))

  def testFullCQBuilderDoNotRunHWTest(self):
    """Full CQ configs should not run HWTest."""
    msg = ('%s should not be a full builder and run HWTest for '
           'performance reasons')
    for build_name, config in self.all_configs.iteritems():
      if config.build_type == constants.PALADIN_TYPE:
        self.assertFalse(config.chrome_binhost_only and config.hw_tests,
                         msg % build_name)

  def testExternalConfigsDoNotUseInternalFeatures(self):
    """External configs should not use chrome_internal, or official.xml."""
    msg = ('%s is not internal, so should not use chrome_internal, or an '
           'internal manifest')
    for build_name, config in self.all_configs.iteritems():
      if not config['internal']:
        self.assertFalse('chrome_internal' in config['useflags'],
                         msg % build_name)
        self.assertNotEqual(config.get('manifest'),
                            constants.OFFICIAL_MANIFEST,
                            msg % build_name)

  def testNoShadowedUseflags(self):
    """Configs should not have both useflags x and -x."""
    msg = ('%s contains useflag %s and -%s.')
    for build_name, config in self.all_configs.iteritems():
      useflag_set = set(config['useflags'])
      for flag in useflag_set:
        if not flag.startswith('-'):
          self.assertFalse('-' + flag in useflag_set,
                           msg % (build_name, flag, flag))

  def testHealthCheckEmails(self):
    """Configs should only have valid email addresses or aliases"""
    msg = ('%s contains an invalid tree alias or email address: %s')
    for build_name, config in self.all_configs.iteritems():
      health_alert_recipients = config['health_alert_recipients']
      for recipient in health_alert_recipients:
        self.assertTrue(re.match(r'[^@]+@[^@]+\.[^@]+', recipient) or
                        recipient in constants.SHERIFF_TYPE_TO_URL.keys(),
                        msg % (build_name, recipient))

  def testCheckBuilderClass(self):
    """Verify builder_class_name is a valid value."""
    for build_name, config in self.all_configs.iteritems():
      builder_class_name = config['builder_class_name']
      if builder_class_name is None:
        continue

      cls = builders.GetBuilderClass(builder_class_name)
      self.assertTrue(issubclass(cls, generic_builders.Builder),
                      msg=('config %s has a broken builder_class_name' %
                           build_name))

  def testDistinctBoardSets(self):
    """Verify that distinct board sets are distinct."""
    # Every board should be in exactly one of the distinct board sets.
    for board in chromeos_config._all_boards:
      found = False
      for s in chromeos_config._distinct_board_sets:
        if board in s:
          if found:
            assert False, '%s in multiple board sets.' % board
          else:
            found = True
      if not found:
        assert False, '%s in no board sets' % board
    for s in chromeos_config._distinct_board_sets:
      for board in s - chromeos_config._all_boards:
        assert False, ('%s in _distinct_board_sets but not in _all_boards' %
                       board)


class OverrideForTrybotTest(GenerateChromeosConfigTestBase):
  """Test config override functionality."""

  def _testWithOptions(self, **kwargs):
    mock_options = mock.Mock()
    for k, v in kwargs.iteritems():
      mock_options.setattr(k, v)

    for config in self.all_configs.itervalues():
      chromeos_config.OverrideConfigForTrybot(config, mock_options)

  def testLocalTrybot(self):
    """Override each config for local trybot."""
    self._testWithOptions(remote_trybot=False, hw_test=False)

  def testRemoteTrybot(self):
    """Override each config for remote trybot."""
    self._testWithOptions(remote_trybot=True, hw_test=False)

  def testRemoteHWTest(self):
    """Override each config for remote trybot + hwtests."""
    self._testWithOptions(remote_trybot=True, hw_test=True)

  def testVmTestOverride(self):
    """Verify that vm_tests override for trybots pay heed to original config."""
    mock_options = mock.Mock()
    old = self.all_configs['x86-mario-paladin']
    new = chromeos_config.OverrideConfigForTrybot(old, mock_options)
    self.assertEquals(new['vm_tests'], [constants.SMOKE_SUITE_TEST_TYPE,
                                        constants.SIMPLE_AU_TEST_TYPE,
                                        constants.CROS_VM_TEST_TYPE])

    # Don't override vm tests for arm boards.
    old = self.all_configs['daisy-paladin']
    new = chromeos_config.OverrideConfigForTrybot(old, mock_options)
    self.assertEquals(new['vm_tests'], old['vm_tests'])

    # Don't override vm tests for brillo boards.
    old = self.all_configs['storm-paladin']
    new = chromeos_config.OverrideConfigForTrybot(old, mock_options)
    self.assertEquals(new['vm_tests'], old['vm_tests'])

  # pylint: disable=protected-access
  def testWaterfallManualConfigIsValid(self):
    """Verify the correctness of the manual waterfall configuration."""
    all_build_names = set(self.all_configs.iterkeys())
    redundant = set()
    seen = set()
    waterfall_iter = chromeos_config._waterfall_config_map.iteritems()
    for waterfall, names in waterfall_iter:
      for build_name in names:
        # Every build in the configuration map must be valid.
        self.assertTrue(build_name in all_build_names,
                        "Invalid build name in manual waterfall config: %s" % (
                            build_name,))
        # No build should appear in multiple waterfalls.
        self.assertFalse(build_name in seen,
                         "Duplicate manual config for board: %s" % (
                             build_name,))
        seen.add(build_name)

        # The manual configuration must be applied and override any default
        # configuration.
        config = self.all_configs[build_name]
        self.assertEqual(config['active_waterfall'], waterfall,
                         "Manual waterfall membership is not in the "
                         "configuration for: %s" % (build_name,))


        default_waterfall = chromeos_config.GetDefaultWaterfall(config)
        if config['active_waterfall'] == default_waterfall:
          redundant.add(build_name)

    # No configurations should be redundant with defaults.
    self.assertFalse(redundant,
                     "Manual waterfall configs are automatically included: "
                     "%s" % (sorted(redundant),))

  def testNoDuplicateCanaryBuildersOnWaterfall(self):
    seen = {}
    for config in self.all_configs.itervalues():
      waterfall = config['active_waterfall']
      btype = config['build_type']
      if not (waterfall and config_lib.IsCanaryType(btype)):
        continue

      waterfall_seen = seen.setdefault(waterfall, set())
      stack = [config]
      while stack:
        current_config = stack.pop()
        self.assertNotIn(current_config['name'], waterfall_seen,
                         "Multiple builders for '%s' on '%s' waterfall" % (
                             current_config['name'], waterfall))
        waterfall_seen.add(current_config['name'])
        stack += current_config['child_configs']

  def testBinhostTest(self):
    """Builders with the binhost_test setting shouldn't have boards."""
    for config in self.all_configs.values():
      if config.binhost_test:
        self.assertEqual(config.boards, [])


class TemplateTest(GenerateChromeosConfigTestBase):
  """Tests for templates."""

  def testTemplatesUsed(self):
    """Test that all templates are used."""
    templates_used = set(cfg['_template'] for cfg in self.all_configs.values())
    templates = set([None] + self.all_configs.GetTemplates().keys())
    self.assertEqual(templates, templates_used)

  def testConfigNamesMatchTemplate(self):
    """Test that all configs have names that match their templates."""
    for name, config in self.all_configs.iteritems():
      template = config._template
      if template:
        child_configs = config.child_configs
        if not child_configs:
          msg = '%s should end with %s to match its template'
          self.assertTrue(name.endswith(template), msg % (name, template))
        else:
          msg = 'Child config of %s has name that does not match its template'
          self.assertTrue(child_configs[0].name.endswith(template),
                          msg % name)

      for other in self.all_configs.GetTemplates():
        if name.endswith(other) and other != template:
          if template:
            msg = '%s has more specific template: %s' % (name, other)
            self.assertGreater(len(template), len(other), msg)
          else:
            msg = '%s should have %s as template' % (name, other)
            self.assertFalse(name, msg)
