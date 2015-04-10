# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for the sysroot library."""

from __future__ import print_function

from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import sysroot_lib


class SysrootLibTest(cros_test_lib.TestCase):
  """Unittest for sysroot_lib.py"""

  def testGetStandardField(self):
    """Tests that standard field can be fetched correctly."""
    with osutils.TempDir(sudo_rm=True) as tempdir:
      sysroot = sysroot_lib.Sysroot(tempdir)
      sysroot.WriteConfig('FOO="bar"')
      self.assertEqual('bar', sysroot.GetStandardField('FOO'))

      # Works with multiline strings
      multiline = """foo
bar
baz
"""
      sysroot.WriteConfig('TEST="%s"' % multiline)
      self.assertEqual(multiline, sysroot.GetStandardField('TEST'))

  def testReadWriteCache(self):
    """Tests that we can write and read to the cache."""
    with osutils.TempDir(sudo_rm=True) as tempdir:
      sysroot = sysroot_lib.Sysroot(tempdir)

      # If a field is not defined we get None.
      self.assertEqual(None, sysroot.GetCachedField('foo'))

      # If we set a field, we can get it.
      sysroot.SetCachedField('foo', 'bar')
      self.assertEqual('bar', sysroot.GetCachedField('foo'))

      # Setting a field in an existing cache preserve the previous values.
      sysroot.SetCachedField('hello', 'bonjour')
      self.assertEqual('bar', sysroot.GetCachedField('foo'))
      self.assertEqual('bonjour', sysroot.GetCachedField('hello'))

      # Setting a field to None unsets it.
      sysroot.SetCachedField('hello', None)
      self.assertEqual(None, sysroot.GetCachedField('hello'))

  def testErrorOnBadCachedValue(self):
    """Tests that we detect bad value for the sysroot cache."""
    with osutils.TempDir(sudo_rm=True) as tempdir:
      sysroot = sysroot_lib.Sysroot(tempdir)

      forbidden = [
          'hello"bonjour',
          'hello\\bonjour',
          'hello\nbonjour',
          'hello$bonjour',
          'hello`bonjour',
      ]
      for value in forbidden:
        with self.assertRaises(ValueError):
          sysroot.SetCachedField('FOO', value)
