#!/usr/bin/python

# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module tests the cros image command."""

import os
import sys

sys.path.insert(0, os.path.abspath('%s/../../../..' % __file__))
from chromite.cros.commands import cros_build
from chromite.cros.commands import init_unittest
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import parallel_unittest
from chromite.lib import partial_mock

# TODO(build): Finish test wrapper (http://crosbug.com/37517).
# Until then, this has to be after the chromite imports.
import mock


class MockBuildCommand(init_unittest.MockCommand):
  """Mock out the build command."""
  TARGET = 'chromite.cros.commands.cros_build.BuildCommand'
  TARGET_CLASS = cros_build.BuildCommand

  def Run(self, inst):
    packages = cros_build.GetToolchainPackages()
    with mock.patch.object(cros_build, 'GetToolchainPackages') as tc_mock:
      tc_mock.return_value = packages
      with parallel_unittest.ParallelMock():
        init_unittest.MockCommand.Run(self, inst)


class BuildCommandTest(cros_test_lib.TestCase):
  """Test class for our BuildCommand class."""

  def testSuccess(self):
    """Test that successful commands work."""
    cmds = [['--host', 'power_manager'],
            ['--board=foo', 'power_manager'],
            ['--board=foo', '--debug', 'power_manager'],
            ['--board=foo', '--no-deps', 'power_manager'],
            ['--board=foo', '--no-chroot-update', 'power_manager']]
    for cmd in cmds:
      with MockBuildCommand(cmd) as build:
        build.inst.Run()
        if '--no-deps' not in cmd:
          self.assertFalse(build.inst.chroot_update)

  def testFailedDeps(self):
    """Tests that failures are detected correctly."""
    # pylint: disable=W0212
    args = ['--board=foo', 'power_manager']
    with MockBuildCommand(args) as build:
      cmd = partial_mock.In('--backtrack=0')
      build.rc_mock.AddCmdResult(cmd=cmd, returncode=1, error='error\n')
      ex = self.assertRaises2(cros_build_lib.RunCommandError, build.inst.Run)
      self.assertTrue(cros_build.BuildCommand._BAD_DEPEND_MSG in ex.msg)

  def testGetToolchainPackages(self):
    """Test GetToolchainPackages function without mocking."""
    packages = cros_build.GetToolchainPackages()
    self.assertTrue(packages)


if __name__ == '__main__':
  cros_test_lib.main()
