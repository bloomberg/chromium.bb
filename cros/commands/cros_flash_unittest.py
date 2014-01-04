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
from chromite.lib import remote_access


# pylint: disable=W0212
class TestXbuddyHelpers(cros_test_lib.MockTempDirTestCase):
  """Test xbuddy helper functions."""
  def testCheckBoardMismatches(self):
    """Test we correctly determine whether there is board mismatch."""

    board = 'lumpy'
    path = 'local/lumpy/latest'
    self.assertEqual(cros_flash._CheckBoardMismatch(path, board), False)

    board = 'peppy'
    path = 'remote/lumpy/latest'
    self.assertEqual(cros_flash._CheckBoardMismatch(path, board), True)

    board = 'lumpy'
    path = 'lumpy/latest'
    self.assertEqual(cros_flash._CheckBoardMismatch(path, board), False)

  def testGenerateXbuddyRequest(self):
    """Test we generate correct xbuddy requests."""
    # Use the latest build when 'latest' is given.
    board = 'stumpy'
    req = 'xbuddy/local/stumpy/latest?for_update=true&return_dir=true'
    self.assertEqual(
        cros_flash.GenerateXbuddyRequest('latest', None, board=board), req)

    # Convert the path starting with 'xbuddy://' to 'xbuddy/'
    board = 'stumpy'
    path = 'xbuddy://remote/stumpy/version'
    req = 'xbuddy/remote/stumpy/version?for_update=true&return_dir=true'
    self.assertEqual(
        cros_flash.GenerateXbuddyRequest(path, None, board=board), req)

  @mock.patch('chromite.lib.cros_build_lib.IsInsideChroot', return_value=True)
  @mock.patch('os.path.exists', return_value=True)
  def testGenerateXbuddyRequestWithSymlink(self, _mock1, _mock2):
    """Tests that we create symlinks for xbuddy requests."""
    image_path = '/foo/bar/chromiumos_test_image.bin'
    link = os.path.join(self.tempdir, 'others',
                        '2fdcf2f57d2bca72cf2f698363a4da1c')
    cros_flash.GenerateXbuddyRequest(image_path, self.tempdir, board=None)
    self.assertTrue(os.path.lexists(link))


class MockFlashCommand(init_unittest.MockCommand):
  """Mock out the flash command."""
  TARGET = 'chromite.cros.commands.cros_flash.FlashCommand'
  TARGET_CLASS = cros_flash.FlashCommand
  COMMAND = 'flash'
  ATTRS = init_unittest.MockCommand.ATTRS + (
      'UpdateStateful', 'UpdateRootfs', 'GetUpdatePayloads')

  UPDATE_ENGINE_TIMEOUT = 1

  def __init__(self, *args, **kwargs):
    init_unittest.MockCommand.__init__(self, *args, **kwargs)

  def Run(self, inst):
    # with mock.patch(
    #   'chromite.lib.dev_server_wrapper.DevServerWrapper') as _m1:
    init_unittest.MockCommand.Run(self, inst)

  def GetUpdatePayloads(self, _inst, *_args, **_kwargs):
    """Mock out GetUpdatePayloads."""

  def UpdateStateful(self, _inst, *_args, **_kwargs):
    """Mock out UpdateStateful."""

  def UpdateRootfs(self, _inst, *_args, **_kwargs):
    """Mock out UpdateRootfs."""


class ReadOptionTest(cros_test_lib.MockTempDirTestCase):
  """Test that we read options and set flags correctly."""
  IMAGE = '/path/to/image'
  DEVICE = '1.1.1.1'

  def SetupCommandMock(self, cmd_args):
    """Setup command mock."""
    self.cmd_mock = MockFlashCommand(
        cmd_args, base_args=['--cache-dir', self.tempdir])
    self.StartPatcher(self.cmd_mock)

  def setUp(self):
    self.cmd_mock = None

  def testParser(self):
    """Tests that our parser works normally."""
    self.SetupCommandMock([self.DEVICE, self.IMAGE])
    self.assertEquals(self.cmd_mock.inst.options.device, self.DEVICE)
    self.assertEquals(self.cmd_mock.inst.options.image, self.IMAGE)

  def testUpdateAll(self):
    """Tests that update operations are set correctly."""
    self.SetupCommandMock([self.DEVICE, self.IMAGE])
    self.cmd_mock.inst._ReadOptions()
    self.assertEquals(self.cmd_mock.inst.do_stateful_update, True)
    self.assertEquals(self.cmd_mock.inst.do_rootfs_update, True)

  def testUpdateRootfsOnly(self):
    """Tests that update operations are set correctly."""
    self.SetupCommandMock(['--no-stateful-update', self.DEVICE, self.IMAGE])
    self.cmd_mock.inst._ReadOptions()
    self.assertEquals(self.cmd_mock.inst.do_stateful_update, False)
    self.assertEquals(self.cmd_mock.inst.do_rootfs_update, True)

  def testUpdateStatefulOnly(self):
    """Tests that update operations are set correctly."""
    self.SetupCommandMock(['--no-rootfs-update', self.DEVICE, self.IMAGE])
    self.cmd_mock.inst._ReadOptions()
    self.assertEquals(self.cmd_mock.inst.do_stateful_update, True)
    self.assertEquals(self.cmd_mock.inst.do_rootfs_update, False)

  def testNeedUpdateOp(self):
    """Tests that at least one update operations is set."""
    self.SetupCommandMock(['--no-stateful-update', '--no-rootfs-update',
                           self.DEVICE, self.IMAGE])
    self.assertRaises(cros_build_lib.DieSystemExit,
                      self.cmd_mock.inst._ReadOptions)

  def testImageAsPayloadDir(self):
    """Tests that image_as_payload_dir is set when a directory is given."""
    self.SetupCommandMock([self.DEVICE, self.IMAGE])
    with mock.patch('os.path.isdir', return_value=True):
      self.cmd_mock.inst._ReadOptions()
    self.assertEquals(self.cmd_mock.inst.image_as_payload_dir, True)

  def testDebug(self):
    """Tests that no wipe when debug is set."""
    self.SetupCommandMock(['--debug', self.DEVICE, self.IMAGE])
    self.cmd_mock.inst._ReadOptions()
    self.assertEquals(self.cmd_mock.inst.options.debug, True)
    self.assertEquals(self.cmd_mock.inst.wipe, False)


class RunTest(cros_test_lib.MockTempDirTestCase,
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
      self.assertTrue(self.cmd_mock.patched['UpdateStateful'].called)
      self.assertTrue(self.cmd_mock.patched['UpdateRootfs'].called)

  def testUpdateStateful(self):
    """Tests that update methods are called correctly."""
    self.SetupCommandMock(['--no-rootfs-update', self.DEVICE, self.IMAGE])
    with mock.patch('os.path.exists', return_value=True) as _m:
      self.cmd_mock.inst.Run()
      self.assertTrue(self.cmd_mock.patched['UpdateStateful'].called)
      self.assertFalse(self.cmd_mock.patched['UpdateRootfs'].called)

  def testUpdateRootfs(self):
    """Tests that update methods are called correctly."""
    self.SetupCommandMock(['--no-stateful-update', self.DEVICE, self.IMAGE])
    with mock.patch('os.path.exists', return_value=True) as _m:
      self.cmd_mock.inst.Run()
      self.assertFalse(self.cmd_mock.patched['UpdateStateful'].called)
      self.assertTrue(self.cmd_mock.patched['UpdateRootfs'].called)

  def testMissingPayloads(self):
    """Tests we exit when payloads are missing."""
    self.SetupCommandMock([self.DEVICE, self.IMAGE])
    with mock.patch('os.path.exists', return_value=False) as _m1:
      self.assertRaises(cros_build_lib.DieSystemExit, self.cmd_mock.inst.Run)


if __name__ == '__main__':
  cros_test_lib.main()
