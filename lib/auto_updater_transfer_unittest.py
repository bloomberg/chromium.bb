# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for the auto_updater_tranfer module.

The main parts of unittest include:
  1. test transfer methods in LocalTransfer.
  5. test retrials in LocalTransfer.
"""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import os

import mock

from chromite.lib import auto_updater_transfer
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import partial_mock
from chromite.lib import path_util
from chromite.lib import remote_access


# pylint: disable=protected-access


def CreateLocalTransferInstance(device, **kwargs):
  """Create auto_updater_transfer.LocalTransfer instance.

  Args:
    device: a remote_access.ChromiumOSDeviceHandler object.
    kwargs: contains parameter name and value pairs for any argument accepted
      by auto_updater_transfer.LocalTransfer. The values provided through
      kwargs will supersede the defaults set within this function.

  Returns:
    An instance of auto_updater_transfer.LocalTransfer.
  """
  default_args = {'payload_dir': '', 'device_payload_dir': '', 'tempdir': '',
                  'payload_name': '', 'cmd_kwargs': {},
                  'device_restore_dir': ''}
  default_args.update(kwargs)
  return auto_updater_transfer.LocalTransfer(device=device, **default_args)

def CreateLabTransferInstance(device, **kwargs):
  """Create auto_updater_transfer.LabTransfer instance.

  Args:
    device: a remote_access.ChromiumOSDeviceHandler object.
    kwargs: contains parameter name and value pairs for any argument accepted
      by auto_updater_transfer.LocalTransfer. The values provided through
      kwargs will supersede the defaults set within this function.

  Returns:
    An instance of auto_updater_transfer.LabTransfer.
  """
  default_args = {'payload_dir': '', 'device_payload_dir': '',
                  'payload_name': '', 'cmd_kwargs': {},
                  'device_restore_dir': '',
                  'staging_server': 'http://0.0.0.0:8000'}
  default_args.update(kwargs)
  return auto_updater_transfer.LabTransfer(device=device, **default_args)


class CrOSLocalTransferPrivateMock(partial_mock.PartialCmdMock):
  """Mock out all transfer functions in auto_updater_transfer.LocalTransfer."""
  TARGET = 'chromite.lib.auto_updater_transfer.LocalTransfer'
  ATTRS = ('_TransferStatefulUpdate', '_TransferRootfsUpdate',
           '_TransferUpdateUtilsPackage', '_GetStatefulUpdateScript',
           '_EnsureDeviceDirectory')

  def __init__(self):
    partial_mock.PartialCmdMock.__init__(self)

  def _TransferStatefulUpdate(self, _inst, *_args, **_kwargs):
    """Mock auto_updater_transfer.LocalTransfer._TransferStatefulUpdate."""

  def _TransferRootfsUpdate(self, _inst, *_args, **_kwargs):
    """Mock auto_updater_transfer.LocalTransfer._TransferRootfsUpdate."""

  def _TransferUpdateUtilsPackage(self, _inst, *_args, **_kwargs):
    """Mock auto_updater_transfer.LocalTransfer._TransferUpdateUtilsPackage."""

  def _GetStatefulUpdateScript(self, _inst, *_args, **_kwargs):
    """Mock auto_updater_transfer.LocalTransfer._GetStatefulUpdateScript."""
    return True, ''

  def _EnsureDeviceDirectory(self, _inst, *_args, **_kwargs):
    """Mock auto_updater_transfer.LocalTransfer._EnsureDeviceDirectory."""


class CrosLocalTransferTest(cros_test_lib.MockTestCase):
  """Test whether LocalTransfer's transfer functions are retried."""

  def setUp(self):
    """Mock remote_access.RemoteDevice's functions for update."""
    self.PatchObject(remote_access.RemoteDevice, 'work_dir', '')
    self.PatchObject(remote_access.RemoteDevice, 'CopyToWorkDir')
    self.PatchObject(remote_access.RemoteDevice, 'CopyToDevice')

  def testErrorTriggerRetryTransferUpdateUtils(self):
    """Test LocalTransfer._TransferUpdateUtilsPackage() is retried properly."""
    with remote_access.ChromiumOSDeviceHandler('1.1.1.1') as device:
      CrOS_LocalTransfer = CreateLocalTransferInstance(device)
      self.PatchObject(auto_updater_transfer, '_DELAY_SEC_FOR_RETRY', 1)
      _MAX_RETRY = self.PatchObject(auto_updater_transfer, '_MAX_RETRY', 1)
      transfer_update_utils = self.PatchObject(
          auto_updater_transfer.LocalTransfer,
          '_TransferUpdateUtilsPackage',
          side_effect=cros_build_lib.RunCommandError('fail'))
      self.assertRaises(cros_build_lib.RunCommandError,
                        CrOS_LocalTransfer.TransferUpdateUtilsPackage)
      self.assertEqual(transfer_update_utils.call_count, _MAX_RETRY + 1)

  def testErrorTriggerRetryTransferStateful(self):
    """Test LocalTransfer._TransferStatefulUpdate() is retried properly."""
    with remote_access.ChromiumOSDeviceHandler('1.1.1.1') as device:
      CrOS_LocalTransfer = CreateLocalTransferInstance(device)
      self.PatchObject(auto_updater_transfer, '_DELAY_SEC_FOR_RETRY', 1)
      _MAX_RETRY = self.PatchObject(auto_updater_transfer, '_MAX_RETRY', 2)
      transfer_stateful = self.PatchObject(
          auto_updater_transfer.LocalTransfer,
          '_TransferStatefulUpdate',
          side_effect=cros_build_lib.RunCommandError('fail'))
      self.assertRaises(cros_build_lib.RunCommandError,
                        CrOS_LocalTransfer.TransferStatefulUpdate)
      self.assertEqual(transfer_stateful.call_count, _MAX_RETRY + 1)

  def testErrorTriggerRetryTransferRootfs(self):
    """Test LocalTransfer._TransferRootfsUpdate() is retried properly."""
    with remote_access.ChromiumOSDeviceHandler('1.1.1.1') as device:
      CrOS_LocalTransfer = CreateLocalTransferInstance(device)
      self.PatchObject(auto_updater_transfer, '_DELAY_SEC_FOR_RETRY', 1)
      _MAX_RETRY = self.PatchObject(auto_updater_transfer, '_MAX_RETRY', 3)
      transfer_rootfs = self.PatchObject(
          auto_updater_transfer.LocalTransfer,
          '_TransferRootfsUpdate',
          side_effect=cros_build_lib.RunCommandError('fail'))
      self.assertRaises(cros_build_lib.RunCommandError,
                        CrOS_LocalTransfer.TransferRootfsUpdate)
      self.assertEqual(transfer_rootfs.call_count, _MAX_RETRY + 1)

  def testTransferStatefulUpdateNeedsTransfer(self):
    """Test transfer functions for stateful update.

    Test whether auto_updater_transfer.LocalTransfer._GetStatefulUpdateScript()
    and auto_updater_transfer.LocalTransfer._EnsureDeviceDirectory() are being
    called correctly. Test the value if the instance variable
    auto_updater_transfer.LocalTransfer._stateful_update_bin is set correctly
    when auto_updater_transfer.LocalTransfer._original_payload_dir is set to
    None.
    """
    self.PatchObject(auto_updater_transfer.LocalTransfer,
                     '_GetStatefulUpdateScript',
                     return_value=[False, 'test_stateful_update_bin'])
    self.PatchObject(auto_updater_transfer.LocalTransfer,
                     '_EnsureDeviceDirectory')
    self.PatchObject(auto_updater_transfer, 'STATEFUL_FILENAME',
                     'test_stateful.tgz')
    with remote_access.ChromiumOSDeviceHandler('1.1.1.1') as device:
      CrOS_LocalTransfer = CreateLocalTransferInstance(device)
      CrOS_LocalTransfer._TransferStatefulUpdate()
      self.assertTrue(
          auto_updater_transfer.LocalTransfer._GetStatefulUpdateScript.called)
      self.assertEqual(
          CrOS_LocalTransfer._stateful_update_bin, 'test_stateful_update_bin')
      self.assertFalse(
          auto_updater_transfer.LocalTransfer._EnsureDeviceDirectory.called)

  def testTransferStatefulUpdateNoNeedsTransfer(self):
    """Test transfer functions for stateful update.

    Test whether auto_updater_transfer.LocalTransfer._GetStatefulUpdateScript()
    and auto_updater_transfer.LocalTransfer._EnsureDeviceDirectory() are being
    called correctly. Test the value if the instance variable
    auto_updater_transfer.LocalTransfer._stateful_update_bin is set correctly
    when auto_updater_transfer.LocalTransfer._original_payload_dir is set to
    'test_dir'.
    """
    self.PatchObject(auto_updater_transfer.LocalTransfer,
                     '_GetStatefulUpdateScript',
                     return_value=[True, 'test_stateful_update_bin'])
    self.PatchObject(auto_updater_transfer.LocalTransfer,
                     '_EnsureDeviceDirectory')
    self.PatchObject(auto_updater_transfer, 'STATEFUL_FILENAME',
                     'test_stateful.tgz')
    with remote_access.ChromiumOSDeviceHandler('1.1.1.1') as device:
      CrOS_LocalTransfer = CreateLocalTransferInstance(
          device, original_payload_dir='test_dir')
      CrOS_LocalTransfer._TransferStatefulUpdate()
      self.assertTrue(
          auto_updater_transfer.LocalTransfer._GetStatefulUpdateScript.called)
      self.assertEqual(CrOS_LocalTransfer._stateful_update_bin,
                       'stateful_update')
      self.assertTrue(
          auto_updater_transfer.LocalTransfer._EnsureDeviceDirectory.called)

  def testLocalTransferCheckPayloadsError(self):
    """Test auto_updater_transfer.CheckPayloads with raising exception.

    auto_updater_transfer.ChromiumOSTransferError is raised if it does not find
    payloads in its path.
    """
    self.PatchObject(os.path, 'exists', return_value=False)
    with remote_access.ChromiumOSDeviceHandler('1.1.1.1') as device:
      CrOS_LocalTransfer = CreateLocalTransferInstance(
          device, payload_name='does_not_exist')
      self.assertRaises(
          auto_updater_transfer.ChromiumOSTransferError,
          CrOS_LocalTransfer.CheckPayloads)

  def testLocalTransferCheckPayloads(self):
    """Test auto_updater_transfer.CheckPayloads without raising exception.

    Test will fail if ChromiumOSTransferError is raised when payload exists.
    """
    self.PatchObject(os.path, 'exists', return_value=True)
    with remote_access.ChromiumOSDeviceHandler('1.1.1.1') as device:
      CrOS_LocalTransfer = CreateLocalTransferInstance(device,
                                                       payload_name='exists')
      CrOS_LocalTransfer.CheckPayloads()

  def testGetStatefulUpdateScriptLocalChroot(self):
    """Test auto_updater_transfer.LocalTransfer._GetStatefulUpdateScript().

    Test if method returns correct responses when os.path.exists() is set to
    return True for the path returned by the path_util.FromChrootPath() call.
    """
    self.PatchObject(path_util, 'FromChrootPath', return_value='test_path')
    with remote_access.ChromiumOSDeviceHandler('1.1.1.1') as device:
      CrOS_LocalTransfer = CreateLocalTransferInstance(device)
      with self.PatchObject(os.path, 'exists', side_effect=[True]):
        self.assertTrue(CrOS_LocalTransfer._GetStatefulUpdateScript(),
                        (True, 'test_path'))

  def testGetStatefulUpdateScriptLocalStatefulUpdateFile(self):
    """Test auto_updater_transfer.LocalTransfer._GetStatefulUpdateScript().

    Test if method returns correct responses when os.path.exists() is set to
    return True for the path returned by the os.path.join() call that joins
    _dev_dir and LOCAL_STATEFUL_UPDATE_FILENAME.
    """
    self.PatchObject(path_util, 'FromChrootPath', return_value='test_path')
    with remote_access.ChromiumOSDeviceHandler('1.1.1.1') as device:
      CrOS_LocalTransfer = CreateLocalTransferInstance(device)
      with self.PatchObject(os.path, 'exists', side_effect=[False, True]):
        self.assertEqual(CrOS_LocalTransfer._GetStatefulUpdateScript(),
                         (True, 'stateful_update'))

  def testGetStatefulUpdateScriptRemoteStatefulUpdatePath(self):
    """Test auto_updater_transfer.LocalTransfer._GetStatefulUpdateScript().

    Test if method returns correct responses when os.path.exists() is set to
    return False for all its calls and _GetStatefulUpdateScript() is made to
    rely on default return values.
    """
    self.PatchObject(path_util, 'FromChrootPath', return_value='test_path')
    with remote_access.ChromiumOSDeviceHandler('1.1.1.1') as device:
      CrOS_LocalTransfer = CreateLocalTransferInstance(device)
      with self.PatchObject(os.path, 'exists', side_effect=[False, False]):
        self.assertEqual(CrOS_LocalTransfer._GetStatefulUpdateScript(),
                         (False, '/usr/local/bin/stateful_update'))


class CrosLabTransferTest(cros_test_lib.MockTestCase):
  """Test all methods in auto_updater_transfer.LabTransfer."""

  def setUp(self):
    """Mock remote_access.RemoteDevice/ChromiumOSDevice functions for update."""
    self.PatchObject(remote_access.RemoteDevice, 'work_dir', '/test/work/dir')

  def testErrorTriggerRetryTransferUpdateUtils(self):
    """Test LabTransfer._TransferUpdateUtilsPackage() is retried properly."""
    with remote_access.ChromiumOSDeviceHandler('1.1.1.1') as device:
      CrOS_LabTransfer = CreateLabTransferInstance(device)
      self.PatchObject(auto_updater_transfer, '_DELAY_SEC_FOR_RETRY', 1)
      _MAX_RETRY = self.PatchObject(auto_updater_transfer, '_MAX_RETRY', 1)
      transfer_update_utils = self.PatchObject(
          auto_updater_transfer.LabTransfer,
          '_TransferUpdateUtilsPackage',
          side_effect=cros_build_lib.RunCommandError('fail'))
      self.assertRaises(cros_build_lib.RunCommandError,
                        CrOS_LabTransfer.TransferUpdateUtilsPackage)
      self.assertEqual(transfer_update_utils.call_count, _MAX_RETRY + 1)

  def testTransferUpdateUtilsCurlCalls(self):
    """Test methods calls of LabTransfer._TransferUpdateUtilsPackage().

    Test whether LabTransfer._GetCurlCmdForPayloadDownload() is being called
    the correct number of times with the correct arguments.
    """
    with remote_access.ChromiumOSDeviceHandler('1.1.1.1') as device:
      CrOS_LabTransfer = CreateLabTransferInstance(device)
      lab_xfer = auto_updater_transfer.LabTransfer
      expected = [{'payload_dir': os.path.join(device.work_dir, 'src'),
                   'payload_filename': 'nebraska.py'}]

      self.PatchObject(lab_xfer, '_EnsureDeviceDirectory')
      self.PatchObject(lab_xfer, '_GetCurlCmdForPayloadDownload')
      self.PatchObject(remote_access.ChromiumOSDevice, 'RunCommand')

      CrOS_LabTransfer._TransferUpdateUtilsPackage()
      self.assertListEqual(
          lab_xfer._GetCurlCmdForPayloadDownload.call_args_list,
          [mock.call(**x) for x in expected])

  def testTransferUpdateUtilsRunCmdCalls(self):
    """Test methods calls of LabTransfer._TransferUpdateUtilsPackage().

    Test whether remote_access.ChromiumOSDevice.RunCommand() is being called
    the correct number of times with the correct arguments.
    """
    with remote_access.ChromiumOSDeviceHandler('1.1.1.1') as device:
      CrOS_LabTransfer = CreateLabTransferInstance(device)
      lab_xfer = auto_updater_transfer.LabTransfer
      expected = [['curl', '-o', '/test/work/dir/src/nebraska.py',
                   'http://0.0.0.0:8000/static/nebraska.py']]

      self.PatchObject(lab_xfer, '_EnsureDeviceDirectory')
      self.PatchObject(remote_access.ChromiumOSDevice, 'RunCommand')

      CrOS_LabTransfer._TransferUpdateUtilsPackage()
      self.assertListEqual(
          remote_access.ChromiumOSDevice.RunCommand.call_args_list,
          [mock.call(x) for x in expected])

  def testTransferUpdateUtilsEnsureDirCalls(self):
    """Test methods calls of LabTransfer._TransferUpdateUtilsPackage().

    Test whether LabTransfer._EnsureDeviceDirectory() is being called
    the correct number of times with the correct arguments.
    """
    with remote_access.ChromiumOSDeviceHandler('1.1.1.1') as device:
      CrOS_LabTransfer = CreateLabTransferInstance(device)
      lab_xfer = auto_updater_transfer.LabTransfer
      expected = [os.path.join(device.work_dir, 'src'), device.work_dir]

      self.PatchObject(lab_xfer, '_EnsureDeviceDirectory')
      self.PatchObject(remote_access.ChromiumOSDevice, 'RunCommand')

      CrOS_LabTransfer._TransferUpdateUtilsPackage()
      self.assertListEqual(lab_xfer._EnsureDeviceDirectory.call_args_list,
                           [mock.call(x) for x in expected])

  def testErrorTransferUpdateUtilsServerError(self):
    """Test errors thrown by LabTransfer._TransferUpdateUtilsPackage()."""
    with remote_access.ChromiumOSDeviceHandler('1.1.1.1') as device:
      CrOS_LabTransfer = CreateLabTransferInstance(
          device, staging_server='http://wrong:server')
      self.PatchObject(auto_updater_transfer.LabTransfer,
                       '_EnsureDeviceDirectory')
      self.PatchObject(remote_access.ChromiumOSDevice, 'RunCommand',
                       side_effect=cros_build_lib.RunCommandError('fail'))
      self.assertRaises(cros_build_lib.RunCommandError,
                        CrOS_LabTransfer._TransferUpdateUtilsPackage)

  def testErrorTriggerRetryTransferStateful(self):
    """Test LabTransfer._TransferStatefulUpdate() is retried properly."""
    with remote_access.ChromiumOSDeviceHandler('1.1.1.1') as device:
      CrOS_LabTransfer = CreateLabTransferInstance(device)
      self.PatchObject(auto_updater_transfer, '_DELAY_SEC_FOR_RETRY', 1)
      _MAX_RETRY = self.PatchObject(auto_updater_transfer, '_MAX_RETRY', 2)
      transfer_stateful = self.PatchObject(
          auto_updater_transfer.LabTransfer,
          '_TransferStatefulUpdate',
          side_effect=cros_build_lib.RunCommandError('fail'))
      self.assertRaises(cros_build_lib.RunCommandError,
                        CrOS_LabTransfer.TransferStatefulUpdate)
      self.assertEqual(transfer_stateful.call_count, _MAX_RETRY + 1)

  def testErrorTransferStatefulServerError(self):
    """Test errors thrown by LabTransfer._TransferStatefulUpdate()."""
    with remote_access.ChromiumOSDeviceHandler('1.1.1.1') as device:
      CrOS_LabTransfer = CreateLabTransferInstance(
          device, staging_server='http://wrong:server')
      self.PatchObject(auto_updater_transfer.LabTransfer,
                       '_EnsureDeviceDirectory')
      self.PatchObject(remote_access.ChromiumOSDevice, 'RunCommand',
                       side_effect=cros_build_lib.RunCommandError('fail'))
      self.assertRaises(cros_build_lib.RunCommandError,
                        CrOS_LabTransfer._TransferStatefulUpdate)

  def testTransferStatefulCurlCmdCalls(self):
    """Test methods calls of LabTransfer._TransferStatefulUpdate().

    Test whether LabTransfer._GetCurlCmdForPayloadDownload() is being called
    the correct number of times with the correct arguments.
    """
    with remote_access.ChromiumOSDeviceHandler('1.1.1.1') as device:
      CrOS_LabTransfer = CreateLabTransferInstance(
          device, device_payload_dir='/test/device/payload/dir')
      lab_xfer = auto_updater_transfer.LabTransfer

      self.PatchObject(auto_updater_transfer, '_STATEFUL_UPDATE_FILENAME',
                       'test_stateful_update')
      self.PatchObject(lab_xfer, '_EnsureDeviceDirectory')
      self.PatchObject(lab_xfer, '_GetCurlCmdForPayloadDownload')
      self.PatchObject(remote_access.ChromiumOSDevice, 'RunCommand')
      expected = [
          {'payload_dir': CrOS_LabTransfer._device_payload_dir,
           'payload_filename': auto_updater_transfer._STATEFUL_UPDATE_FILENAME},
          {'payload_dir': device.work_dir,
           'payload_filename': 'stateful.tgz',
           'image_name': ''}]

      CrOS_LabTransfer._TransferStatefulUpdate()
      self.assertListEqual(
          lab_xfer._GetCurlCmdForPayloadDownload.call_args_list,
          [mock.call(**x) for x in expected])

  def testTransferStatefulRunCmdCalls(self):
    """Test methods calls of LabTransfer._TransferStatefulUpdate().

    Test whether remote_access.ChromiumOSDevice.RunCommand() is being called
    the correct number of times with the correct arguments.
    """
    with remote_access.ChromiumOSDeviceHandler('1.1.1.1') as device:
      CrOS_LabTransfer = CreateLabTransferInstance(device)
      lab_xfer = auto_updater_transfer.LabTransfer
      expected = [
          ['curl', '-o', 'stateful_update',
           'http://0.0.0.0:8000/static/stateful_update'],
          ['curl', '-o', '/test/work/dir/stateful.tgz',
           'http://0.0.0.0:8000/static/stateful.tgz']]

      self.PatchObject(lab_xfer, '_EnsureDeviceDirectory')
      self.PatchObject(remote_access.ChromiumOSDevice, 'RunCommand')

      CrOS_LabTransfer._TransferStatefulUpdate()
      self.assertListEqual(
          remote_access.ChromiumOSDevice.RunCommand.call_args_list,
          [mock.call(x) for x in expected])

  def testTransferStatefulEnsureDirCalls(self):
    """Test methods calls of LabTransfer._TransferStatefulUpdate().

    Test whether LabTransfer._EnsureDeviceDirectory() is being called
    the correct number of times with the correct arguments.
    """
    with remote_access.ChromiumOSDeviceHandler('1.1.1.1') as device:
      CrOS_LabTransfer = CreateLabTransferInstance(
          device, device_payload_dir='/test/device/payload/dir')
      lab_xfer = auto_updater_transfer.LabTransfer
      expected = [CrOS_LabTransfer._device_payload_dir]

      self.PatchObject(lab_xfer, '_EnsureDeviceDirectory')
      self.PatchObject(remote_access.ChromiumOSDevice, 'RunCommand')

      CrOS_LabTransfer._TransferStatefulUpdate()
      self.assertListEqual(lab_xfer._EnsureDeviceDirectory.call_args_list,
                           [mock.call(x) for x in expected])

  def testTransferStatefulOriginalPayloadCurlCmdCalls(self):
    """Test methods calls of LabTransfer._TransferStatefulUpdate().

    Test whether LabTransfer._GetCurlCmdForPayloadDownload() is being called
    the correct number of times with the correct arguments when original
    payload directory exists.
    """
    with remote_access.ChromiumOSDeviceHandler('1.1.1.1') as device:
      CrOS_LabTransfer = CreateLabTransferInstance(
          device, original_payload_dir='/test/original/payload/dir',
          device_payload_dir='/test/device/payload/dir',
          device_restore_dir='/test/device/restore/dir',
          payload_dir='/test/payload/dir')
      lab_xfer = auto_updater_transfer.LabTransfer

      self.PatchObject(auto_updater_transfer, 'STATEFUL_FILENAME',
                       'test_stateful.tgz')
      self.PatchObject(auto_updater_transfer, '_STATEFUL_UPDATE_FILENAME',
                       'test_stateful_update')
      self.PatchObject(lab_xfer, '_EnsureDeviceDirectory')
      self.PatchObject(lab_xfer, '_GetCurlCmdForPayloadDownload')
      self.PatchObject(remote_access.ChromiumOSDevice, 'RunCommand')

      expected = [
          {'payload_dir': CrOS_LabTransfer._device_payload_dir,
           'payload_filename': auto_updater_transfer._STATEFUL_UPDATE_FILENAME},
          {'payload_dir': CrOS_LabTransfer._device_restore_dir,
           'payload_filename': auto_updater_transfer.STATEFUL_FILENAME,
           'image_name': CrOS_LabTransfer._original_payload_dir},
          {'payload_dir': device.work_dir,
           'payload_filename': auto_updater_transfer.STATEFUL_FILENAME,
           'image_name': CrOS_LabTransfer._payload_dir}]

      CrOS_LabTransfer._TransferStatefulUpdate()
      self.assertListEqual(
          lab_xfer._GetCurlCmdForPayloadDownload.call_args_list,
          [mock.call(**x) for x in expected])

  def testTransferStatefulOriginalPayloadRunCmdCalls(self):
    """Test methods calls of LabTransfer._TransferStatefulUpdate().

    Test whether remote_access.ChromiumOSDevice.RunCommand() is being called
    the correct number of times with the correct arguments when original
    payload directory exists.
    """
    with remote_access.ChromiumOSDeviceHandler('1.1.1.1') as device:
      CrOS_LabTransfer = CreateLabTransferInstance(
          device, original_payload_dir='/test/original/payload/dir')
      lab_xfer = auto_updater_transfer.LabTransfer
      expected = [
          ['curl', '-o', 'stateful_update',
           'http://0.0.0.0:8000/static/stateful_update'],
          ['curl', '-o', 'stateful.tgz',
           'http://0.0.0.0:8000/test/original/payload/dir/stateful.tgz'],
          ['curl', '-o', '/test/work/dir/stateful.tgz',
           'http://0.0.0.0:8000/static/stateful.tgz']]

      self.PatchObject(lab_xfer, '_EnsureDeviceDirectory')
      self.PatchObject(remote_access.ChromiumOSDevice, 'RunCommand')

      CrOS_LabTransfer._TransferStatefulUpdate()
      self.assertListEqual(
          remote_access.ChromiumOSDevice.RunCommand.call_args_list,
          [mock.call(x) for x in expected])


  def testTransferStatefulOriginalPayloadEnsureDirCalls(self):
    """Test methods calls of LabTransfer._TransferStatefulUpdate().

    Test whether LabTransfer._EnsureDeviceDirectory() is being called
    the correct number of times with the correct arguments when original
    payload directory exists.
    """
    with remote_access.ChromiumOSDeviceHandler('1.1.1.1') as device:
      CrOS_LabTransfer = CreateLabTransferInstance(
          device, original_payload_dir='/test/original/payload/dir',
          device_payload_dir='/test/device/payload/dir',
          device_restore_dir='/test/device/restore/dir')
      lab_xfer = auto_updater_transfer.LabTransfer
      expected = [CrOS_LabTransfer._device_payload_dir,
                  CrOS_LabTransfer._device_restore_dir]

      self.PatchObject(lab_xfer, '_EnsureDeviceDirectory')
      self.PatchObject(remote_access.ChromiumOSDevice, 'RunCommand')

      CrOS_LabTransfer._TransferStatefulUpdate()
      self.assertListEqual(lab_xfer._EnsureDeviceDirectory.call_args_list,
                           [mock.call(x) for x in expected])

  def testErrorTriggerRetryTransferRootfs(self):
    """Test LabTransfer._TransferRootfsUpdate() is retried properly."""
    with remote_access.ChromiumOSDeviceHandler('1.1.1.1') as device:
      CrOS_LabTransfer = CreateLabTransferInstance(device)
      self.PatchObject(auto_updater_transfer, '_DELAY_SEC_FOR_RETRY', 1)
      _MAX_RETRY = self.PatchObject(auto_updater_transfer, '_MAX_RETRY', 3)
      transfer_rootfs = self.PatchObject(
          auto_updater_transfer.LabTransfer,
          '_TransferRootfsUpdate',
          side_effect=cros_build_lib.RunCommandError('fail'))
      self.assertRaises(cros_build_lib.RunCommandError,
                        CrOS_LabTransfer.TransferRootfsUpdate)
      self.assertEqual(transfer_rootfs.call_count, _MAX_RETRY + 1)

  def testErrorTransferRootfsServerError(self):
    """Test errors thrown by LabTransfer._TransferRootfsUpdate()."""
    with remote_access.ChromiumOSDeviceHandler('1.1.1.1') as device:
      CrOS_LabTransfer = CreateLabTransferInstance(
          device, staging_server='http://wrong:server')
      self.PatchObject(auto_updater_transfer.LabTransfer,
                       '_EnsureDeviceDirectory')
      self.PatchObject(remote_access.ChromiumOSDevice, 'RunCommand',
                       side_effect=cros_build_lib.RunCommandError('fail'))
      self.assertRaises(cros_build_lib.RunCommandError,
                        CrOS_LabTransfer._TransferRootfsUpdate)

  def testTransferRootfsCurlCmdCalls(self):
    """Test method calls of LabTransfer._TransferRootfsUpdate().

    Test whether LabTransfer._GetCurlCmdForPayloadDownload() is being called
    the correct number of times with the correct arguments.
    """
    with remote_access.ChromiumOSDeviceHandler('1.1.1.1') as device:
      CrOS_LabTransfer = CreateLabTransferInstance(
          device, device_payload_dir='/test/device/payload/dir',
          payload_dir='test/payload/dir', payload_name='test_update.gz')
      lab_xfer = auto_updater_transfer.LabTransfer
      expected = [
          {'payload_dir': CrOS_LabTransfer._device_payload_dir,
           'payload_filename': CrOS_LabTransfer._payload_name,
           'image_name': CrOS_LabTransfer._payload_dir},
          {'payload_dir': CrOS_LabTransfer._device_payload_dir,
           'payload_filename': CrOS_LabTransfer._payload_name + '.json',
           'image_name': CrOS_LabTransfer._payload_dir}]

      self.PatchObject(lab_xfer, '_EnsureDeviceDirectory')
      self.PatchObject(lab_xfer, '_GetCurlCmdForPayloadDownload')
      self.PatchObject(remote_access.ChromiumOSDevice, 'RunCommand')

      CrOS_LabTransfer._TransferRootfsUpdate()

      self.assertListEqual(
          lab_xfer._GetCurlCmdForPayloadDownload.call_args_list,
          [mock.call(**x) for x in expected])

  def testTransferRootfsRunCmdCalls(self):
    """Test method calls of LabTransfer._TransferRootfsUpdate().

    Test whether remote_access.ChromiumOSDevice.RunCommand() is being called
    the correct number of times with the correct arguments.
    """
    with remote_access.ChromiumOSDeviceHandler('1.1.1.1') as device:
      CrOS_LabTransfer = CreateLabTransferInstance(
          device, payload_name='test_update.gz')
      lab_xfer = auto_updater_transfer.LabTransfer
      expected = [
          ['curl', '-o', 'test_update.gz',
           'http://0.0.0.0:8000/static/test_update.gz'],
          ['curl', '-o', 'test_update.gz.json',
           'http://0.0.0.0:8000/static/test_update.gz.json']]

      self.PatchObject(lab_xfer, '_EnsureDeviceDirectory')
      self.PatchObject(remote_access.ChromiumOSDevice, 'RunCommand')

      CrOS_LabTransfer._TransferRootfsUpdate()
      self.assertListEqual(
          remote_access.ChromiumOSDevice.RunCommand.call_args_list,
          [mock.call(x) for x in expected])

  def testTransferRootfsEnsureDirCalls(self):
    """Test method calls of LabTransfer._TransferRootfsUpdate().

    Test whether LabTransfer._EnsureDeviceDirectory() is being called
    the correct number of times with the correct arguments.
    """
    with remote_access.ChromiumOSDeviceHandler('1.1.1.1') as device:
      CrOS_LabTransfer = CreateLabTransferInstance(
          device, device_payload_dir='/test/device/payload/dir')
      lab_xfer = auto_updater_transfer.LabTransfer
      expected = [CrOS_LabTransfer._device_payload_dir]

      self.PatchObject(lab_xfer, '_EnsureDeviceDirectory')
      self.PatchObject(remote_access.ChromiumOSDevice, 'RunCommand')

      CrOS_LabTransfer._TransferRootfsUpdate()
      self.assertListEqual(lab_xfer._EnsureDeviceDirectory.call_args_list,
                           [mock.call(x) for x in expected])

  def testLabTransferGetCurlCmdStandard(self):
    """Test auto_updater_transfer._GetCurlCmdForPayloadDownload.

    Tests the typical usage of the _GetCurlCmdForPayloadDownload() method.
    """
    with remote_access.ChromiumOSDeviceHandler('1.1.1.1') as device:
      CrOS_LabTransfer = CreateLabTransferInstance(device)
      expected_cmd = ['curl', '-o',
                      '/tmp/test_payload_dir/payload_filename.ext',
                      'http://0.0.0.0:8000/static/board-release/12345.0.0/'
                      'payload_filename.ext']
      cmd = CrOS_LabTransfer._GetCurlCmdForPayloadDownload(
          payload_dir='/tmp/test_payload_dir',
          payload_filename='payload_filename.ext',
          image_name='board-release/12345.0.0/')
      self.assertEqual(cmd, expected_cmd)

  def testLabTransferGetCurlCmdNoImageName(self):
    """Test auto_updater_transfer._GetCurlCmdForPayloadDownload.

    Tests when the payload file is available in the static directory on the
    staging server.
    """
    with remote_access.ChromiumOSDeviceHandler('1.1.1.1') as device:
      CrOS_LabTransfer = CreateLabTransferInstance(device)
      expected_cmd = ['curl', '-o',
                      '/tmp/test_payload_dir/payload_filename.ext',
                      'http://0.0.0.0:8000/static/payload_filename.ext']
      cmd = CrOS_LabTransfer._GetCurlCmdForPayloadDownload(
          payload_dir='/tmp/test_payload_dir',
          payload_filename='payload_filename.ext')
      self.assertEqual(cmd, expected_cmd)
