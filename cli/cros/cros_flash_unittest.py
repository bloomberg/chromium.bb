# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module tests the cros flash command."""

from __future__ import print_function

from chromite.cli import command
from chromite.cli import command_unittest
from chromite.cli import flash
from chromite.cli.cros import cros_flash
from chromite.lib import commandline
from chromite.lib import cros_test_lib


class MockFlashCommand(command_unittest.MockCommand):
  """Mock out the flash command."""
  TARGET = 'chromite.cli.cros.cros_flash.FlashCommand'
  TARGET_CLASS = cros_flash.FlashCommand
  COMMAND = 'flash'

  def __init__(self, *args, **kwargs):
    command_unittest.MockCommand.__init__(self, *args, **kwargs)

  def Run(self, inst):
    command_unittest.MockCommand.Run(self, inst)


class CrosFlashTest(cros_test_lib.MockTempDirTestCase,
                    cros_test_lib.OutputTestCase):
  """Test calling `cros flash` with various arguments.

  These tests just check that arguments as specified on the command
  line are properly passed through to flash.Flash(). Testing the
  actual update flow is done in the flash.Flash() unit tests.
  """

  IMAGE = '/path/to/image'
  DEVICE = '1.1.1.1'

  def SetupCommandMock(self, cmd_args):
    """Setup comand mock."""
    self.cmd_mock = MockFlashCommand(
        cmd_args, base_args=['--cache-dir', self.tempdir])
    self.StartPatcher(self.cmd_mock)

  def setUp(self):
    """Patches objects."""
    self.cmd_mock = None
    self.flash_mock = self.PatchObject(flash, 'Flash', autospec=True)

  def VerifyFlashParameters(self, device, image, **kwargs):
    """Verifies the arguments passed to flash.Flash().

    This function helps verify that command line specifications are
    parsed properly and handed to flash.Flash() as expected.

    Args:
      device: expected device hostname; currently only SSH devices
          are supported.
      image: expected image parameter.
      kwargs: keyword arguments expected in the call to flash.Flash().
          Arguments unspecified here are checked against their default
          value for `cros flash`.
    """
    flash_args, flash_kwargs = self.flash_mock.call_args
    self.assertEqual(device, flash_args[0].hostname)
    self.assertEqual(image, flash_args[1])
    # `cros flash` default options. Must match the configuration in AddParser().
    expected_kwargs = {
        'project_sdk_image': False,
        'sdk_version': None,
        'board': None,
        'brick_name': None,
        'blueprint_name': None,
        'install': False,
        'src_image_to_delta': None,
        'rootfs_update': True,
        'stateful_update': True,
        'clobber_stateful': False,
        'reboot': True,
        'wipe': True,
        'ping': True,
        'disable_rootfs_verification': False,
        'clear_cache': False,
        'yes': False,
        'force': False,
        'debug': False}
    # Overwrite defaults with any variations in this test.
    expected_kwargs.update(kwargs)
    self.assertDictEqual(expected_kwargs, flash_kwargs)

  def testDefaults(self):
    """Tests `cros flash` default values."""
    self.SetupCommandMock([self.DEVICE, self.IMAGE])
    self.cmd_mock.inst.Run()
    self.VerifyFlashParameters(self.DEVICE, self.IMAGE)

  def testBrilloFlashEntersChroot(self):
    """Test that brillo flash enters the chroot."""
    self.SetupCommandMock([self.DEVICE, self.IMAGE])
    self.PatchObject(command, 'GetToolset', return_value='brillo')
    enter_chroot = self.PatchObject(commandline, 'RunInsideChroot')
    self.cmd_mock.inst.Run()
    self.assertTrue(enter_chroot.called)

  def testCrosFlashDoesNotEnterChroot(self):
    """Test that cros flash doesn't enter the chroot."""
    self.SetupCommandMock([self.DEVICE, self.IMAGE])
    self.PatchObject(command, 'GetToolset', return_value='cros')
    enter_chroot = self.PatchObject(commandline, 'RunInsideChroot')
    self.cmd_mock.inst.Run()
    self.assertFalse(enter_chroot.called)

  def testBrick(self):
    """Tests command line --brick."""
    self.SetupCommandMock([self.DEVICE, self.IMAGE, '--brick', '//foo'])
    self.cmd_mock.inst.Run()
    self.VerifyFlashParameters(self.DEVICE, self.IMAGE, brick_name='//foo')

  def testImplicitBrick(self):
    """Tests an implicit brick based on |curr_brick_locator|."""
    self.SetupCommandMock([self.DEVICE, self.IMAGE])
    self.cmd_mock.inst.curr_brick_locator = '//bar'
    self.cmd_mock.inst.Run()
    self.VerifyFlashParameters(self.DEVICE, self.IMAGE, brick_name='//bar')

  def testBrickPriority(self):
    """Tests that command line --brick takes precedence."""
    self.SetupCommandMock([self.DEVICE, self.IMAGE, '--brick', '//foo'])
    self.cmd_mock.inst.curr_brick_locator = '//bar'
    self.cmd_mock.inst.Run()
    self.VerifyFlashParameters(self.DEVICE, self.IMAGE, brick_name='//foo')

  def testBrickPathNormalization(self):
    """Tests --brick path normalization."""
    self.SetupCommandMock([self.DEVICE, self.IMAGE, '--brick', 'foo'])
    self.cmd_mock.inst.Run()
    self.VerifyFlashParameters(self.DEVICE, self.IMAGE,
                               brick_name='//bricks/foo')

  def testFlashErrorDebug(self):
    """Tests that FlashErrors are passed through."""
    with self.OutputCapturer():
      self.SetupCommandMock([self.DEVICE, self.IMAGE])
      self.flash_mock.side_effect = flash.FlashError
      with self.assertRaises(flash.FlashError):
        self.cmd_mock.inst.Run()
