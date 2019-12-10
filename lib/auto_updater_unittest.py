# -*- coding: utf-8 -*-
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

import json
import os

import mock

from chromite.lib import auto_updater
from chromite.lib import auto_updater_transfer
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import nebraska_wrapper
from chromite.lib import osutils
from chromite.lib import partial_mock
from chromite.lib import remote_access
from chromite.lib import remote_access_unittest


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


class ChromiumOSBaseTransferMock(partial_mock.PartialCmdMock):
  """Mock out all transfer functions in auto_updater.ChromiumOSUpdater.

  These methods have been deprecated in the auto_updater.ChromiumOSUpdater
  class. These mocks exist to test if the methods are not being called when
  ChromiumOSUpdater.RunUpdate() is called.

  TODO (sanikak): Mocked class should be removed once the deprecated methods
  are removed from auto_updater.ChromiumOSUpdater
  """
  TARGET = 'chromite.lib.auto_updater.ChromiumOSUpdater'
  ATTRS = ('TransferUpdateUtilsPackage', 'TransferRootfsUpdate',
           'TransferStatefulUpdate')

  def __init__(self):
    partial_mock.PartialCmdMock.__init__(self)

  def TransferUpdateUtilsPackage(self, _inst, *_args, **_kwargs):
    """Mock auto_updater.ChromiumOSUpdater.TransferUpdateUtilsPackage."""

  def TransferRootfsUpdate(self, _inst, *_args, **_kwargs):
    """Mock auto_updater.ChromiumOSUpdater.TransferRootfsUpdate."""

  def TransferStatefulUpdate(self, _inst, *_args, **_kwargs):
    """Mock auto_updater.ChromiumOSUpdater.TransferStatefulUpdate."""


class CrOSLocalTransferMock(partial_mock.PartialCmdMock):
  """Mock out all transfer functions in auto_updater_transfer.LocalTransfer."""
  TARGET = 'chromite.lib.auto_updater_transfer.LocalTransfer'
  ATTRS = ('TransferUpdateUtilsPackage', 'TransferRootfsUpdate',
           'TransferStatefulUpdate')

  def __init__(self):
    partial_mock.PartialCmdMock.__init__(self)

  def TransferUpdateUtilsPackage(self, _inst, *_args, **_kwargs):
    """Mock auto_updater_transfer.LocalTransfer.TransferUpdateUtilsPackage."""

  def TransferRootfsUpdate(self, _inst, *_args, **_kwargs):
    """Mock auto_updater_transfer.LocalTransferTransferRootfsUpdate."""

  def TransferStatefulUpdate(self, _inst, *_args, **_kwargs):
    """Mock auto_updater_transfer.LocalTransferTransferStatefulUpdate."""


class ChromiumOSPreCheckMock(partial_mock.PartialCmdMock):
  """Mock out Precheck function in auto_updater.ChromiumOSUpdater."""
  TARGET = 'chromite.lib.auto_updater.ChromiumOSUpdater'
  ATTRS = ('CheckRestoreStateful', '_CheckNebraskaCanRun')

  def __init__(self):
    partial_mock.PartialCmdMock.__init__(self)

  def CheckRestoreStateful(self, _inst, *_args, **_kwargs):
    """Mock out auto_updater.ChromiumOSUpdater.CheckRestoreStateful."""

  def _CheckNebraskaCanRun(self, _inst, *_args, **_kwargs):
    """Mock out auto_updater.ChromiumOSUpdater._CheckNebraskaCanRun."""


class RemoteAccessMock(remote_access_unittest.RemoteShMock):
  """Mock out RemoteAccess."""

  ATTRS = ('RemoteSh', 'Rsync', 'Scp')

  def Rsync(self, *_args, **_kwargs):
    return cros_build_lib.CommandResult(returncode=0)

  def Scp(self, *_args, **_kwargs):
    return cros_build_lib.CommandResult(returncode=0)


class ChromiumOSUpdaterBaseTest(cros_test_lib.MockTempDirTestCase):
  """The base class for auto_updater.ChromiumOSUpdater test.

  In the setup, device, all transfer and update functions are mocked.
  """

  def setUp(self):
    self._payload_dir = self.tempdir
    self._base_updater_mock = self.StartPatcher(ChromiumOSBaseUpdaterMock())
    self._transfer_mock = self.StartPatcher(CrOSLocalTransferMock())
    self._cros_transfer_mock = self.StartPatcher(ChromiumOSBaseTransferMock())
    self.PatchObject(remote_access.ChromiumOSDevice, 'Pingable',
                     return_value=True)
    m = self.StartPatcher(RemoteAccessMock())
    m.SetDefaultCmdResult()


class CrOSLocalTransferTest(ChromiumOSUpdaterBaseTest):
  """Test the transfer code path."""

  def testLocalTransferForRootfs(self):
    """Test transfer functions for rootfs update.

    When rootfs update is enabled, update-utils and rootfs update payload are
    transferred. Stateful update payload is not.
    """
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(
          device, None, self._payload_dir, do_stateful_update=False)
      self.PatchObject(auto_updater.ChromiumOSUpdater,
                       '_FixPayloadPropertiesFile')
      CrOS_AU.RunUpdate()
      self.assertTrue(
          self._transfer_mock.patched['TransferUpdateUtilsPackage'].called)
      self.assertTrue(
          self._transfer_mock.patched['TransferRootfsUpdate'].called)
      self.assertFalse(
          self._transfer_mock.patched['TransferStatefulUpdate'].called)

  def testLocalTransferForStateful(self):
    """Test Transfer functions' code path for stateful update.

    When stateful update is enabled, update-utils and stateful update payload
    are transferred. Rootfs update payload is not.
    """
    with remote_access.ChromiumOSDeviceHandler(
        remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(
          device, None, self._payload_dir, do_rootfs_update=False)
      CrOS_AU.RunUpdate()
      self.assertTrue(
          self._transfer_mock.patched['TransferUpdateUtilsPackage'].called)
      self.assertFalse(
          self._transfer_mock.patched['TransferRootfsUpdate'].called)
      self.assertTrue(
          self._transfer_mock.patched['TransferStatefulUpdate'].called)

  def testCrosTransferForRootfs(self):
    """Test auto_updater.ChromiumOSUpdater transfer methods for rootfs update.

    None of these functions should be called when auto_updater.RunUpdate() is
    called.

    TODO (sanikak): Function should be removed once the namesake deprecated
    methods are removed from auto_updater.ChromiumOSUpdater.
    """
    with remote_access.ChromiumOSDeviceHandler(
        remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(
          device, None, self._payload_dir, do_stateful_update=False)
      self.PatchObject(auto_updater.ChromiumOSUpdater,
                       '_FixPayloadPropertiesFile')
      CrOS_AU.RunUpdate()
      self.assertFalse(
          self._cros_transfer_mock.patched['TransferUpdateUtilsPackage'].called)
      self.assertFalse(
          self._cros_transfer_mock.patched['TransferRootfsUpdate'].called)
      self.assertFalse(
          self._cros_transfer_mock.patched['TransferStatefulUpdate'].called)

  def testCrosTransferForStateful(self):
    """Test auto_updater.ChromiumOSUpdater transfer methods for stateful update.

    None of these functions should be called when auto_updater.RunUpdate() is
    called.

    TODO (sanikak): Function should be removed once the namesake deprecated
    methods are removed from auto_updater.ChromiumOSUpdater.
    """
    with remote_access.ChromiumOSDeviceHandler(
        remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(
          device, None, self._payload_dir, do_rootfs_update=False)
      CrOS_AU.RunUpdate()
      self.assertFalse(
          self._cros_transfer_mock.patched['TransferUpdateUtilsPackage'].called)
      self.assertFalse(
          self._cros_transfer_mock.patched['TransferRootfsUpdate'].called)
      self.assertFalse(
          self._cros_transfer_mock.patched['TransferStatefulUpdate'].called)


class ChromiumOSUpdatePreCheckTest(ChromiumOSUpdaterBaseTest):
  """Test precheck function."""

  def testCheckRestoreStateful(self):
    """Test whether CheckRestoreStateful is called in update process."""
    precheck_mock = self.StartPatcher(ChromiumOSPreCheckMock())
    with remote_access.ChromiumOSDeviceHandler(
        remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(device, None, self._payload_dir)
      self.PatchObject(auto_updater.ChromiumOSUpdater,
                       '_FixPayloadPropertiesFile')
      CrOS_AU.RunUpdate()
      self.assertTrue(precheck_mock.patched['CheckRestoreStateful'].called)

  def testCheckRestoreStatefulError(self):
    """Test CheckRestoreStateful fails with raising ChromiumOSUpdateError."""
    with remote_access.ChromiumOSDeviceHandler(
        remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(device, None, self._payload_dir)
      self.PatchObject(cros_build_lib, 'BooleanPrompt', return_value=False)
      self.PatchObject(auto_updater.ChromiumOSUpdater,
                       '_CheckNebraskaCanRun',
                       side_effect=nebraska_wrapper.NebraskaStartupError())
      self.assertRaises(auto_updater.ChromiumOSUpdateError, CrOS_AU.RunUpdate)

  def testNoPomptWithYes(self):
    """Test prompts won't be called if yes is set as True."""
    with remote_access.ChromiumOSDeviceHandler(
        remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(
          device, None, self._payload_dir, yes=True)
      self.PatchObject(cros_build_lib, 'BooleanPrompt')
      self.PatchObject(auto_updater.ChromiumOSUpdater,
                       '_FixPayloadPropertiesFile')
      CrOS_AU.RunUpdate()
      self.assertFalse(cros_build_lib.BooleanPrompt.called)


class ChromiumOSUpdaterRunTest(ChromiumOSUpdaterBaseTest):
  """Test all update functions."""

  def testRestoreStateful(self):
    """Test RestoreStateful is called when it's required."""
    with remote_access.ChromiumOSDeviceHandler(
        remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(device, None, self._payload_dir)
      self.PatchObject(auto_updater.ChromiumOSUpdater,
                       'CheckRestoreStateful',
                       return_value=True)
      self.PatchObject(auto_updater.ChromiumOSUpdater,
                       '_FixPayloadPropertiesFile')
      CrOS_AU.RunUpdate()
      self.assertTrue(self._base_updater_mock.patched['RestoreStateful'].called)

  def test_FixPayloadPropertiesFileLocal(self):
    """Tests _FixPayloadPropertiesFile() correctly fixes the properties file.

    Tests if the payload properties file gets filled with correct data when
    LocalTransfer methods are invoked internally.
    """
    payload_filename = ('payloads/chromeos_9334.58.2_reef_stable-'
                        'channel_full_test.bin-e6a3290274a5b2ae0b9f491712ae1d8')
    payload_path = os.path.join(self._payload_dir, payload_filename)
    payload_properties_path = payload_path + '.json'
    osutils.WriteFile(payload_path, 'dummy-payload', makedirs=True)
    # Empty properties file.
    osutils.WriteFile(payload_properties_path, '{}')

    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(
          device, None, self._payload_dir, payload_filename=payload_filename)

      self.PatchObject(auto_updater_transfer.LocalTransfer,
                       'GetPayloadPropsFile',
                       return_value=payload_properties_path)
      CrOS_AU._FixPayloadPropertiesFile() # pylint: disable=protected-access

    # The payload properties file should be updated with new fields.
    props = json.loads(osutils.ReadFile(payload_properties_path))
    expected_props = {
        'appid': '',
        'is_delta': False,
        'size': os.path.getsize(payload_path),
        'target_version': '9334.58.2',
    }
    self.assertEqual(props, expected_props)

  def test_FixPayloadPropertiesFileLab(self):
    """Tests _FixPayloadPropertiesFile() correctly fixes the properties file.

    Tests if the payload properties file gets filled with correct data when
    LabTransfer methods are invoked internally.
    """
    payload_dir = 'nyan_kitty-release/R76-12345.17.0'
    payload_path = os.path.join(self.tempdir, 'test_update.gz')
    payload_properties_path = payload_path + '.json'
    osutils.WriteFile(payload_path, 'dummy-payload', makedirs=True)
    # Empty properties file.
    osutils.WriteFile(payload_properties_path, '{}')
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(
          device, None, payload_dir=payload_dir,
          staging_server='http://0.0.0.0:8082')

      self.PatchObject(auto_updater_transfer.LabTransfer, 'GetPayloadPropsFile',
                       return_value=payload_properties_path)
      self.PatchObject(auto_updater_transfer.LabTransfer, '_GetPayloadSize',
                       return_value=os.path.getsize(payload_path))
      CrOS_AU._FixPayloadPropertiesFile() # pylint: disable=protected-access

    # The payload properties file should be updated with new fields.
    props = json.loads(osutils.ReadFile(payload_properties_path))
    expected_props = {
        'appid': '',
        'is_delta': False,
        'size': os.path.getsize(payload_path),
        'target_version': '12345.17.0',
    }
    self.assertEqual(props, expected_props)

  def testRunRootfs(self):
    """Test the update functions are called correctly.

    SetupRootfsUpdate and UpdateRootfs are called for rootfs update.
    """
    with remote_access.ChromiumOSDeviceHandler(
        remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(
          device, None, self._payload_dir, do_stateful_update=False)
      self.PatchObject(auto_updater.ChromiumOSUpdater,
                       '_FixPayloadPropertiesFile')
      CrOS_AU.RunUpdate()
      self.assertTrue(
          self._base_updater_mock.patched['SetupRootfsUpdate'].called)
      self.assertTrue(self._base_updater_mock.patched['UpdateRootfs'].called)
      self.assertFalse(self._base_updater_mock.patched['UpdateStateful'].called)

  def testRunStateful(self):
    """Test the update functions are called correctly.

    Only UpdateStateful is called for stateful update.
    """
    with remote_access.ChromiumOSDeviceHandler(
        remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(
          device, None, self._payload_dir, do_rootfs_update=False)
      CrOS_AU.RunUpdate()
      self.assertFalse(
          self._base_updater_mock.patched['SetupRootfsUpdate'].called)
      self.assertFalse(self._base_updater_mock.patched['UpdateRootfs'].called)
      self.assertTrue(self._base_updater_mock.patched['UpdateStateful'].called)

  def testMismatchAppId(self):
    """Tests that we correctly set the payload App ID to empty when mismatch."""
    self._payload_dir = self.tempdir

    with remote_access.ChromiumOSDeviceHandler(
        remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(device, None, self._payload_dir)
      prop_file = os.path.join(self._payload_dir, 'payload.json')
      osutils.WriteFile(prop_file, '{"appid": "helloworld!"}')
      self.PatchObject(remote_access.ChromiumOSDevice, 'app_id',
                       return_value='different')
      CrOS_AU.ResolveAPPIDMismatchIfAny(prop_file)
      self.assertEqual(osutils.ReadFile(prop_file), '{"appid": ""}')

      osutils.WriteFile(prop_file, '{"appid": "same"}')
      self.PatchObject(remote_access.ChromiumOSDevice, 'app_id',
                       return_value='same')
      self.assertEqual(osutils.ReadFile(prop_file), '{"appid": "same"}')

  def testPrepareLocalPayloadPropsFile(self):
    """Tests that we correctly call ResolveAPPIDMismatchIfAny."""
    self._payload_dir = self.tempdir

    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(device, None, self._payload_dir)
      local_xfer = auto_updater_transfer.LocalTransfer
      cros_updater = auto_updater.ChromiumOSUpdater

      self.PatchObject(local_xfer, 'GetPayloadPropsFile',
                       return_value='/local/test_update.gz.json')
      self.PatchObject(cros_updater, 'ResolveAPPIDMismatchIfAny')

      CrOS_AU.PreparePayloadPropsFile()

      self.assertTrue(local_xfer.GetPayloadPropsFile.called)
      self.assertListEqual(
          cros_updater.ResolveAPPIDMismatchIfAny.call_args_list,
          [mock.call('/local/test_update.gz.json')])

  def testPrepareLabPayloadPropsFile(self):
    """Tests that we correctly call ResolveAPPIDMismatchIfAny."""
    self._payload_dir = self.tempdir

    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(
          device, None, self._payload_dir,
          staging_server='http://0.0.0.0:8082/')
      lab_xfer = auto_updater_transfer.LabTransfer
      cros_updater = auto_updater.ChromiumOSUpdater

      self.PatchObject(lab_xfer, 'GetPayloadPropsFile',
                       return_value='/local/test_update.gz.json')
      self.PatchObject(cros_updater, 'ResolveAPPIDMismatchIfAny')

      CrOS_AU.PreparePayloadPropsFile()

      self.assertTrue(lab_xfer.GetPayloadPropsFile.called)
      self.assertListEqual(
          cros_updater.ResolveAPPIDMismatchIfAny.call_args_list,
          [mock.call('/local/test_update.gz.json')])


class ChromiumOSUpdaterVerifyTest(ChromiumOSUpdaterBaseTest):
  """Test verify function in ChromiumOSUpdater."""

  def testRebootAndVerifyWithRootfsAndReboot(self):
    """Test RebootAndVerify if rootfs update and reboot are enabled."""
    with remote_access.ChromiumOSDeviceHandler(
        remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(device, None, self._payload_dir)
      self.PatchObject(auto_updater.ChromiumOSUpdater,
                       '_FixPayloadPropertiesFile')
      CrOS_AU.RunUpdate()
      self.assertTrue(self._base_updater_mock.patched['RebootAndVerify'].called)

  def testRebootAndVerifyWithoutReboot(self):
    """Test RebootAndVerify doesn't run if reboot is unenabled."""
    with remote_access.ChromiumOSDeviceHandler(
        remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(
          device, None, self._payload_dir, reboot=False)
      self.PatchObject(auto_updater.ChromiumOSUpdater,
                       '_FixPayloadPropertiesFile')
      CrOS_AU.RunUpdate()
      self.assertFalse(
          self._base_updater_mock.patched['RebootAndVerify'].called)


class ChromiumOSErrorTest(cros_test_lib.MockTestCase):
  """Base class for error test in auto_updater."""

  def setUp(self):
    """Mock device's functions for update.

    Not mock the class ChromiumOSDevice, in order to raise the errors that
    caused by a inner function of the device's base class, like 'run'.
    """
    self._payload_dir = ''
    self.PatchObject(remote_access.RemoteDevice, 'Pingable', return_value=True)
    self.PatchObject(remote_access.RemoteDevice, 'work_dir', new='')
    self.PatchObject(remote_access.RemoteDevice, 'Reboot')
    self.PatchObject(remote_access.RemoteDevice, 'Cleanup')


class ChromiumOSUpdaterRunErrorTest(ChromiumOSErrorTest):
  """Test whether error is correctly reported during update process."""

  def setUp(self):
    """Mock device's function, and transfer/precheck functions for update.

    Since cros_test_lib.MockTestCase run all setUp & tearDown methods in the
    inheritance tree, we don't call super().setUp().
    """
    self.StartPatcher(CrOSLocalTransferMock())
    self.StartPatcher(ChromiumOSPreCheckMock())

  def prepareRootfsUpdate(self):
    """Prepare work for test errors in rootfs update."""
    self.PatchObject(auto_updater.ChromiumOSUpdater, 'SetupRootfsUpdate')
    self.PatchObject(auto_updater.ChromiumOSUpdater, 'GetRootDev')
    self.PatchObject(nebraska_wrapper.RemoteNebraskaWrapper, 'PrintLog')
    self.PatchObject(remote_access.RemoteDevice, 'CopyFromDevice')

  def testRestoreStatefulError(self):
    """Test ChromiumOSUpdater.RestoreStateful with raising exception.

    Nebraska still cannot run after restoring stateful partition will lead
    to ChromiumOSUpdateError.
    """
    with remote_access.ChromiumOSDeviceHandler(
        remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(device, None, self._payload_dir)
      self.PatchObject(auto_updater.ChromiumOSUpdater,
                       'CheckRestoreStateful',
                       return_value=True)
      self.PatchObject(auto_updater.ChromiumOSUpdater, 'RunUpdateRootfs')
      self.PatchObject(auto_updater.ChromiumOSUpdater, 'PreSetupStatefulUpdate')
      self.PatchObject(auto_updater.ChromiumOSUpdater, 'TransferStatefulUpdate')
      self.PatchObject(auto_updater.ChromiumOSUpdater, 'ResetStatefulPartition')
      self.PatchObject(auto_updater.ChromiumOSUpdater, 'UpdateStateful')
      self.PatchObject(auto_updater.ChromiumOSUpdater,
                       'PostCheckStatefulUpdate')
      self.PatchObject(auto_updater.ChromiumOSUpdater,
                       '_IsUpdateUtilsPackageInstalled')
      self.PatchObject(auto_updater.ChromiumOSUpdater,
                       '_CheckNebraskaCanRun',
                       side_effect=nebraska_wrapper.NebraskaStartupError())
      self.assertRaises(auto_updater.ChromiumOSUpdateError, CrOS_AU.RunUpdate)

  def testSetupRootfsUpdateError(self):
    """Test ChromiumOSUpdater.SetupRootfsUpdate with raising exception.

    RootfsUpdateError is raised if it cannot get status from GetUpdateStatus.
    """
    with remote_access.ChromiumOSDeviceHandler(
        remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(device, None, self._payload_dir)
      self.PatchObject(auto_updater.ChromiumOSUpdater,
                       '_StartUpdateEngineIfNotRunning')
      self.PatchObject(auto_updater.ChromiumOSUpdater, 'GetUpdateStatus',
                       return_value=('cannot_update', ))
      self.assertRaises(auto_updater.RootfsUpdateError, CrOS_AU.RunUpdate)

  def testRootfsUpdateNebraskaError(self):
    """Test ChromiumOSUpdater.UpdateRootfs with raising exception.

    RootfsUpdateError is raised if nebraska cannot start.
    """
    with remote_access.ChromiumOSDeviceHandler(
        remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(device, None, self._payload_dir)
      self.prepareRootfsUpdate()
      self.PatchObject(nebraska_wrapper.RemoteNebraskaWrapper, 'Start',
                       side_effect=nebraska_wrapper.Error())
      self.PatchObject(auto_updater.ChromiumOSUpdater,
                       'RevertBootPartition')
      self.PatchObject(auto_updater.ChromiumOSUpdater,
                       '_FixPayloadPropertiesFile')
      with mock.patch('os.path.join', return_value=''):
        self.assertRaises(auto_updater.RootfsUpdateError, CrOS_AU.RunUpdate)

  def testRootfsUpdateCmdError(self):
    """Test ChromiumOSUpdater.UpdateRootfs with raising exception.

    RootfsUpdateError is raised if device fails to run rootfs update command
    'update_engine_client ...'.
    """
    with remote_access.ChromiumOSDeviceHandler(
        remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(device, None, self._payload_dir)
      self.prepareRootfsUpdate()
      self.PatchObject(nebraska_wrapper.RemoteNebraskaWrapper, 'Start')
      self.PatchObject(nebraska_wrapper.RemoteNebraskaWrapper, 'GetURL')
      self.PatchObject(remote_access.ChromiumOSDevice, 'RunCommand',
                       side_effect=cros_build_lib.RunCommandError('fail'))
      self.PatchObject(auto_updater.ChromiumOSUpdater,
                       'RevertBootPartition')
      self.PatchObject(auto_updater.ChromiumOSUpdater,
                       '_FixPayloadPropertiesFile')
      with mock.patch('os.path.join', return_value=''):
        self.assertRaises(auto_updater.RootfsUpdateError, CrOS_AU.RunUpdate)

  def testRootfsUpdateTrackError(self):
    """Test ChromiumOSUpdater.UpdateRootfs with raising exception.

    RootfsUpdateError is raised if it fails to track device's status by
    GetUpdateStatus.
    """
    with remote_access.ChromiumOSDeviceHandler(
        remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(device, None, self._payload_dir)
      self.prepareRootfsUpdate()
      self.PatchObject(nebraska_wrapper.RemoteNebraskaWrapper, 'Start')
      self.PatchObject(nebraska_wrapper.RemoteNebraskaWrapper, 'GetURL')
      self.PatchObject(remote_access.ChromiumOSDevice, 'RunCommand')
      self.PatchObject(auto_updater.ChromiumOSUpdater, 'GetUpdateStatus',
                       side_effect=ValueError('Cannot get update status'))
      self.PatchObject(auto_updater.ChromiumOSUpdater,
                       '_FixPayloadPropertiesFile')
      with mock.patch('os.path.join', return_value=''):
        self.assertRaises(auto_updater.RootfsUpdateError, CrOS_AU.RunUpdate)

  def testStatefulUpdateCmdError(self):
    """Test ChromiumOSUpdater.UpdateStateful with raising exception.

    StatefulUpdateError is raised if device fails to run stateful update
    command 'stateful_update ...'.
    """
    with remote_access.ChromiumOSDeviceHandler(
        remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(
          device, None, self._payload_dir, do_rootfs_update=False)
      self.PatchObject(remote_access.ChromiumOSDevice, 'RunCommand',
                       side_effect=cros_build_lib.RunCommandError('fail'))
      self.PatchObject(auto_updater.ChromiumOSUpdater,
                       'ResetStatefulPartition')
      with mock.patch('os.path.join', return_value=''):
        self.assertRaises(auto_updater.StatefulUpdateError, CrOS_AU.RunUpdate)

  def testVerifyErrorWithSameRootDev(self):
    """Test RebootAndVerify fails with raising AutoUpdateVerifyError."""
    with remote_access.ChromiumOSDeviceHandler(
        remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(
          device, None, self._payload_dir, do_stateful_update=False)
      self.PatchObject(remote_access.ChromiumOSDevice, 'RunCommand')
      self.PatchObject(auto_updater.ChromiumOSUpdater, 'SetupRootfsUpdate')
      self.PatchObject(auto_updater.ChromiumOSUpdater, 'UpdateRootfs')
      self.PatchObject(auto_updater.ChromiumOSUpdater, 'GetRootDev',
                       return_value='fake_path')
      self.PatchObject(auto_updater.ChromiumOSUpdater,
                       '_FixPayloadPropertiesFile')
      self.assertRaises(auto_updater.AutoUpdateVerifyError, CrOS_AU.RunUpdate)

  def testVerifyErrorWithRootDevEqualsNone(self):
    """Test RebootAndVerify fails with raising AutoUpdateVerifyError."""
    with remote_access.ChromiumOSDeviceHandler(
        remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(
          device, None, self._payload_dir, do_stateful_update=False)
      self.PatchObject(auto_updater.ChromiumOSUpdater, 'SetupRootfsUpdate')
      self.PatchObject(auto_updater.ChromiumOSUpdater, 'UpdateRootfs')
      self.PatchObject(remote_access.ChromiumOSDevice, 'RunCommand')
      self.PatchObject(auto_updater.ChromiumOSUpdater, 'GetRootDev',
                       return_value=None)
      self.PatchObject(auto_updater.ChromiumOSUpdater,
                       '_FixPayloadPropertiesFile')
      self.assertRaises(auto_updater.AutoUpdateVerifyError, CrOS_AU.RunUpdate)

  def testNoVerifyError(self):
    """Test RebootAndVerify won't raise any error when unable do_rootfs_update.

    If do_rootfs_update is unabled, verify logic won't be touched, which means
    no AutoUpdateVerifyError will be thrown.
    """
    with remote_access.ChromiumOSDeviceHandler(
        remote_access.TEST_IP) as device:
      CrOS_AU = auto_updater.ChromiumOSUpdater(
          device, None, self._payload_dir, do_rootfs_update=False)
      self.PatchObject(remote_access.ChromiumOSDevice, 'RunCommand')
      self.PatchObject(auto_updater.ChromiumOSUpdater, 'GetRootDev',
                       return_value=None)
      try:
        CrOS_AU.RunUpdate()
      except auto_updater.AutoUpdateVerifyError:
        self.fail('RunUpdate raise AutoUpdateVerifyError.')
