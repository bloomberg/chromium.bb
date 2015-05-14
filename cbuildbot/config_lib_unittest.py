# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for config."""

from __future__ import print_function

import copy
import cPickle

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
