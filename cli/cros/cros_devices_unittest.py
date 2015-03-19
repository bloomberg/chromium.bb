# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module tests the cros devices command."""

from __future__ import print_function

from chromite.cli import command_unittest
from chromite.cli.cros import cros_devices
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib


class MockDevicesCommand(command_unittest.MockCommand):
  """Mock out the devices command."""
  TARGET = 'chromite.cli.cros.cros_devices.DevicesCommand'
  TARGET_CLASS = cros_devices.DevicesCommand
  COMMAND = 'devices'
  ATTRS = ('_ListDevices', '_SetAlias', '_FullReset')

  def __init__(self, *arg, **kwargs):
    command_unittest.MockCommand.__init__(self, *arg, **kwargs)

  def _ListDevices(self, _inst, *_args, **_kwargs):
    """Mock out _ListDevices."""

  def _SetAlias(self, _inst, *_args, **_kwargs):
    """Mock out _SetAlias."""

  def _FullReset(self, _inst, *_args, **_kwargs):
    """Mock out _FullReset."""

  def Run(self, inst):
    command_unittest.MockCommand.Run(self, inst)


class DevicesRunThroughTest(cros_test_lib.MockTempDirTestCase):
  """Test the flow of DevicesCommand.run with the devices methods mocked out."""

  ALIAS_NAME = 'toaster1'

  def SetupCommandMock(self, cmd_args):
    """Set up command mock."""
    self.cmd_mock = MockDevicesCommand(
        cmd_args, base_args=['--cache-dir', self.tempdir])
    self.StartPatcher(self.cmd_mock)

  def setUp(self):
    """Patches objects."""
    self.cmd_mock = None

  def testListDevices(self):
    """Test that listing devices works correctly."""
    self.SetupCommandMock([])
    self.cmd_mock.inst.Run()
    self.assertTrue(self.cmd_mock.patched['_ListDevices'].called)

  def testInvalidSubcommand(self):
    """Test that command fails when the subcommand is invalid."""
    self.SetupCommandMock(['invalid'])
    self.assertRaises(cros_build_lib.DieSystemExit, self.cmd_mock.inst.Run)

  def testAliasMissing(self):
    """Test that command fails when alias is missing for "alias" command."""
    self.SetupCommandMock(['alias'])
    self.assertRaises(cros_build_lib.DieSystemExit, self.cmd_mock.inst.Run)

  def testSetAlias(self):
    """Test that setting alias works correctly."""
    self.SetupCommandMock(['alias', self.ALIAS_NAME])
    self.cmd_mock.inst.Run()
    self.assertTrue(self.cmd_mock.patched['_SetAlias'].called)

  def testFullReset(self):
    """Test that full reset works correctly."""
    self.SetupCommandMock(['full-reset'])
    self.cmd_mock.inst.Run()
    self.assertTrue(self.cmd_mock.patched['_FullReset'].called)

  def testFullResetExtraArg(self):
    """Test that "full-reset" command takes no extra argument."""
    self.SetupCommandMock(['full-reset', self.ALIAS_NAME])
    self.assertRaises(cros_build_lib.DieSystemExit, self.cmd_mock.inst.Run)
