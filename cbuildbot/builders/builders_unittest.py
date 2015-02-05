# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for main builder logic (__init__.py)."""

from __future__ import print_function

from chromite.cbuildbot import builders
from chromite.lib import cros_test_lib


class ModuleTest(cros_test_lib.MockTempDirTestCase):
  """Module loading related tests"""

  def testGetBuilderClassNone(self):
    """Check behavior when requesting missing builders."""
    self.assertRaises(ValueError, builders.GetBuilderClass, 'Foalksdjo')
    self.assertRaises(ImportError, builders.GetBuilderClass, 'foo.Foalksdjo')
    self.assertRaises(AttributeError, builders.GetBuilderClass,
                      'misc_builders.Foalksdjo')
