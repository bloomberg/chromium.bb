# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for config."""

from __future__ import print_function

import copy
import cPickle
import json

from chromite.cbuildbot import cbuildbot_config
from chromite.cbuildbot import config_lib
from chromite.lib import cros_test_lib


def MockBuildConfig():
  """Create a BuildConfig object for convenient testing pleasure."""
  site_config = MockSiteConfig()
  return site_config['x86-generic-paladin']


def MockSiteConfig():
  """Create a SiteConfig object for convenient testing pleasure."""
  return config_lib.CreateConfigFromString(
      '''
      {
        "_default": {
          "active_waterfall": null,
          "afdo_generate": false,
          "afdo_generate_min": false,
          "afdo_update_ebuild": false,
          "afdo_use": false,
          "archive": true,
          "archive_build_debug": false,
          "binhost_base_url": null,
          "binhost_bucket": null,
          "binhost_key": null,
          "binhost_test": false,
          "board_replace": false,
          "boards": null,
          "branch": false,
          "build_before_patching": false,
          "build_packages_in_background": false,
          "build_tests": true,
          "build_type": "pfq",
          "buildbot_waterfall_name": null,
          "builder_class_name": null,
          "child_configs": [],
          "chrome_binhost_only": false,
          "chrome_rev": null,
          "chrome_sdk": false,
          "chrome_sdk_build_chrome": true,
          "chrome_sdk_goma": false,
          "chromeos_official": false,
          "chroot_replace": false,
          "compilecheck": false,
          "cpe_export": true,
          "create_delta_sysroot": false,
          "critical_for_chrome": false,
          "debug_symbols": true,
          "description": null,
          "dev_installer_prebuilts": false,
          "dev_manifest": "default.xml",
          "disk_layout": null,
          "do_not_apply_cq_patches": false,
          "doc": null,
          "factory": true,
          "factory_install_netboot": true,
          "factory_toolkit": true,
          "gcc_githash": null,
          "git_sync": false,
          "grouped": false,
          "gs_path": "default",
          "health_alert_recipients": [],
          "health_threshold": 0,
          "hw_tests": [],
          "hwqual": false,
          "image_test": false,
          "images": [
              "test"
          ],
          "important": false,
          "internal": false,
          "latest_toolchain": false,
          "lkgm_manifest": "LKGM/lkgm.xml",
          "manifest": "default.xml",
          "manifest_repo_url": "mock_manifest_repo_url",
          "manifest_version": false,
          "master": false,
          "name": null,
          "overlays": "public",
          "packages": [],
          "paygen": false,
          "paygen_skip_delta_payloads": false,
          "paygen_skip_testing": false,
          "payload_image": null,
          "postsync_patch": true,
          "postsync_reexec": true,
          "pre_cq": false,
          "prebuilts": false,
          "profile": null,
          "push_image": false,
          "push_overlays": null,
          "quick_unit": false,
          "rootfs_verification": true,
          "sanity_check_slaves": null,
          "separate_debug_symbols": true,
          "shared_user_password": null,
          "signer_tests": false,
          "sync_chrome": null,
          "tests_supported": true,
          "trybot_list": false,
          "unittest_blacklist": [],
          "unittests": true,
          "upload_hw_test_artifacts": true,
          "upload_standalone_images": true,
          "upload_symbols": false,
          "uprev": true,
          "use_chrome_lkgm": false,
          "use_lkgm": false,
          "use_sdk": true,
          "useflags": [],
          "usepkg_build_packages": true,
          "usepkg_toolchain": true,
          "vm_test_runs": 1,
          "vm_tests": [
              "smoke_suite",
              "pfq_suite"
          ]
        },
        "x86-generic-paladin": {
            "active_waterfall": "chromiumos",
            "boards": [
                "x86-generic"
            ],
            "build_type": "paladin",
            "chrome_sdk": true,
            "chrome_sdk_build_chrome": false,
            "description": "Commit Queue",
            "doc": "http://mock_url/",
            "image_test": true,
            "images": [
                "base",
                "test"
            ],
            "important": true,
            "manifest_version": true,
            "name": "x86-generic-paladin",
            "prebuilts": "public",
            "trybot_list": true,
            "upload_standalone_images": false,
            "vm_tests": [
                "smoke_suite"
            ]
        }
      }
      ''')


class _CustomObject(object):
  """Simple object. For testing deepcopy."""

  def __init__(self, x):
    self.x = x

  def __eq__(self, other):
    return self.x == other.x


class _CustomObjectWithSlots(object):
  """Simple object with slots. For testing deepcopy."""

  __slots__ = ['x']

  def __init__(self, x):
    self.x = x

  def __eq__(self, other):
    return self.x == other.x


class BuildConfigClassTest(cros_test_lib.TestCase):
  """BuildConfig tests."""

  def setUp(self):
    self.fooConfig = config_lib.BuildConfig(name='foo', value=1)
    self.barConfig = config_lib.BuildConfig(name='bar', value=2)
    self.deepConfig = config_lib.BuildConfig(
        name='deep', nested=[1, 2, 3], value=3,
        child_configs=[self.fooConfig, self.barConfig])

    self.config = {
        'foo': self.fooConfig,
        'bar': self.barConfig,
        'deep': self.deepConfig,
    }


  def testMockSiteConfig(self):
    """Make sure Mock generator fucntion doesn't crash."""
    site_config = MockSiteConfig()
    self.assertIsNotNone(site_config)

    build_config = MockBuildConfig()
    self.assertIsNotNone(build_config)

  def testValueAccess(self):
    self.assertEqual(self.fooConfig.name, 'foo')
    self.assertEqual(self.fooConfig.name, self.fooConfig['name'])

    self.assertRaises(AttributeError, getattr, self.fooConfig, 'foobar')

  # pylint: disable=protected-access
  def testDeleteKey(self):
    base_config = config_lib.BuildConfig(foo='bar')
    inherited_config = base_config.derive(
        foo=config_lib.BuildConfig.delete_key())
    self.assertTrue('foo' in base_config)
    self.assertFalse('foo' in inherited_config)

  def testDeleteKeys(self):
    base_config = config_lib.BuildConfig(foo='bar', baz='bak')
    inherited_config_1 = base_config.derive(qzr='flp')
    inherited_config_2 = inherited_config_1.derive(
        config_lib.BuildConfig.delete_keys(base_config))
    self.assertEqual(inherited_config_2, {'qzr': 'flp'})

  def testCallableOverrides(self):
    append_foo = lambda x: x + 'foo' if x else 'foo'
    base_config = config_lib.BuildConfig()
    inherited_config_1 = base_config.derive(foo=append_foo)
    inherited_config_2 = inherited_config_1.derive(foo=append_foo)
    self.assertEqual(inherited_config_1, {'foo': 'foo'})
    self.assertEqual(inherited_config_2, {'foo': 'foofoo'})

  def AssertDeepCopy(self, obj1, obj2, obj3):
    """Assert that |obj3| is a deep copy of |obj1|.

    Args:
      obj1: Object that was copied.
      obj2: A true deep copy of obj1 (produced using copy.deepcopy).
      obj3: The purported deep copy of obj1.
    """
    # Check whether the item was copied by deepcopy. If so, then it
    # must have been copied by our algorithm as well.
    if obj1 is not obj2:
      self.assertIsNot(obj1, obj3)

    # Assert the three items are all equal.
    self.assertEqual(obj1, obj2)
    self.assertEqual(obj1, obj3)

    if isinstance(obj1, (tuple, list)):
      # Copy tuples and lists item by item.
      for i in range(len(obj1)):
        self.AssertDeepCopy(obj1[i], obj2[i], obj3[i])
    elif isinstance(obj1, set):
      # Compare sorted versions of the set.
      self.AssertDeepCopy(list(sorted(obj1)), list(sorted(obj2)),
                          list(sorted(obj3)))
    elif isinstance(obj1, dict):
      # Copy dicts item by item.
      for k in obj1:
        self.AssertDeepCopy(obj1[k], obj2[k], obj3[k])
    elif hasattr(obj1, '__dict__'):
      # Make sure the dicts are copied.
      self.AssertDeepCopy(obj1.__dict__, obj2.__dict__, obj3.__dict__)
    elif hasattr(obj1, '__slots__'):
      # Make sure the slots are copied.
      for attr in obj1.__slots__:
        self.AssertDeepCopy(getattr(obj1, attr), getattr(obj2, attr),
                            getattr(obj3, attr))
    else:
      # This should be an object that copy.deepcopy didn't copy (probably an
      # immutable object.) If not, the test needs to be updated to handle this
      # kind of object.
      self.assertIs(obj1, obj2)

  def testDeepCopy(self):
    """Test that we deep copy correctly."""
    for cfg in [self.fooConfig, self.barConfig, self.deepConfig]:
      self.AssertDeepCopy(cfg, copy.deepcopy(cfg), cfg.deepcopy())

  def testAssertDeepCopy(self):
    """Test that we test deep copy correctly."""
    test1 = ['foo', 'bar', ['hey']]
    tests = [test1,
             set([tuple(x) for x in test1]),
             dict(zip([tuple(x) for x in test1], test1)),
             _CustomObject(test1),
             _CustomObjectWithSlots(test1)]

    for x in tests + [[tests]]:
      copy_x = copy.deepcopy(x)
      self.AssertDeepCopy(x, copy_x, copy.deepcopy(x))
      self.AssertDeepCopy(x, copy_x, cPickle.loads(cPickle.dumps(x, -1)))
      self.assertRaises(AssertionError, self.AssertDeepCopy, x,
                        copy_x, x)
      if not isinstance(x, set):
        self.assertRaises(AssertionError, self.AssertDeepCopy, x,
                          copy_x, copy.copy(x))

class ConfigClassTest(cros_test_lib.TestCase):
  """Config tests."""

  def testSaveLoadEmpty(self):
    config = config_lib.SiteConfig()

    config_str = config.SaveConfigToString()
    self.assertEqual(config_str, """{
    "_default": {}
}""")

    loaded = config_lib.CreateConfigFromString(config_str)
    loaded_str = loaded.SaveConfigToString()

    self.assertEqual(config, loaded)
    self.assertEqual(config_str, loaded_str)

  def testSaveLoadComplex(self):

    # pylint: disable=line-too-long
    src_str = """{
    "_default": {
        "bar": true,
        "baz": false,
        "child_configs": [],
        "foo": false,
        "hw_tests": []
    },
    "diff_build": {
        "bar": false,
        "foo": true,
        "name": "diff_build"
    },
    "match_build": {
        "name": "match_build"
    },
    "parent_build": {
        "child_configs": [
            {
                "name": "empty_build"
            },
            {
                "bar": false,
                "name": "child_build",
                "hw_tests": [
                    "{\\n    \\"async\\": true,\\n    \\"blocking\\": false,\\n    \\"critical\\": false,\\n    \\"file_bugs\\": true,\\n    \\"max_retries\\": null,\\n    \\"minimum_duts\\": 4,\\n    \\"num\\": 2,\\n    \\"pool\\": \\"bvt\\",\\n    \\"priority\\": \\"PostBuild\\",\\n    \\"retry\\": false,\\n    \\"suite\\": \\"bvt-perbuild\\",\\n    \\"suite_min_duts\\": 1,\\n    \\"timeout\\": 13200,\\n    \\"warn_only\\": false\\n}"
                ]
            }
        ],
        "name": "parent_build"
    }
}"""

    config = config_lib.CreateConfigFromString(src_str)
    config_str = config.SaveConfigToString()

    # Verify that the dumped object matches the source object.
    self.assertEqual(json.loads(src_str), json.loads(config_str))

    # Verify assorted stuff in the loaded config to make sure it matches
    # expectations.
    self.assertFalse(config['match_build'].foo)
    self.assertTrue(config['match_build'].bar)
    self.assertTrue(config['diff_build'].foo)
    self.assertFalse(config['diff_build'].bar)
    self.assertTrue(config['parent_build'].bar)
    self.assertTrue(config['parent_build'].child_configs[0].bar)
    self.assertFalse(config['parent_build'].child_configs[1].bar)
    self.assertEqual(
        config['parent_build'].child_configs[1].hw_tests[0],
        config_lib.HWTestConfig(
            suite='bvt-perbuild',
            async=True, file_bugs=True, max_retries=None,
            minimum_duts=4, num=2, priority='PostBuild',
            retry=False, suite_min_duts=1))

    # Load an save again, just to make sure there are no changes.
    loaded = config_lib.CreateConfigFromString(config_str)
    loaded_str = loaded.SaveConfigToString()

    self.assertEqual(config, loaded)
    self.assertEqual(config_str, loaded_str)


class FindConfigsForBoardTest(cros_test_lib.TestCase):
  """Test locating of official build for a board."""

  def setUp(self):
    self.config = cbuildbot_config.GetConfig()

  def _CheckFullConfig(
      self, board, external_expected=None, internal_expected=None):
    """Check FindFullConfigsForBoard has expected results.

    Args:
      board: Argument to pass to FindFullConfigsForBoard.
      external_expected: Expected config name (singular) to be found.
      internal_expected: Expected config name (singular) to be found.
    """

    def check_expected(l, expected):
      if expected is not None:
        self.assertTrue(expected in [v['name'] for v in l])

    external, internal = self.config.FindFullConfigsForBoard(board)
    self.assertFalse(external_expected is None and internal_expected is None)
    check_expected(external, external_expected)
    check_expected(internal, internal_expected)

  def _CheckCanonicalConfig(self, board, ending):
    self.assertEquals(
        '-'.join((board, ending)),
        self.config.FindCanonicalConfigForBoard(board)['name'])

  def testExternal(self):
    """Test finding of a full builder."""
    self._CheckFullConfig(
        'amd64-generic', external_expected='amd64-generic-full')

  def testInternal(self):
    """Test finding of a release builder."""
    self._CheckFullConfig('lumpy', internal_expected='lumpy-release')

  def testBoth(self):
    """Both an external and internal config exist for board."""
    self._CheckFullConfig(
        'daisy', external_expected='daisy-full',
        internal_expected='daisy-release')

  def testExternalCanonicalResolution(self):
    """Test an external canonical config."""
    self._CheckCanonicalConfig('x86-generic', 'full')

  def testInternalCanonicalResolution(self):
    """Test prefer internal over external when both exist."""
    self._CheckCanonicalConfig('daisy', 'release')

  def testAFDOCanonicalResolution(self):
    """Test prefer non-AFDO over AFDO builder."""
    self._CheckCanonicalConfig('lumpy', 'release')

  def testOneFullConfigPerBoard(self):
    """There is at most one 'full' config for a board."""
    # Verifies that there is one external 'full' and one internal 'release'
    # build per board.  This is to ensure that we fail any new configs that
    # wrongly have names like *-bla-release or *-bla-full. This case can also
    # be caught if the new suffix was added to
    # config_lib.CONFIG_TYPE_DUMP_ORDER
    # (see testNonOverlappingConfigTypes), but that's not guaranteed to happen.
    def AtMostOneConfig(board, label, configs):
      if len(configs) > 1:
        self.fail(
            'Found more than one %s config for %s: %r'
            % (label, board, [c['name'] for c in configs]))

    boards = set()
    for build_config in self.config.itervalues():
      boards.update(build_config['boards'])

    # Sanity check of the boards.
    self.assertTrue(boards)

    for b in boards:
      # TODO(akeshet): Figure out why we have both panther_embedded-minimal
      # release and panther_embedded-release, and eliminate one of them.
      if b == 'panther_embedded':
        continue
      external, internal = self.config.FindFullConfigsForBoard(b)
      AtMostOneConfig(b, 'external', external)
      AtMostOneConfig(b, 'internal', internal)
