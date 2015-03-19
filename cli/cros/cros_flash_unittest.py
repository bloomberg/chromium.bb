# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module tests the cros flash command."""

from __future__ import print_function

import mock
import os

from chromite.cli import command_unittest
from chromite.cros.commands import cros_flash
from chromite.lib import brick_lib
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import dev_server_wrapper
from chromite.lib import partial_mock
from chromite.lib import remote_access


# pylint: disable=W0212
class MockFlashCommand(command_unittest.MockCommand):
  """Mock out the flash command."""
  TARGET = 'chromite.cros.commands.cros_flash.FlashCommand'
  TARGET_CLASS = cros_flash.FlashCommand
  COMMAND = 'flash'

  def __init__(self, *args, **kwargs):
    command_unittest.MockCommand.__init__(self, *args, **kwargs)

  def Run(self, inst):
    command_unittest.MockCommand.Run(self, inst)


class RemoteDeviceUpdaterMock(partial_mock.PartialCmdMock):
  """Mock out RemoteDeviceUpdater."""
  TARGET = 'chromite.cros.commands.cros_flash.RemoteDeviceUpdater'
  ATTRS = ('UpdateStateful', 'UpdateRootfs', 'SetupRootfsUpdate', 'Verify')

  def __init__(self):
    partial_mock.PartialCmdMock.__init__(self)

  def UpdateStateful(self, _inst, *_args, **_kwargs):
    """Mock out UpdateStateful."""

  def UpdateRootfs(self, _inst, *_args, **_kwargs):
    """Mock out UpdateRootfs."""

  def SetupRootfsUpdate(self, _inst, *_args, **_kwargs):
    """Mock out SetupRootfsUpdate."""

  def Verify(self, _inst, *_args, **_kwargs):
    """Mock out SetupRootfsUpdate."""


class BrickMock(partial_mock.PartialMock):
  """Mock out brick_lib.Brick."""
  TARGET = 'chromite.lib.brick_lib.Brick'
  ATTRS = ('Inherits')

  def __init__(self):
    partial_mock.PartialMock.__init__(self)

  def Inherits(self, _inst, *_args, **_kwargs):
    """Mock out Inherits."""
    return True


class UpdateRunThroughTest(cros_test_lib.MockTempDirTestCase,
                           cros_test_lib.LoggingTestCase):
  """Test the flow of FlashCommand.run with the update methods mocked out."""

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
    self.updater_mock = self.StartPatcher(RemoteDeviceUpdaterMock())
    self.PatchObject(dev_server_wrapper, 'GenerateXbuddyRequest',
                     return_value='xbuddy/local/latest')
    self.PatchObject(dev_server_wrapper, 'DevServerWrapper')
    self.PatchObject(dev_server_wrapper, 'GetImagePathWithXbuddy',
                     return_value='taco-paladin/R36/chromiumos_test_image.bin')
    self.PatchObject(dev_server_wrapper, 'GetUpdatePayloads')
    self.PatchObject(remote_access, 'CHECK_INTERVAL', new=0)
    self.PatchObject(remote_access, 'ChromiumOSDevice')

  def testUpdateAll(self):
    """Tests that update methods are called correctly."""
    self.SetupCommandMock([self.DEVICE, self.IMAGE])
    proj = BrickMock()
    with mock.patch('os.path.exists', return_value=True):
      with mock.patch('chromite.lib.brick_lib.FindBrickByName',
                      return_value=proj):
        self.cmd_mock.inst.Run()
        self.assertTrue(self.updater_mock.patched['UpdateStateful'].called)
        self.assertTrue(self.updater_mock.patched['UpdateRootfs'].called)

  def testUpdateStateful(self):
    """Tests that update methods are called correctly."""
    self.SetupCommandMock(['--no-rootfs-update', self.DEVICE, self.IMAGE])
    proj = BrickMock()
    with mock.patch('os.path.exists', return_value=True):
      with mock.patch('chromite.lib.brick_lib.FindBrickByName',
                      return_value=proj):
        self.cmd_mock.inst.Run()
        self.assertTrue(self.updater_mock.patched['UpdateStateful'].called)
        self.assertFalse(self.updater_mock.patched['UpdateRootfs'].called)

  def testUpdateRootfs(self):
    """Tests that update methods are called correctly."""
    self.SetupCommandMock(['--no-stateful-update', self.DEVICE, self.IMAGE])
    proj = BrickMock()
    with mock.patch('os.path.exists', return_value=True):
      with mock.patch('chromite.lib.brick_lib.FindBrickByName',
                      return_value=proj):
        self.cmd_mock.inst.Run()
        self.assertFalse(self.updater_mock.patched['UpdateStateful'].called)
        self.assertTrue(self.updater_mock.patched['UpdateRootfs'].called)

  def testMissingPayloads(self):
    """Tests we exit when payloads are missing."""
    self.SetupCommandMock([self.DEVICE, self.IMAGE])
    with mock.patch('os.path.exists', return_value=False):
      self.assertRaises(cros_build_lib.DieSystemExit, self.cmd_mock.inst.Run)

  def testProjectSdk(self):
    """Tests that Project SDK flashing invoked as expected."""
    self.SetupCommandMock(['--project-sdk', self.DEVICE, self.IMAGE])
    proj = BrickMock()
    with mock.patch('os.path.exists', return_value=True):
      with mock.patch('chromite.lib.brick_lib.FindBrickByName',
                      return_value=proj):
        with mock.patch('chromite.lib.project_sdk.FindVersion',
                        return_value='1.2.3'):
          self.cmd_mock.inst.Run()
          dev_server_wrapper.GetImagePathWithXbuddy.assert_called_with(
              'project_sdk', mock.ANY, version='1.2.3', static_dir=mock.ANY,
              device='ssh://%s' % self.DEVICE, lookup_only=True)
          self.assertTrue(self.updater_mock.patched['UpdateStateful'].called)
          self.assertTrue(self.updater_mock.patched['UpdateRootfs'].called)


class USBImagerMock(partial_mock.PartialCmdMock):
  """Mock out USBImager."""
  TARGET = 'chromite.cros.commands.cros_flash.USBImager'
  ATTRS = ('CopyImageToDevice', 'InstallImageToDevice',
           'ChooseRemovableDevice', 'ListAllRemovableDevices',
           'GetRemovableDeviceDescription', 'IsFilePathGPTDiskImage')
  VALID_IMAGE = True

  def __init__(self):
    partial_mock.PartialCmdMock.__init__(self)

  def CopyImageToDevice(self, _inst, *_args, **_kwargs):
    """Mock out CopyImageToDevice."""

  def InstallImageToDevice(self, _inst, *_args, **_kwargs):
    """Mock out InstallImageToDevice."""

  def ChooseRemovableDevice(self, _inst, *_args, **_kwargs):
    """Mock out ChooseRemovableDevice."""

  def ListAllRemovableDevices(self, _inst, *_args, **_kwargs):
    """Mock out ListAllRemovableDevices."""
    return ['foo', 'taco', 'milk']

  def GetRemovableDeviceDescription(self, _inst, *_args, **_kwargs):
    """Mock out GetRemovableDeviceDescription."""

  def IsFilePathGPTDiskImage(self, _inst, *_args, **_kwargs):
    """Mock out IsFilePathGPTDiskImage."""
    return self.VALID_IMAGE


class ImagingRunThroughTest(cros_test_lib.MockTempDirTestCase,
                            cros_test_lib.LoggingTestCase):
  """Test the flow of FlashCommand.run with the imaging methods mocked out."""
  IMAGE = '/path/to/image'

  def SetupCommandMock(self, cmd_args):
    """Setup comand mock."""
    self.cmd_mock = MockFlashCommand(
        cmd_args, base_args=['--cache-dir', self.tempdir, '--debug'])
    self.StartPatcher(self.cmd_mock)

  def setUp(self):
    """Patches objects."""
    self.cmd_mock = None
    self.usb_mock = USBImagerMock()
    self.imager_mock = self.StartPatcher(self.usb_mock)
    self.PatchObject(dev_server_wrapper, 'GenerateXbuddyRequest',
                     return_value='xbuddy/local/latest')
    self.PatchObject(dev_server_wrapper, 'DevServerWrapper')
    self.PatchObject(dev_server_wrapper, 'GetImagePathWithXbuddy',
                     return_value='taco-paladin/R36/chromiumos_test_image.bin')
    self.PatchObject(os.path, 'exists', return_value=True)
    self.PatchObject(brick_lib, 'FindBrickInPath', return_value=None)

  def testLocalImagePathCopy(self):
    """Tests that imaging methods are called correctly."""
    self.SetupCommandMock(['usb:///dev/foo', self.IMAGE])
    with mock.patch('os.path.isfile', return_value=True):
      self.cmd_mock.inst.Run()
      self.assertTrue(self.imager_mock.patched['CopyImageToDevice'].called)

  def testLocalImagePathInstall(self):
    """Tests that imaging methods are called correctly."""
    self.SetupCommandMock(['--board=taco', '--install', 'usb:///dev/foo',
                           self.IMAGE])
    with mock.patch('os.path.isfile', return_value=True):
      self.cmd_mock.inst.Run()
      self.assertTrue(self.imager_mock.patched['InstallImageToDevice'].called)

  def testLocalBadImagePath(self):
    """Tests that using an image not having the magic bytes has prompt."""
    self.usb_mock.VALID_IMAGE = False
    self.SetupCommandMock(['usb:///dev/foo', self.IMAGE])
    with mock.patch('os.path.isfile', return_value=True):
      with mock.patch.object(cros_build_lib, 'BooleanPrompt') as mock_prompt:
        mock_prompt.return_value = False
        self.cmd_mock.inst.Run()
        self.assertTrue(mock_prompt.called)

  def testNonLocalImagePath(self):
    """Tests that we try to get the image path using xbuddy."""
    self.SetupCommandMock(['usb:///dev/foo', self.IMAGE])
    with mock.patch.object(
        dev_server_wrapper,
        'GetImagePathWithXbuddy',
        return_value='translated/xbuddy/path') as mock_xbuddy:
      with mock.patch('os.path.isfile', return_value=False):
        with mock.patch('os.path.isdir', return_value=False):
          self.cmd_mock.inst.Run()
          self.assertTrue(mock_xbuddy.called)

  def testConfirmNonRemovableDevice(self):
    """Tests that we ask user to confirm if the device is not removable."""
    with mock.patch.object(cros_build_lib, 'BooleanPrompt') as mock_prompt:
      self.SetupCommandMock(['usb:///dev/dummy', self.IMAGE])
      self.cmd_mock.inst.Run()
      self.assertTrue(mock_prompt.called)

  def testSkipPromptNonRemovableDevice(self):
    """Tests that we skip the prompt for non-removable with --yes."""
    with mock.patch.object(cros_build_lib, 'BooleanPrompt') as mock_prompt:
      self.SetupCommandMock(['--yes', 'usb:///dev/dummy', self.IMAGE])
      self.cmd_mock.inst.Run()
      self.assertFalse(mock_prompt.called)

  def testChooseRemovableDevice(self):
    """Tests that we ask user to choose a device if none is given."""
    self.SetupCommandMock(['usb://', self.IMAGE])
    self.cmd_mock.inst.Run()
    self.assertTrue(self.imager_mock.patched['ChooseRemovableDevice'].called)
