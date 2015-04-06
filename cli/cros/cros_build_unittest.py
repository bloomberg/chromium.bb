# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module tests the cros build command."""

from __future__ import print_function

from chromite.cli import command_unittest
from chromite.cli.cros import cros_build
from chromite.lib import chroot_util
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import parallel_unittest
from chromite.lib import partial_mock


class MockBuildCommand(command_unittest.MockCommand):
  """Mock out the build command."""
  TARGET = 'chromite.cli.cros.cros_build.BuildCommand'
  TARGET_CLASS = cros_build.BuildCommand

  def __init__(self, *args, **kwargs):
    super(MockBuildCommand, self).__init__(*args, **kwargs)
    self.chroot_update_called = 0

  def OnChrootUpdate(self, *_args, **_kwargs):
    self.chroot_update_called += 1

  def Run(self, inst):
    self.PatchObject(chroot_util, 'UpdateChroot',
                     side_effect=self.OnChrootUpdate)
    with parallel_unittest.ParallelMock():
      command_unittest.MockCommand.Run(self, inst)


class BuildCommandTest(cros_test_lib.MockTempDirTestCase):
  """Test class for our BuildCommand class."""

  def testSuccess(self):
    """Test that successful commands work."""
    cmds = [['--host', 'power_manager'],
            ['--board=foo', 'power_manager'],
            ['--board=foo', '--debug', 'power_manager'],
            ['--board=foo', '--no-deps', 'power_manager'],
            ['--board=foo', '--no-chroot-update', 'power_manager']]
    for cmd in cmds:
      update_chroot = not ('--no-deps' in cmd or '--no-chroot-update' in cmd)
      with MockBuildCommand(cmd) as build:
        build.inst.Run()
        self.assertEquals(1 if update_chroot else 0, build.chroot_update_called)

  def testFailedDeps(self):
    """Test that failures are detected correctly."""
    # pylint: disable=protected-access
    args = ['--board=foo', 'power_manager']
    with MockBuildCommand(args) as build:
      cmd = partial_mock.In('--backtrack=0')
      build.rc_mock.AddCmdResult(cmd=cmd, returncode=1, error='error\n')
      ex = self.assertRaises2(cros_build_lib.RunCommandError, build.inst.Run)
      self.assertTrue(cros_build.BuildCommand._BAD_DEPEND_MSG in ex.msg)
