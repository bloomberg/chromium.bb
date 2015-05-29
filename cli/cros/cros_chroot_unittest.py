# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests the `cros chroot` command."""

from __future__ import print_function

import os

from chromite.cbuildbot import constants
from chromite.cli import command_unittest
from chromite.cli.cros import cros_chroot
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import path_util
from chromite.lib import osutils
from chromite.lib import workspace_lib


class MockChrootCommand(command_unittest.MockCommand):
  """Mock out the `cros chroot` command."""
  TARGET = 'chromite.cli.cros.cros_chroot.ChrootCommand'
  TARGET_CLASS = cros_chroot.ChrootCommand
  COMMAND = 'chroot'

  def __init__(self, *args, **kwargs):
    command_unittest.MockCommand.__init__(self, *args, **kwargs)

  def Run(self, inst):
    return command_unittest.MockCommand.Run(self, inst)


class ChrootTest(cros_test_lib.WorkspaceTestCase, cros_test_lib.MockTestCase):
  """Test the ChrootCommand."""

  def SetupCommandMock(self, cmd_args):
    """Sets up the `cros chroot` command mock."""
    self.cmd_mock = MockChrootCommand(cmd_args)
    self.StartPatcher(self.cmd_mock)

  def setUp(self):
    """Patches objects."""
    self.cmd_mock = None

    # Pretend we are inside the chroot, so the command doesn't really enter.
    self.mock_inside = self.PatchObject(cros_build_lib, 'IsInsideChroot',
                                        return_value=True)

  def testInteractive(self):
    """Tests flow for an interactive session."""
    self.SetupCommandMock([])
    self.cmd_mock.inst.Run()

    # Ensure we exec'd bash.
    self.cmd_mock.rc_mock.assertCommandContains(['bash'], mute_output=False)

  def testExplicitCmdNoArgs(self):
    """Tests a non-interactive command as a single argument."""
    self.SetupCommandMock(['ls'])
    self.cmd_mock.inst.Run()

    # Ensure we exec'd ls with arguments.
    self.cmd_mock.rc_mock.assertCommandContains(['ls'])

  def testLoggingLevelNotice(self):
    """Tests that logging level is passed to cros_sdk script."""
    self.SetupCommandMock(['ls'])
    self.cmd_mock.inst.options.log_level = 'notice'
    # Pretend that we are outside the chroot so the logging level gets passed as
    # an argument to cros_sdk.
    self.mock_inside.return_value = False
    self.cmd_mock.inst.Run()

    #Ensure that we exec'd with logging level notice.
    self.cmd_mock.rc_mock.assertCommandContains(
        ['ls'], chroot_args=['--log-level', 'notice'], enter_chroot=True)

  def testExplicitCmd(self):
    """Tests a non-interactive command as a single argument."""
    self.SetupCommandMock(['ls', '/tmp'])
    self.cmd_mock.inst.Run()

    # Ensure we exec'd ls with arguments.
    self.cmd_mock.rc_mock.assertCommandContains(['ls', '/tmp'])

  def testOverlappingArguments(self):
    """Tests a non-interactive command as a single argument."""
    self.SetupCommandMock(['ls', '--help'])
    self.cmd_mock.inst.Run()

    # Ensure we pass along "--help" instead of processing it directly.
    self.cmd_mock.rc_mock.assertCommandContains(['ls', '--help'])

  def testDashDashShell(self):
    """Tests an interactive specified with  '--'."""
    self.SetupCommandMock(['--'])
    self.cmd_mock.inst.Run()

    # Only -- implies we run bash.
    self.cmd_mock.rc_mock.assertCommandContains(['bash'])

  def testDashDashArgCommand(self):
    """Tests a command name that matches a valid argument, after '--'."""
    # Technically, this should try to run the command "--help".
    self.SetupCommandMock(['--', '--help'])
    self.cmd_mock.inst.Run()

    # Ensure we pass along "--help" instead of processing it directly.
    self.cmd_mock.rc_mock.assertCommandContains(['--help'])

  def testNoWorkspace(self):
    """Tests entering the chroot from outside a workspace."""
    self.SetupCommandMock([])
    self.cmd_mock.inst.Run()

    # Make sure nothing was set in |extra_env|.
    self.cmd_mock.rc_mock.assertCommandContains(extra_env={})

  def testWorkspace(self):
    """Tests entering the chroot from inside a workspace."""
    self.SetupCommandMock([])
    self.CreateWorkspace()
    self.PatchObject(path_util.ChrootPathResolver, 'ToChroot',
                     return_value=constants.CHROOT_WORKSPACE_ROOT)
    self.cmd_mock.inst.Run()

    # Make sure CWD was set properly in |extra_env|.
    self.cmd_mock.rc_mock.assertCommandContains(extra_env={
        commandline.CHROOT_CWD_ENV_VAR: constants.CHROOT_WORKSPACE_ROOT})


class ChrootMoveTest(cros_test_lib.MockTempDirTestCase):
  """Test the ChrootCommand move functionality."""

  def SetupCommandMock(self, cmd_args):
    """Sets up the `cros chroot` command mock."""
    self.cmd_mock = MockChrootCommand(cmd_args)
    self.StartPatcher(self.cmd_mock)

  def setUp(self):
    """Patches objects."""
    self.cmd_mock = None

    self.work_dir = os.path.join(self.tempdir, 'work')
    osutils.SafeMakedirs(self.work_dir)

    # Force us to be inside the workspace.
    self.PatchObject(workspace_lib, 'WorkspacePath', return_value=self.work_dir)

  def testMove(self):
    """Tests a command name that matches a valid argument, after '--'."""
    # Technically, this should try to run the command "--help".
    self.SetupCommandMock(['--move', '/foo'])
    self.cmd_mock.inst.Run()

    # Verify that it took effect.
    self.assertEqual('/foo', workspace_lib.ChrootPath(self.work_dir))

