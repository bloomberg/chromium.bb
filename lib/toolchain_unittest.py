# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for toolchain."""

from __future__ import print_function

from chromite.lib import cros_build_lib_unittest
from chromite.lib import cros_test_lib
from chromite.lib import toolchain


class ToolchainTest(cros_test_lib.TestCase):
  """Tests for lib.toolchain."""

  def testArchForToolchain(self):
    """Tests that we correctly parse crossdev's output."""
    rc_mock = cros_build_lib_unittest.RunCommandMock()

    noarch = """target=foo
category=bla
"""
    rc_mock.SetDefaultCmdResult(output=noarch)
    with rc_mock:
      self.assertEqual(None, toolchain.GetArchForTarget('fake_target'))

    amd64arch = """arch=amd64
target=foo
"""
    rc_mock.SetDefaultCmdResult(output=amd64arch)
    with rc_mock:
      self.assertEqual('amd64', toolchain.GetArchForTarget('fake_target'))
