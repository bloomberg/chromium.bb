# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for config."""

from __future__ import print_function

import copy
import cPickle
import mock

from chromite.cbuildbot import chromeos_config
from chromite.cbuildbot import config_lib
from chromite.lib import cros_test_lib

# pylint: disable=protected-access



def MockBuildConfig():
  """Create a BuildConfig object for convenient testing pleasure."""
  site_config = MockSiteConfig()
  return site_config['x86-generic-paladin']


def MockSiteConfig():
  """Create a SiteConfig object for convenient testing pleasure.

  Shared amoung a number of unittest files, so be careful if changing it.
  """
  result = config_lib.SiteConfig()

  # Add a single, simple build config.
  result.Add(
      'x86-generic-paladin',
      active_waterfall='chromiumos',
      boards=['x86-generic'],
      build_type='paladin',
      chrome_sdk=True,
      chrome_sdk_build_chrome=False,
      description='Commit Queue',
      doc='http://mock_url/',
      image_test=True,
      images=['base', 'test'],
      important=True,
      manifest_version=True,
      prebuilts='public',
      trybot_list=True,
      upload_standalone_images=False,
      vm_tests=[config_lib.VMTestConfig('smoke_suite')],
  )

  return result


def AssertSiteIndependentParameters(site_config):
  """Helper function to test that SiteConfigs contain site-independent values.

  Args:
    site_config: A SiteConfig object.

  Returns:
    A boolean. True if the config contained all site-independent values.
    False otherwise.
  """
  # Enumerate the necessary site independent parameter keys.
  # All keys must be documented.
  # TODO (msartori): Fill in this list.
  site_independent_params = [
  ]

  site_params = site_config.params
  return all([x in site_params for x in site_independent_params])


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

class SiteParametersClassTest(cros_test_lib.TestCase):
  """SiteParameters tests."""

  def testAttributeAccess(self):
    """Test that SiteParameters dot-accessor works correctly."""
    site_params = config_lib.SiteParameters()

    # Ensure our test key is not in site_params.
    self.assertTrue(site_params.get('foo') is None)

    # Test that we raise when accessing a non-existent value.
    # pylint: disable=pointless-statement
    with self.assertRaises(AttributeError):
      site_params.foo

    # Test the dot-accessor.
    site_params.update({'foo': 'bar'})
    self.assertEquals('bar', site_params.foo)


class SiteConfigClassTest(cros_test_lib.TestCase):
  """Config tests."""

  def testAdd(self):
    """Test the SiteConfig.Add behavior."""

    minimal_defaults = {
        'name': None, '_template': None, 'value': 'default',
    }

    site_config = config_lib.SiteConfig(defaults=minimal_defaults)
    template = site_config.AddTemplate('template', value='template')
    mixin = config_lib.BuildConfig(value='mixin')

    site_config.Add('default')

    site_config.Add('default_with_override',
                    value='override')

    site_config.Add('default_with_mixin',
                    mixin)

    site_config.Add('mixin_with_override',
                    mixin,
                    value='override')

    site_config.Add('default_with_template',
                    template)

    site_config.Add('template_with_override',
                    template,
                    value='override')


    site_config.Add('template_with_mixin',
                    template,
                    mixin)

    site_config.Add('template_with_mixin_override',
                    template,
                    mixin,
                    value='override')

    expected = {
        'default': {
            '_template': None,
            'name': 'default',
            'value': 'default',
        },
        'default_with_override': {
            '_template': None,
            'name': 'default_with_override',
            'value': 'override',
        },
        'default_with_mixin': {
            '_template': None,
            'name': 'default_with_mixin',
            'value': 'mixin',
        },
        'mixin_with_override': {
            '_template': None,
            'name': 'mixin_with_override',
            'value': 'override',
        },
        'default_with_template': {
            '_template': 'template',
            'name': 'default_with_template',
            'value': 'template',
        },
        'template_with_override': {
            '_template': 'template',
            'name': 'template_with_override',
            'value': 'override'
        },
        'template_with_mixin': {
            '_template': 'template',
            'name': 'template_with_mixin',
            'value': 'mixin',
        },
        'template_with_mixin_override': {
            '_template': 'template',
            'name': 'template_with_mixin_override',
            'value': 'override'
        },
    }

    self.maxDiff = None
    self.assertDictEqual(site_config, expected)

  def testAddErrors(self):
    """Test the SiteConfig.Add behavior."""
    site_config = MockSiteConfig()

    site_config.Add('foo')

    # TODO(kevcheng): Disabled test for now until I figure out where configs
    #                 are repeatedly added in chromeos_config_unittest.
    # Test we can't add the 'foo' config again.
    # with self.assertRaises(AssertionError):
    #   site_config.Add('foo')

    # Create a template without using AddTemplate so the site config doesn't
    # know about it.
    fake_template = config_lib.BuildConfig(
        name='fake_template', _template='fake_template')

    with self.assertRaises(AssertionError):
      site_config.Add('bar', fake_template)

  def testSaveLoadEmpty(self):
    config = config_lib.SiteConfig(defaults={}, site_params={})
    config_str = config.SaveConfigToString()
    loaded = config_lib.LoadConfigFromString(config_str)

    self.assertEqual(config, loaded)

    self.assertEqual(loaded.keys(), [])
    self.assertEqual(loaded._templates.keys(), [])
    self.assertEqual(loaded.GetDefault(), config_lib.DefaultSettings())
    self.assertEqual(loaded.params,
                     config_lib.SiteParameters(
                         config_lib.DefaultSiteParameters()))

    self.assertNotEqual(loaded.SaveConfigToString(), '')

    # Make sure we can dump debug content without crashing.
    self.assertNotEqual(loaded.DumpExpandedConfigToString(), '')

  def testSaveLoadComplex(self):

    # pylint: disable=line-too-long
    src_str = """{
    "_default": {
        "bar": true,
        "baz": false,
        "child_configs": [],
        "foo": false,
        "hw_tests": [],
        "nested": { "sub1": 1, "sub2": 2 }
    },
    "_site_params": {
        "site_foo": true,
        "site_bar": false
    },
    "_templates": {
       "build": {
            "baz": true
       }
    },
    "diff_build": {
        "_template": "build",
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
                    "{\\n    \\"async\\": true,\\n    \\"blocking\\": false,\\n    \\"critical\\": false,\\n    \\"file_bugs\\": true,\\n    \\"max_retries\\": null,\\n    \\"minimum_duts\\": 4,\\n    \\"num\\": 2,\\n    \\"offload_failures_only\\": false,\\n    \\"pool\\": \\"bvt\\",\\n    \\"priority\\": \\"PostBuild\\",\\n    \\"retry\\": false,\\n    \\"suite\\": \\"bvt-perbuild\\",\\n    \\"suite_min_duts\\": 1,\\n    \\"timeout\\": 5400,\\n    \\"warn_only\\": false\\n}"
                ]
            }
        ],
        "name": "parent_build"
    },
    "default_name_build": {
    }
}"""

    config = config_lib.LoadConfigFromString(src_str)

    expected_defaults = config_lib.DefaultSettings()
    expected_defaults.update({
        "bar": True,
        "baz": False,
        "child_configs": [],
        "foo": False,
        "hw_tests": [],
        "nested": {"sub1": 1, "sub2": 2},
    })

    self.assertEqual(config.GetDefault(), expected_defaults)

    # Verify assorted stuff in the loaded config to make sure it matches
    # expectations.
    self.assertFalse(config['match_build'].foo)
    self.assertTrue(config['match_build'].bar)
    self.assertFalse(config['match_build'].baz)
    self.assertTrue(config['diff_build'].foo)
    self.assertFalse(config['diff_build'].bar)
    self.assertTrue(config['diff_build'].baz)
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
    self.assertEqual(config['default_name_build'].name, 'default_name_build')

    self.assertTrue(config.params.site_foo)
    self.assertFalse(config.params.site_bar)

    # Load an save again, just to make sure there are no changes.
    loaded = config_lib.LoadConfigFromString(config.SaveConfigToString())

    self.assertEqual(config, loaded)

    # Make sure we can dump debug content without crashing.
    self.assertNotEqual(config.DumpExpandedConfigToString(), '')

  def testChromeOsLoad(self):
    """This test compares chromeos_config to config_dump.json."""
    # If there is a test failure, the diff will be big.
    self.maxDiff = None

    src = chromeos_config.GetConfig()
    new = config_lib.LoadConfigFromFile()

    self.assertDictEqual(src.GetDefault(),
                         new.GetDefault())

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


class SiteConfigFindTest(cros_test_lib.TestCase):
  """Tests related to Find helpers on SiteConfig."""

  def testGetBoardsMockConfig(self):
    site_config = MockSiteConfig()
    self.assertEqual(
        site_config.GetBoards(),
        set(['x86-generic']))

  def testGetBoardsComplexConfig(self):
    site_config = MockSiteConfig()
    site_config.Add('build_a', config_lib.BuildConfig(), boards=['foo_board'])
    site_config.Add('build_b', config_lib.BuildConfig(), boards=['bar_board'])
    site_config.Add('build_c', config_lib.BuildConfig(),
                    boards=['foo_board', 'car_board'])

    self.assertEqual(
        site_config.GetBoards(),
        set(['x86-generic', 'foo_board', 'bar_board', 'car_board']))


class FindConfigsForBoardTest(cros_test_lib.TestCase):
  """Test locating of official build for a board."""

  def setUp(self):
    self.config = chromeos_config.GetConfig()

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


class OverrideForTrybotTest(cros_test_lib.TestCase):
  """Test config override functionality."""

  # TODO(dgarrett): Test other override behaviors.

  def setUp(self):
    self.base_vmtests = [config_lib.VMTestConfig('base')]
    self.override_vmtests = [config_lib.VMTestConfig('override')]
    self.base_hwtests = [config_lib.HWTestConfig('base')]
    self.override_hwtests = [config_lib.HWTestConfig('override')]

    self.all_configs = MockSiteConfig()
    self.all_configs.Add(
        'no_tests_without_override',
        vm_tests=[],
    )
    self.all_configs.Add(
        'no_tests_with_override',
        vm_tests=[],
        vm_tests_override=self.override_vmtests,
        hw_tests_override=self.override_hwtests,
    )
    self.all_configs.Add(
        'tests_without_override',
        vm_tests=self.base_vmtests,
        hw_tests=self.base_hwtests,
    )
    self.all_configs.Add(
        'tests_with_override',
        vm_tests=self.base_vmtests,
        vm_tests_override=self.override_vmtests,
        hw_tests=self.base_hwtests,
        hw_tests_override=self.override_hwtests,
    )

  def _createMockOptions(self, **kwargs):
    mock_options = mock.Mock()
    for k, v in kwargs.iteritems():
      mock_options.__setattr__(k, v)

    return mock_options

  def testVmTestOverride(self):
    """Verify that vm_tests override for trybots pay heed to original config."""
    mock_options = self._createMockOptions(hwtest=False, remote_trybot=False)

    result = config_lib.OverrideConfigForTrybot(
        self.all_configs['no_tests_without_override'], mock_options)
    self.assertEqual(result.vm_tests, [])

    result = config_lib.OverrideConfigForTrybot(
        self.all_configs['no_tests_with_override'], mock_options)
    self.assertEqual(result.vm_tests, self.override_vmtests)

    result = config_lib.OverrideConfigForTrybot(
        self.all_configs['tests_without_override'], mock_options)
    self.assertEqual(result.vm_tests, self.base_vmtests)

    result = config_lib.OverrideConfigForTrybot(
        self.all_configs['tests_with_override'], mock_options)
    self.assertEqual(result.vm_tests, self.override_vmtests)

  def testHwTestOverrideDisabled(self):
    """Verify that hw_tests_override is not used without --hwtest."""
    mock_options = self._createMockOptions(hwtest=False, remote_trybot=False)

    result = config_lib.OverrideConfigForTrybot(
        self.all_configs['no_tests_without_override'], mock_options)
    self.assertEqual(result.hw_tests, [])

    result = config_lib.OverrideConfigForTrybot(
        self.all_configs['no_tests_with_override'], mock_options)
    self.assertEqual(result.hw_tests, [])

    result = config_lib.OverrideConfigForTrybot(
        self.all_configs['tests_without_override'], mock_options)
    self.assertEqual(result.hw_tests, self.base_hwtests)

    result = config_lib.OverrideConfigForTrybot(
        self.all_configs['tests_with_override'], mock_options)
    self.assertEqual(result.hw_tests, self.base_hwtests)

  def testHwTestOverrideEnabled(self):
    """Verify that hw_tests_override is not used without --hwtest."""
    mock_options = self._createMockOptions(hwtest=True, remote_trybot=False)

    result = config_lib.OverrideConfigForTrybot(
        self.all_configs['no_tests_without_override'], mock_options)
    self.assertEqual(result.hw_tests, [])

    result = config_lib.OverrideConfigForTrybot(
        self.all_configs['no_tests_with_override'], mock_options)
    self.assertEqual(result.hw_tests, self.override_hwtests)

    result = config_lib.OverrideConfigForTrybot(
        self.all_configs['tests_without_override'], mock_options)
    self.assertEqual(result.hw_tests, self.base_hwtests)

    result = config_lib.OverrideConfigForTrybot(
        self.all_configs['tests_with_override'], mock_options)
    self.assertEqual(result.hw_tests, self.override_hwtests)


class GetConfigTests(cros_test_lib.TestCase):
  """Tests related to SiteConfig.GetConfig()."""

  def testGetConfigCaching(self):
    """Test that config_lib.GetConfig() caches it's results correctly."""
    config_a = config_lib.GetConfig()
    config_b = config_lib.GetConfig()

    # Ensure that we get a SiteConfig, and that the result is cached.
    self.assertIsInstance(config_a, config_lib.SiteConfig)
    self.assertIs(config_a, config_b)

    # Clear our cache.
    config_lib.ClearConfigCache()
    config_c = config_lib.GetConfig()
    config_d = config_lib.GetConfig()

    # Ensure that this gives us a new instance of the SiteConfig.
    self.assertIsNot(config_a, config_c)

    # But also that it's cached going forward.
    self.assertIsInstance(config_c, config_lib.SiteConfig)
    self.assertIs(config_c, config_d)
