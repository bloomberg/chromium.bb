# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for the sysroot library."""

from __future__ import print_function

from chromite.lib import cros_test_lib
from chromite.lib import sysroot_lib


class SysrootLibTest(cros_test_lib.TempDirTestCase):
  """Unittest for sysroot_lib.py"""

  def testGetStandardField(self):
    """Tests that standard field can be fetched correctly."""
    sysroot = sysroot_lib.Sysroot(self.tempdir)
    sysroot.WriteConfig('FOO="bar"')
    self.assertEqual('bar', sysroot.GetStandardField('FOO'))

    # Works with multiline strings
    multiline = """foo
bar
baz
"""
    sysroot.WriteConfig('TEST="%s"' % multiline)
    self.assertEqual(multiline, sysroot.GetStandardField('TEST'))
