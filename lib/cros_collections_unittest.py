# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test the cros_collections module."""

from __future__ import print_function

from chromite.lib import cros_collections
from chromite.lib import cros_test_lib


class CollectionTest(cros_test_lib.TestCase):
  """Tests for Collection helper."""

  def testDefaults(self):
    """Verify default values kick in."""
    O = cros_collections.Collection('O', a=0, b='string', c={})
    o = O()
    self.assertEqual(o.a, 0)
    self.assertEqual(o.b, 'string')
    self.assertEqual(o.c, {})

  def testOverrideDefaults(self):
    """Verify we can set custom values at instantiation time."""
    O = cros_collections.Collection('O', a=0, b='string', c={})
    o = O(a=1000)
    self.assertEqual(o.a, 1000)
    self.assertEqual(o.b, 'string')
    self.assertEqual(o.c, {})

  def testSetNoNewMembers(self):
    """Verify we cannot add new members after the fact."""
    O = cros_collections.Collection('O', a=0, b='string', c={})
    o = O()

    # Need the func since self.assertRaises evaluates the args in this scope.
    def _setit(collection):
      collection.does_not_exit = 10
    self.assertRaises(AttributeError, _setit, o)
    self.assertRaises(AttributeError, setattr, o, 'new_guy', 10)

  def testGetNoNewMembers(self):
    """Verify we cannot get new members after the fact."""
    O = cros_collections.Collection('O', a=0, b='string', c={})
    o = O()

    # Need the func since self.assertRaises evaluates the args in this scope.
    def _getit(collection):
      return collection.does_not_exit
    self.assertRaises(AttributeError, _getit, o)
    self.assertRaises(AttributeError, getattr, o, 'foooo')

  def testNewValue(self):
    """Verify we change members correctly."""
    O = cros_collections.Collection('O', a=0, b='string', c={})
    o = O()
    o.a = 'a string'
    o.c = 123
    self.assertEqual(o.a, 'a string')
    self.assertEqual(o.b, 'string')
    self.assertEqual(o.c, 123)

  def testString(self):
    """Make sure the string representation is readable by da hue mans."""
    O = cros_collections.Collection('O', a=0, b='string', c={})
    o = O()
    self.assertEqual("Collection_O(a=0, b='string', c={})", str(o))
