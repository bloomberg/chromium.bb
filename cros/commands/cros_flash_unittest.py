#!/usr/bin/python
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module tests the cros flash command."""

import mock
import os
import sys

sys.path.insert(0, os.path.abspath('%s/../../..' % os.path.dirname(__file__)))
from chromite.cros.commands import cros_flash
from chromite.cros.commands import init_unittest
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import dev_server_wrapper
from chromite.lib import partial_mock
from chromite.lib import remote_access


# pylint: disable=W0212
class TestXbuddyHelpers(cros_test_lib.MockTempDirTestCase):
  """Test xbuddy helper functions."""
  def testGenerateXbuddyRequestForUpdate(self):
    """Test we generate correct xbuddy requests."""
    # Use the latest build when 'latest' is given.
    req = 'xbuddy/latest?for_update=true&return_dir=true'
    self.assertEqual(
        cros_flash.GenerateXbuddyRequest('latest', 'update'), req)

    # Convert the path starting with 'xbuddy://' to 'xbuddy/'
    path = 'xbuddy://remote/stumpy/version'
    req = 'xbuddy/remote/stumpy/version?for_update=true&return_dir=true'
    self.assertEqual(cros_flash.GenerateXbuddyRequest(path, 'update'), req)

  def testGenerateXbuddyRequestForImage(self):
    """Tests that we generate correct requests."""
    image_path = 'foo/bar/taco'
    self.assertEqual(cros_flash.GenerateXbuddyRequest(image_path, 'image'),
                     'xbuddy/foo/bar/taco?return_dir=true')

    image_path = 'xbuddy://foo/bar/taco'
    self.assertEqual(cros_flash.GenerateXbuddyRequest(image_path, 'image'),
                     'xbuddy/foo/bar/taco?return_dir=true')

  @mock.patch('chromite.lib.cros_build_lib.IsInsideChroot', return_value=True)
  def testDevserverURLToLocalPath(self, _mock1):
    """Tests that we convert a devserver URL to a local path correctly."""
    url = 'http://localhost:8080/static/peppy-release/R33-5116.87.0'
    base_path = os.path.join(self.tempdir, 'peppy-release/R33-5116.87.0')

    local_path = os.path.join(base_path, 'recovery_image.bin')
    self.assertEqual(
      cros_flash.DevserverURLToLocalPath(
          url, self.tempdir, 'recovery'), local_path)

    # Default to test image.
    local_path = os.path.join(base_path, 'chromiumos_test_image.bin')
    self.assertEqual(
      cros_flash.DevserverURLToLocalPath(url, self.tempdir, 'taco'), local_path)


class MockFlashCommand(init_unittest.MockCommand):
  """Mock out the flash command."""
  TARGET = 'chromite.cros.commands.cros_flash.FlashCommand'
  TARGET_CLASS = cros_flash.FlashCommand
  COMMAND = 'flash'

  def __init__(self, *args, **kwargs):
    init_unittest.MockCommand.__init__(self, *args, **kwargs)

  def Run(self, inst):
    init_unittest.MockCommand.Run(self, inst)


class RemoteDeviceUpdaterMock(partial_mock.PartialCmdMock):
  """Mock out RemoteDeviceUpdater."""
  TARGET = 'chromite.cros.commands.cros_flash.RemoteDeviceUpdater'
  ATTRS = ('UpdateStateful', 'UpdateRootfs', 'GetUpdatePayloads',
           'SetupRootfsUpdate', 'Verify')

  def __init__(self):
    partial_mock.PartialCmdMock.__init__(self)

  def GetUpdatePayloads(self, _inst, *_args, **_kwargs):
    """Mock out GetUpdatePayloads."""

  def UpdateStateful(self, _inst, *_args, **_kwargs):
    """Mock out UpdateStateful."""

  def UpdateRootfs(self, _inst, *_args, **_kwargs):
    """Mock out UpdateRootfs."""

  def SetupRootfsUpdate(self, _inst, *_args, **_kwargs):
    """Mock out SetupRootfsUpdate."""

  def Verify(self, _inst, *_args, **_kwargs):
    """Mock out SetupRootfsUpdate."""


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
    self.PatchObject(cros_flash, 'GenerateXbuddyRequest',
                     return_value='xbuddy/local/latest')
    self.PatchObject(dev_server_wrapper, 'DevServerWrapper')
    self.PatchObject(remote_access, 'CHECK_INTERVAL', new=0)
    self.PatchObject(remote_access.ChromiumOSDevice, '_LearnBoard',
                     return_value='peppy')

  def testUpdateAll(self):
    """Tests that update methods are called correctly."""
    self.SetupCommandMock([self.DEVICE, self.IMAGE])
    with mock.patch('os.path.exists', return_value=True) as _m:
      self.cmd_mock.inst.Run()
      self.assertTrue(self.updater_mock.patched['UpdateStateful'].called)
      self.assertTrue(self.updater_mock.patched['UpdateRootfs'].called)

  def testUpdateStateful(self):
    """Tests that update methods are called correctly."""
    self.SetupCommandMock(['--no-rootfs-update', self.DEVICE, self.IMAGE])
    with mock.patch('os.path.exists', return_value=True) as _m:
      self.cmd_mock.inst.Run()
      self.assertTrue(self.updater_mock.patched['UpdateStateful'].called)
      self.assertFalse(self.updater_mock.patched['UpdateRootfs'].called)

  def testUpdateRootfs(self):
    """Tests that update methods are called correctly."""
    self.SetupCommandMock(['--no-stateful-update', self.DEVICE, self.IMAGE])
    with mock.patch('os.path.exists', return_value=True) as _m:
      self.cmd_mock.inst.Run()
      self.assertFalse(self.updater_mock.patched['UpdateStateful'].called)
      self.assertTrue(self.updater_mock.patched['UpdateRootfs'].called)

  def testMissingPayloads(self):
    """Tests we exit when payloads are missing."""
    self.SetupCommandMock([self.DEVICE, self.IMAGE])
    with mock.patch('os.path.exists', return_value=False) as _m1:
      self.assertRaises(cros_build_lib.DieSystemExit, self.cmd_mock.inst.Run)


class USBImagerMock(partial_mock.PartialCmdMock):
  """Mock out USBImager."""
  TARGET = 'chromite.cros.commands.cros_flash.USBImager'
  ATTRS = ('GetImagePathFromDevserver', 'CopyImageToDevice',
           'ChooseRemovableDevice', 'ListAllRemovableDevices',
           'GetRemovableDeviceDescription')

  def __init__(self):
    partial_mock.PartialCmdMock.__init__(self)

  def GetImagePathFromDevserver(self, _inst, *_args, **_kwargs):
    """Mock out GetImagePathFromDevserver."""

  def CopyImageToDevice(self, _inst, *_args, **_kwargs):
    """Mock out CopyImageToDevice."""

  def ChooseRemovableDevice(self, _inst, *_args, **_kwargs):
    """Mock out ChooseRemovableDevice."""

  def ListAllRemovableDevices(self, _inst, *_args, **_kwargs):
    """Mock out ListAllRemovableDevices."""
    return ['foo', 'taco', 'milk']

  def GetRemovableDeviceDescription(self, _inst, *_args, **_kwargs):
    """Mock out GetRemovableDeviceDescription."""


class ImagingRunThroughTest(cros_test_lib.MockTempDirTestCase,
                            cros_test_lib.LoggingTestCase):
  """Test the flow of FlashCommand.run with the imaging methods mocked out."""
  IMAGE = '/path/to/image'

  def SetupCommandMock(self, cmd_args):
    """Setup comand mock."""
    self.cmd_mock = MockFlashCommand(
        cmd_args, base_args=['--cache-dir', self.tempdir])
    self.StartPatcher(self.cmd_mock)

  def setUp(self):
    """Patches objects."""
    self.cmd_mock = None
    self.imager_mock = self.StartPatcher(USBImagerMock())
    self.PatchObject(cros_flash, 'GenerateXbuddyRequest',
                     return_value='xbuddy/local/latest')

  def testLocalImagePath(self):
    """Tests that imaging methods are called correctly."""
    self.SetupCommandMock(['usb:///dev/foo', self.IMAGE])
    with mock.patch('os.path.isfile', return_value=True) as _m:
      self.cmd_mock.inst.Run()
      self.assertTrue(self.imager_mock.patched['CopyImageToDevice'].called)

  def testNonLocalImgePath(self):
    """Tests that we try to get the image path from devserver."""
    self.SetupCommandMock(['usb:///dev/foo', self.IMAGE])
    with mock.patch('os.path.isfile', return_value=False) as _m:
      self.cmd_mock.inst.Run()
      self.assertTrue(
          self.imager_mock.patched['GetImagePathFromDevserver'].called)
      self.assertTrue(self.imager_mock.patched['CopyImageToDevice'].called)

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


if __name__ == '__main__':
  cros_test_lib.main()
