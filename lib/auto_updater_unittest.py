# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for the auto_updater module.

The main parts of unittest include:
  1. test transfer methods in ChromiumOSUpdater.
  2. test precheck methods in ChromiumOSUpdater.
  3. test update methods in ChromiumOSUpdater.
  4. test reboot and verify method in ChromiumOSUpdater.
  5. test error raising in ChromiumOSUpdater.
"""

from __future__ import print_function

import mock

from chromite.lib import auto_updater
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import dev_server_wrapper
from chromite.lib import partial_mock
from chromite.lib import remote_access


class ChromiumOSBaseUpdaterMock(partial_mock.PartialCmdMock):
  """Mock out all update and verify functions in ChromiumOSUpdater."""
  TARGET = 'chromite.lib.auto_updater.ChromiumOSUpdater'
  ATTRS = ('RestoreStateful', 'UpdateStateful', 'UpdateRootfs',
           'SetupRootfsUpdate', 'RebootAndVerify')

  def __init__(self):
    partial_mock.PartialCmdMock.__init__(self)

  def RestoreStateful(self, _inst, *_args, **_kwargs):
    """Mock out RestoreStateful."""

  def UpdateStateful(self, _inst, *_args, **_kwargs):
    """Mock out UpdateStateful."""

  def UpdateRootfs(self, _inst, *_args, **_kwargs):
    """Mock out UpdateRootfs."""

  def SetupRootfsUpdate(self, _inst, *_args, **_kwargs):
    """Mock out SetupRootfsUpdate."""

  def RebootAndVerify(self, _inst, *_args, **_kwargs):
    """Mock out RebootAndVerify."""


class ChromiumOSTransferMock(partial_mock.PartialCmdMock):
  """Mock out all transfer functions in ChromiumOSUpdater."""
  TARGET = 'chromite.lib.auto_updater.ChromiumOSUpdater'
  ATTRS = ('TransferDevServerPackage', 'TransferRootfsUpdate',
           'TransferStatefulUpdate')

  def __init__(self):
    partial_mock.PartialCmdMock.__init__(self)

  def TransferDevServerPackage(self, _inst, *_args, **_kwargs):
    """Mock out TransferDevServerPackage."""

  def TransferRootfsUpdate(self, _inst, *_args, **_kwargs):
    """Mock out TransferRootfsUpdate."""

  def TransferStatefulUpdate(self, _inst, *_args, **_kwargs):
    """Mock out TransferStatefulUpdate."""


class ChromiumOSPreCheckMock(partial_mock.PartialCmdMock):
  """Mock out Precheck function in ChromiumOSUpdater."""
  TARGET = 'chromite.lib.auto_updater.ChromiumOSUpdater'
  ATTRS = ('_CheckRestoreStateful', '_CanRunDevserver')

  def __init__(self):
    partial_mock.PartialCmdMock.__init__(self)

  def _CheckRestoreStateful(self, _inst, *_args, **_kwargs):
    """Mock out _CheckRestoreStateful."""

  def _CanRunDevserver(self, _inst, *_args, **_kwargs):
    """Mock out _CanRunDevserve."""


class ChromiumOSUpdaterBaseTest(cros_test_lib.MockTestCase):
  """The base class for ChromiumOSUpdater test.

  In the setup, device, all transfer and update functions are mocked.
  """

  def setUp(self):
    self.payload_dir = ''
    self.base_updater_mock = self.StartPatcher(ChromiumOSBaseUpdaterMock())
    self.transfer_mock = self.StartPatcher(ChromiumOSTransferMock())
    self.PatchObject(remote_access, 'ChromiumOSDevice')


class ChromiumOSUpdateTransferTest(ChromiumOSUpdaterBaseTest):
  """Test the transfer code path."""

  def testTransferForRootfs(self):
    """Test transfer functions for rootfs update.

    When rootfs update is enabled, Devserver and rootfs update payload are
    transferred. Stateful update payload is not.
    """
    with remote_access.ChromiumOSDeviceHandler('1.1.1.1') as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(device, self.payload_dir,
                                               do_stateful_update=False)
      CrOS_AU.RunUpdate()
      self.assertTrue(
          self.transfer_mock.patched['TransferDevServerPackage'].called)
      self.assertTrue(
          self.transfer_mock.patched['TransferRootfsUpdate'].called)
      self.assertFalse(
          self.transfer_mock.patched['TransferStatefulUpdate'].called)

  def testTransferForStateful(self):
    """Test Transfer functions' code path for stateful update.

    When stateful update is enabled, Devserver and stateful update payload are
    transferred. Rootfs update payload is not.
    """
    with remote_access.ChromiumOSDeviceHandler('1.1.1.1') as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(device, self.payload_dir,
                                               do_rootfs_update=False)
      CrOS_AU.RunUpdate()
      self.assertTrue(
          self.transfer_mock.patched['TransferDevServerPackage'].called)
      self.assertFalse(
          self.transfer_mock.patched['TransferRootfsUpdate'].called)
      self.assertTrue(
          self.transfer_mock.patched['TransferStatefulUpdate'].called)


class ChromiumOSUpdatePreCheckTest(ChromiumOSUpdaterBaseTest):
  """Test precheck function."""

  def testCheckRestoreStateful(self):
    """Test whether _CheckRestoreStateful is called in update process."""
    precheck_mock = self.StartPatcher(ChromiumOSPreCheckMock())
    with remote_access.ChromiumOSDeviceHandler('1.1.1.1') as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(device, self.payload_dir)
      CrOS_AU.RunUpdate()
      self.assertTrue(precheck_mock.patched['_CheckRestoreStateful'].called)

  def testCheckRestoreStatefulError(self):
    """Test _CheckRestoreStateful fails with raising ChromiumOSUpdateError."""
    with remote_access.ChromiumOSDeviceHandler('1.1.1.1') as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(device, self.payload_dir)
      self.PatchObject(cros_build_lib, 'BooleanPrompt', return_value=False)
      self.PatchObject(auto_updater.ChromiumOSUpdater, '_CanRunDevserver',
                       return_value=False)
      self.assertRaises(auto_updater.ChromiumOSUpdateError, CrOS_AU.RunUpdate)

  def testNoPomptWithYes(self):
    """Test prompts won't be called if yes is set as True."""
    with remote_access.ChromiumOSDeviceHandler('1.1.1.1') as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(device, self.payload_dir,
                                               yes=True)
      self.PatchObject(cros_build_lib, 'BooleanPrompt')
      CrOS_AU.RunUpdate()
      self.assertFalse(cros_build_lib.BooleanPrompt.called)


class ChromiumOSUpdaterRunTest(ChromiumOSUpdaterBaseTest):
  """Test all update functions."""

  def testRestoreStateful(self):
    """Test RestoreStateful is called when it's required."""
    with remote_access.ChromiumOSDeviceHandler('1.1.1.1') as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(device, self.payload_dir)
      self.PatchObject(auto_updater.ChromiumOSUpdater, '_CheckRestoreStateful',
                       return_value=True)
      CrOS_AU.RunUpdate()
      self.assertTrue(self.base_updater_mock.patched['RestoreStateful'].called)

  def testRunRootfs(self):
    """Test the update functions are called correctly.

    SetupRootfsUpdate and UpdateRootfs are called for rootfs update.
    """
    with remote_access.ChromiumOSDeviceHandler('1.1.1.1') as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(device, self.payload_dir,
                                               do_stateful_update=False)
      CrOS_AU.RunUpdate()
      self.assertTrue(
          self.base_updater_mock.patched['SetupRootfsUpdate'].called)
      self.assertTrue(self.base_updater_mock.patched['UpdateRootfs'].called)
      self.assertFalse(self.base_updater_mock.patched['UpdateStateful'].called)

  def testRunStateful(self):
    """Test the update functions are called correctly.

    Only UpdateStateful is called for stateful update.
    """
    with remote_access.ChromiumOSDeviceHandler('1.1.1.1') as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(device, self.payload_dir,
                                               do_rootfs_update=False)
      CrOS_AU.RunUpdate()
      self.assertFalse(
          self.base_updater_mock.patched['SetupRootfsUpdate'].called)
      self.assertFalse(self.base_updater_mock.patched['UpdateRootfs'].called)
      self.assertTrue(self.base_updater_mock.patched['UpdateStateful'].called)


class ChromiumOSUpdaterVerifyTest(ChromiumOSUpdaterBaseTest):
  """Test verify function in ChromiumOSUpdater."""

  def testRebootAndVerifyWithRootfsAndReboot(self):
    """Test RebootAndVerify if rootfs update and reboot are enabled."""
    with remote_access.ChromiumOSDeviceHandler('1.1.1.1') as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(device, self.payload_dir)
      CrOS_AU.RunUpdate()
      self.assertTrue(self.base_updater_mock.patched['RebootAndVerify'].called)

  def testRebootAndVerifyWithoutReboot(self):
    """Test RebootAndVerify doesn't run if reboot is unenabled."""
    with remote_access.ChromiumOSDeviceHandler('1.1.1.1') as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(device, self.payload_dir,
                                               reboot=False)
      CrOS_AU.RunUpdate()
      self.assertFalse(self.base_updater_mock.patched['RebootAndVerify'].called)


class ChromiumOSUpdaterRunErrorTest(cros_test_lib.MockTestCase):
  """Test whether error is correctly reported during update process."""

  def setUp(self):
    """Mock device's function, and transfer/precheck functions for update.

    Not mock the class ChromiumOSDevice, in order to raise the errors that
    caused by a inner function of the device, like 'RunCommand'.
    """
    self.payload_dir = ''
    self.StartPatcher(ChromiumOSTransferMock())
    self.StartPatcher(ChromiumOSPreCheckMock())
    self.PatchObject(remote_access.RemoteDevice, 'Pingable', return_value=True)
    self.PatchObject(remote_access.RemoteDevice, 'work_dir', return_value='')
    self.PatchObject(remote_access.RemoteDevice, 'Reboot')
    self.PatchObject(remote_access.RemoteDevice, 'Cleanup')

  def prepareRootfsUpdate(self):
    """Prepare work for test errors in rootfs update."""
    self.PatchObject(auto_updater.ChromiumOSUpdater, 'SetupRootfsUpdate')
    self.PatchObject(auto_updater.ChromiumOSUpdater, 'GetRootDev')
    self.PatchObject(dev_server_wrapper.DevServerWrapper, 'TailLog')
    self.PatchObject(remote_access.RemoteDevice, 'CopyFromDevice')

  def testRestoreStatefulError(self):
    """Test ChromiumOSUpdater.RestoreStateful with raising exception.

    Devserver still cannot run after restoring stateful partition will lead
    to ChromiumOSUpdateError.
    """
    with remote_access.ChromiumOSDeviceHandler('1.1.1.1') as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(device, self.payload_dir)
      self.PatchObject(auto_updater.ChromiumOSUpdater, 'RunUpdateStateful')
      self.PatchObject(auto_updater.ChromiumOSUpdater, '_CheckRestoreStateful',
                       return_value=True)
      self.PatchObject(auto_updater.ChromiumOSUpdater, '_CanRunDevserver',
                       return_value=False)
      self.assertRaises(auto_updater.ChromiumOSUpdateError, CrOS_AU.RunUpdate)

  def testSetupRootfsUpdateError(self):
    """Test ChromiumOSUpdater.SetupRootfsUpdate with raising exception.

    RootfsUpdateError is raised if it cannot get status from GetUpdateStatus.
    """
    with remote_access.ChromiumOSDeviceHandler('1.1.1.1') as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(device, self.payload_dir)
      self.PatchObject(auto_updater.ChromiumOSUpdater, 'GetUpdateStatus',
                       return_value=('cannot_update', ))
      self.assertRaises(auto_updater.RootfsUpdateError, CrOS_AU.RunUpdate)

  def testRootfsUpdateDevServerError(self):
    """Test ChromiumOSUpdater.UpdateRootfs with raising exception.

    RootfsUpdateError is raised if devserver cannot start.
    """
    with remote_access.ChromiumOSDeviceHandler('1.1.1.1') as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(device, self.payload_dir)
      self.prepareRootfsUpdate()
      self.PatchObject(dev_server_wrapper.RemoteDevServerWrapper, 'Start',
                       side_effect=dev_server_wrapper.DevServerException())
      with mock.patch('os.path.join', return_value=''):
        self.assertRaises(auto_updater.RootfsUpdateError, CrOS_AU.RunUpdate)

  def testRootfsUpdateCmdError(self):
    """Test ChromiumOSUpdater.UpdateRootfs with raising exception.

    RootfsUpdateError is raised if device fails to run rootfs update command
    'update_engine_client ...'.
    """
    with remote_access.ChromiumOSDeviceHandler('1.1.1.1') as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(device, self.payload_dir)
      self.prepareRootfsUpdate()
      self.PatchObject(dev_server_wrapper.DevServerWrapper, 'Start')
      self.PatchObject(dev_server_wrapper.DevServerWrapper, 'GetDevServerURL')
      self.PatchObject(remote_access.ChromiumOSDevice, 'RunCommand',
                       side_effect=cros_build_lib.RunCommandError('fail', ''))
      with mock.patch('os.path.join', return_value=''):
        self.assertRaises(auto_updater.RootfsUpdateError, CrOS_AU.RunUpdate)

  def testRootfsUpdateTrackError(self):
    """Test ChromiumOSUpdater.UpdateRootfs with raising exception.

    RootfsUpdateError is raised if it fails to track device's status by
    GetUpdateStatus.
    """
    with remote_access.ChromiumOSDeviceHandler('1.1.1.1') as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(device, self.payload_dir)
      self.prepareRootfsUpdate()
      self.PatchObject(dev_server_wrapper.DevServerWrapper, 'Start')
      self.PatchObject(dev_server_wrapper.DevServerWrapper, 'GetDevServerURL')
      self.PatchObject(remote_access.ChromiumOSDevice, 'RunCommand')
      self.PatchObject(auto_updater.ChromiumOSUpdater, 'GetUpdateStatus',
                       side_effect=ValueError('Cannot get update status'))
      with mock.patch('os.path.join', return_value=''):
        self.assertRaises(auto_updater.RootfsUpdateError, CrOS_AU.RunUpdate)

  def testStatefulUpdateCmdError(self):
    """Test ChromiumOSUpdater.UpdateStateful with raising exception.

    StatefulUpdateError is raised if device fails to run stateful update
    command 'stateful_update ...'.
    """
    with remote_access.ChromiumOSDeviceHandler('1.1.1.1') as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(device, self.payload_dir,
                                               do_rootfs_update=False)
      self.PatchObject(remote_access.ChromiumOSDevice, 'RunCommand',
                       side_effect=cros_build_lib.RunCommandError('fail', ''))
      with mock.patch('os.path.join', return_value=''):
        self.assertRaises(auto_updater.StatefulUpdateError, CrOS_AU.RunUpdate)

  def testVerifyErrorWithSameRootDev(self):
    """Test RebootAndVerify fails with raising AutoUpdateVerifyError."""
    with remote_access.ChromiumOSDeviceHandler('1.1.1.1') as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(device, self.payload_dir,
                                               do_stateful_update=False)
      self.PatchObject(auto_updater.ChromiumOSUpdater, 'SetupRootfsUpdate')
      self.PatchObject(auto_updater.ChromiumOSUpdater, 'UpdateRootfs')
      self.PatchObject(auto_updater.ChromiumOSUpdater, 'GetRootDev',
                       return_value='fake_path')
      self.assertRaises(auto_updater.AutoUpdateVerifyError, CrOS_AU.RunUpdate)

  def testVerifyErrorWithRootDevEqualsNone(self):
    """Test RebootAndVerify fails with raising AutoUpdateVerifyError."""
    with remote_access.ChromiumOSDeviceHandler('1.1.1.1') as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(device, self.payload_dir,
                                               do_stateful_update=False)
      self.PatchObject(auto_updater.ChromiumOSUpdater, 'SetupRootfsUpdate')
      self.PatchObject(auto_updater.ChromiumOSUpdater, 'UpdateRootfs')
      self.PatchObject(auto_updater.ChromiumOSUpdater, 'GetRootDev',
                       return_value=None)
      self.assertRaises(auto_updater.AutoUpdateVerifyError, CrOS_AU.RunUpdate)

  def testNoVerifyError(self):
    """Test RebootAndVerify won't raise any error when unable do_rootfs_update.

    If do_rootfs_update is unabled, verify logic won't be touched, which means
    no AutoUpdateVerifyError will be thrown.
    """
    with remote_access.ChromiumOSDeviceHandler('1.1.1.1') as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(device, self.payload_dir,
                                               do_rootfs_update=False)
      self.PatchObject(remote_access.ChromiumOSDevice, 'RunCommand')
      self.PatchObject(auto_updater.ChromiumOSUpdater, 'GetRootDev',
                       return_value=None)
      try:
        CrOS_AU.RunUpdate()
      except auto_updater.AutoUpdateVerifyError:
        self.fail('RunUpdate raise AutoUpdateVerifyError.')
