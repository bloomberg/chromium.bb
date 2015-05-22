# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for config."""

from __future__ import print_function

import copy
import cPickle

from chromite.cbuildbot import cbuildbot_config
from chromite.cbuildbot import config_lib
from chromite.lib import cros_test_lib


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
    config = config_lib.Config()

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
        "child_configs": [],
        "foo": false,
        "hw_tests": []
    },
    "diff_build": {
        "bar": false,
        "foo": true
    },
    "match_build": {},
    "parent_build": {
        "child_configs": [
            {},
            {
                "bar": false,
                "hw_tests": [
                    "{\\n    \\"async\\": true,\\n    \\"blocking\\": false,\\n    \\"critical\\": false,\\n    \\"file_bugs\\": true,\\n    \\"max_retries\\": null,\\n    \\"minimum_duts\\": 4,\\n    \\"num\\": 2,\\n    \\"pool\\": \\"bvt\\",\\n    \\"priority\\": \\"PostBuild\\",\\n    \\"retry\\": false,\\n    \\"suite\\": \\"bvt-perbuild\\",\\n    \\"suite_min_duts\\": 1,\\n    \\"timeout\\": 13200,\\n    \\"warn_only\\": false\\n}"
                ]
            }
        ]
    }
}"""

    config = config_lib.CreateConfigFromString(src_str)
    config_str = config.SaveConfigToString()

    # Verify that the dumped string matches the source string.
    self.assertEqual(src_str, config_str)

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
