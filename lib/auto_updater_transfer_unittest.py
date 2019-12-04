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
from chromite.lib import retry_util


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
                  'device_restore_dir': '', 'tempdir': '',
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


class CrosTransferBaseClassTest(cros_test_lib.MockTestCase):
  """Test whether Transfer's public transfer functions are retried correctly."""

  def testErrorTriggerRetryTransferUpdateUtils(self):
    """Test if Transfer._TransferUpdateUtilsPackage() is retried properly."""
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
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
    """Test if Transfer._TransferStatefulUpdate() is retried properly."""
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
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
    """Test if Transfer._TransferRootfsUpdate() is retried properly."""
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
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


class CrosLocalTransferTest(cros_test_lib.MockTempDirTestCase):
  """Test whether LocalTransfer's transfer functions are retried."""

  def setUp(self):
    """Mock remote_access.RemoteDevice's functions for update."""
    self.PatchObject(remote_access.RemoteDevice, 'work_dir', '/test/work/dir')
    self.PatchObject(remote_access.RemoteDevice, 'CopyToWorkDir')
    self.PatchObject(remote_access.RemoteDevice, 'CopyToDevice')

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
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
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
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_LocalTransfer = CreateLocalTransferInstance(
          device, original_payload_dir='test_dir')
      CrOS_LocalTransfer._TransferStatefulUpdate()
      self.assertTrue(
          auto_updater_transfer.LocalTransfer._GetStatefulUpdateScript.called)
      self.assertEqual(CrOS_LocalTransfer._stateful_update_bin,
                       os.path.join(device.work_dir, 'stateful_update'))
      self.assertTrue(
          auto_updater_transfer.LocalTransfer._EnsureDeviceDirectory.called)

  def testLocalTransferCheckPayloadsError(self):
    """Test auto_updater_transfer.CheckPayloads with raising exception.

    auto_updater_transfer.ChromiumOSTransferError is raised if it does not find
    payloads in its path.
    """
    self.PatchObject(os.path, 'exists', return_value=False)
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
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
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_LocalTransfer = CreateLocalTransferInstance(device,
                                                       payload_name='exists')
      CrOS_LocalTransfer.CheckPayloads()

  def testGetStatefulUpdateScriptLocalChroot(self):
    """Test auto_updater_transfer.LocalTransfer._GetStatefulUpdateScript().

    Test if method returns correct responses when os.path.exists() is set to
    return True for the path returned by the path_util.FromChrootPath() call.
    """
    self.PatchObject(path_util, 'FromChrootPath', return_value='test_path')
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
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
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
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
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_LocalTransfer = CreateLocalTransferInstance(device)
      with self.PatchObject(os.path, 'exists', side_effect=[False, False]):
        self.assertEqual(CrOS_LocalTransfer._GetStatefulUpdateScript(),
                         (False, '/usr/local/bin/stateful_update'))

  def testGetPayloadPropsLocal(self):
    """Tests LocalTransfer.GetPayloadProps().

    Tests LocalTransfer.GetPayloadProps() when payload_name is in the
    format payloads/chromeos_xxxx.0.0_<blah>.
    """
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_LocalTransfer = CreateLocalTransferInstance(
          device, payload_dir=self.tempdir,
          payload_name='payloads/chromeos_12345.100.0_board_stable_'
                       'full_v4-1a3e3fd5a2005948ce8e605b66c96b2f.bin')
      self.PatchObject(os.path, 'getsize', return_value=100)
      expected = {'image_version': '12345.100.0', 'size': 100}
      self.assertDictEqual(CrOS_LocalTransfer.GetPayloadProps(),
                           expected)

  def testGetPayloadPropsLocalError(self):
    """Tests error thrown by LocalTransfer.GetPayloadProps().

    Test error thrown when payload_name is not in the expected format of
    payloads/chromeos_xxxx.0.0_<blah> or called update.gz.
    """
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_LocalTransfer = CreateLocalTransferInstance(
          device, payload_dir=self.tempdir, payload_name='wrong_format')
      self.PatchObject(os.path, 'getsize')
      self.assertRaises(ValueError, CrOS_LocalTransfer.GetPayloadProps)

  def testGetPayloadPropsLocalDefaultFilename(self):
    """Tests LocalTransfer.GetPayloadProps().

    Tests LocalTransfer.GetPayloadProps() when payload_name is named
    update.gz.
    """
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_LocalTransfer = CreateLocalTransferInstance(
          device, payload_dir=self.tempdir, payload_name='update.gz')
      self.PatchObject(os.path, 'getsize', return_value=101)
      expected = {'image_version': '99999.0.0', 'size': 101}
      self.assertDictEqual(CrOS_LocalTransfer.GetPayloadProps(),
                           expected)

class CrosLabTransferTest(cros_test_lib.MockTempDirTestCase):
  """Test all methods in auto_updater_transfer.LabTransfer."""

  def setUp(self):
    """Mock remote_access.RemoteDevice/ChromiumOSDevice functions for update."""
    self.PatchObject(remote_access.RemoteDevice, 'work_dir', '/test/work/dir')

  def testTransferUpdateUtilsCurlCalls(self):
    """Test methods calls of LabTransfer._TransferUpdateUtilsPackage().

    Test whether LabTransfer._GetCurlCmdForPayloadDownload() is being called
    the correct number of times with the correct arguments.
    """
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
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
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
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
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
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
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_LabTransfer = CreateLabTransferInstance(
          device, staging_server='http://wrong:server')
      self.PatchObject(auto_updater_transfer.LabTransfer,
                       '_EnsureDeviceDirectory')
      self.PatchObject(remote_access.ChromiumOSDevice, 'RunCommand',
                       side_effect=cros_build_lib.RunCommandError('fail'))
      self.assertRaises(cros_build_lib.RunCommandError,
                        CrOS_LabTransfer._TransferUpdateUtilsPackage)

  def testErrorTransferStatefulServerError(self):
    """Test errors thrown by LabTransfer._TransferStatefulUpdate()."""
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
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
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
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
           'build_id': ''}]

      CrOS_LabTransfer._TransferStatefulUpdate()
      self.assertListEqual(
          lab_xfer._GetCurlCmdForPayloadDownload.call_args_list,
          [mock.call(**x) for x in expected])

  def testTransferStatefulRunCmdCalls(self):
    """Test methods calls of LabTransfer._TransferStatefulUpdate().

    Test whether remote_access.ChromiumOSDevice.RunCommand() is being called
    the correct number of times with the correct arguments.
    """
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
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
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
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
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
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
           'build_id': CrOS_LabTransfer._original_payload_dir},
          {'payload_dir': device.work_dir,
           'payload_filename': auto_updater_transfer.STATEFUL_FILENAME,
           'build_id': CrOS_LabTransfer._payload_dir}]

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
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
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
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
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

  def testErrorTransferRootfsServerError(self):
    """Test errors thrown by LabTransfer._TransferRootfsUpdate()."""
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
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
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_LabTransfer = CreateLabTransferInstance(
          device, device_payload_dir='/test/device/payload/dir',
          payload_dir='test/payload/dir', payload_name='test_update.gz')
      lab_xfer = auto_updater_transfer.LabTransfer
      expected = [
          {'payload_dir': CrOS_LabTransfer._device_payload_dir,
           'payload_filename': CrOS_LabTransfer._payload_name,
           'build_id': CrOS_LabTransfer._payload_dir}]

      self.PatchObject(lab_xfer, '_EnsureDeviceDirectory')
      self.PatchObject(lab_xfer, '_GetCurlCmdForPayloadDownload')
      self.PatchObject(remote_access.ChromiumOSDevice, 'RunCommand')
      self.PatchObject(remote_access.ChromiumOSDevice, 'CopyToWorkDir')

      CrOS_LabTransfer._TransferRootfsUpdate()

      self.assertListEqual(
          lab_xfer._GetCurlCmdForPayloadDownload.call_args_list,
          [mock.call(**x) for x in expected])

  def testTransferRootfsRunCmdCalls(self):
    """Test method calls of LabTransfer._TransferRootfsUpdate().

    Test whether remote_access.ChromiumOSDevice.RunCommand() is being called
    the correct number of times with the correct arguments.
    """
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_LabTransfer = CreateLabTransferInstance(
          device, payload_name='test_update.gz')
      lab_xfer = auto_updater_transfer.LabTransfer
      expected = [['curl', '-o', 'test_update.gz',
                   'http://0.0.0.0:8000/static/test_update.gz']]

      self.PatchObject(lab_xfer, '_EnsureDeviceDirectory')
      self.PatchObject(remote_access.ChromiumOSDevice, 'RunCommand')
      self.PatchObject(remote_access.ChromiumOSDevice, 'CopyToWorkDir')

      CrOS_LabTransfer._TransferRootfsUpdate()
      self.assertListEqual(
          remote_access.ChromiumOSDevice.RunCommand.call_args_list,
          [mock.call(x) for x in expected])

  def testTransferRootfsRunCmdLocalPayloadProps(self):
    """Test method calls of LabTransfer._TransferRootfsUpdate().

    Test whether remote_access.ChromiumOSDevice.RunCommand() is being called
    the correct number of times with the correct arguments when
    LabTransfer.LocalPayloadPropsFile is set to a valid local filepath.
    """
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_LabTransfer = CreateLabTransferInstance(
          device, payload_name='test_update.gz')

      self.PatchObject(os.path, 'isfile', return_value=True)
      CrOS_LabTransfer.LocalPayloadPropsFile = '/existent/test.gz.json'

      lab_xfer = auto_updater_transfer.LabTransfer
      expected = [
          ['curl', '-o', 'test_update.gz',
           'http://0.0.0.0:8000/static/test_update.gz']]

      self.PatchObject(lab_xfer, '_EnsureDeviceDirectory')
      self.PatchObject(remote_access.ChromiumOSDevice, 'RunCommand')
      self.PatchObject(remote_access.ChromiumOSDevice, 'CopyToWorkDir')

      CrOS_LabTransfer._TransferRootfsUpdate()
      self.assertListEqual(
          remote_access.ChromiumOSDevice.RunCommand.call_args_list,
          [mock.call(x) for x in expected])

  def testTransferRootfsEnsureDirCalls(self):
    """Test method calls of LabTransfer._TransferRootfsUpdate().

    Test whether LabTransfer._EnsureDeviceDirectory() is being called
    the correct number of times with the correct arguments.
    """
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_LabTransfer = CreateLabTransferInstance(
          device, device_payload_dir='/test/device/payload/dir')
      lab_xfer = auto_updater_transfer.LabTransfer
      expected = [CrOS_LabTransfer._device_payload_dir]

      self.PatchObject(lab_xfer, '_EnsureDeviceDirectory')
      self.PatchObject(remote_access.ChromiumOSDevice, 'RunCommand')
      self.PatchObject(remote_access.ChromiumOSDevice, 'CopyToWorkDir')

      CrOS_LabTransfer._TransferRootfsUpdate()
      self.assertListEqual(lab_xfer._EnsureDeviceDirectory.call_args_list,
                           [mock.call(x) for x in expected])

  def testTransferRootfsCopyToWorkdirLocalPayloadProps(self):
    """Test method calls of LabTransfer._TransferRootfsUpdate().

    Test whether LabTransfer.device.CopyToWorkDir() is being called
    the correct number of times with the correct arguments when
    LabTransfer.LocalPayloadPropsFile is set to a valid local filepath.
    """
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_LabTransfer = CreateLabTransferInstance(device,
                                                   cmd_kwargs={'test': 'test'})

      CrOS_LabTransfer._local_payload_props_path = '/existent/test.gz.json'

      lab_xfer = auto_updater_transfer.LabTransfer
      expected = [{'src': CrOS_LabTransfer._local_payload_props_path,
                   'dest': CrOS_LabTransfer.PAYLOAD_DIR_NAME,
                   'mode': CrOS_LabTransfer._payload_mode,
                   'log_output': True, 'test': 'test'}]

      self.PatchObject(lab_xfer, '_EnsureDeviceDirectory')
      self.PatchObject(remote_access.ChromiumOSDevice, 'RunCommand')
      self.PatchObject(remote_access.ChromiumOSDevice, 'CopyToWorkDir')

      CrOS_LabTransfer._TransferRootfsUpdate()
      self.assertListEqual(
          remote_access.ChromiumOSDevice.CopyToWorkDir.call_args_list,
          [mock.call(**x) for x in expected])

  def testLabTransferGetCurlCmdStandard(self):
    """Test auto_updater_transfer._GetCurlCmdForPayloadDownload.

    Tests the typical usage of the _GetCurlCmdForPayloadDownload() method.
    """
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_LabTransfer = CreateLabTransferInstance(device)
      expected_cmd = ['curl', '-o',
                      '/tmp/test_payload_dir/payload_filename.ext',
                      'http://0.0.0.0:8000/static/board-release/12345.0.0/'
                      'payload_filename.ext']
      cmd = CrOS_LabTransfer._GetCurlCmdForPayloadDownload(
          payload_dir='/tmp/test_payload_dir',
          payload_filename='payload_filename.ext',
          build_id='board-release/12345.0.0/')
      self.assertEqual(cmd, expected_cmd)

  def testLabTransferGetCurlCmdNoImageName(self):
    """Test auto_updater_transfer._GetCurlCmdForPayloadDownload.

    Tests when the payload file is available in the static directory on the
    staging server.
    """
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_LabTransfer = CreateLabTransferInstance(device)
      expected_cmd = ['curl', '-o',
                      '/tmp/test_payload_dir/payload_filename.ext',
                      'http://0.0.0.0:8000/static/payload_filename.ext']
      cmd = CrOS_LabTransfer._GetCurlCmdForPayloadDownload(
          payload_dir='/tmp/test_payload_dir',
          payload_filename='payload_filename.ext')
      self.assertEqual(cmd, expected_cmd)

  def testCheckPayloadsAllIn(self):
    """Test auto_updater_transfer.CheckPayloads.

    Test CheckPayloads() method when transfer_rootfs_update and
    transfer_stateful_update are both set to True.
    """
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_LabTransfer = CreateLabTransferInstance(
          device, payload_name='test_update.gz',
          payload_dir='board-release/12345.6.7')
      self.PatchObject(auto_updater_transfer, 'STATEFUL_FILENAME',
                       'test_stateful.tgz')
      self.PatchObject(retry_util, 'RunCurl')

      expected = [
          {'curl_args': ['-I', 'http://0.0.0.0:8000/static/board-release/'
                               '12345.6.7/test_update.gz', '--fail'],
           'log_output': True},
          {'curl_args': ['-I', 'http://0.0.0.0:8000/static/board-release/'
                               '12345.6.7/test_update.gz.json', '--fail'],
           'log_output': True},
          {'curl_args': ['-I', 'http://0.0.0.0:8000/static/board-release/'
                               '12345.6.7/test_stateful.tgz', '--fail'],
           'log_output': True}]

      CrOS_LabTransfer.CheckPayloads()
      self.assertListEqual(retry_util.RunCurl.call_args_list,
                           [mock.call(**x) for x in expected])

  def testCheckPayloadsNoStatefulTransfer(self):
    """Test auto_updater_transfer.CheckPayloads.

    Test CheckPayloads() method when transfer_rootfs_update is True and
    transfer_stateful_update is set to False.
    """
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_LabTransfer = CreateLabTransferInstance(
          device, payload_name='test_update.gz',
          payload_dir='board-release/12345.6.7',
          transfer_stateful_update=False)
      self.PatchObject(retry_util, 'RunCurl')

      expected = [
          {'curl_args': ['-I', 'http://0.0.0.0:8000/static/board-release/'
                               '12345.6.7/test_update.gz', '--fail'],
           'log_output': True},
          {'curl_args': ['-I', 'http://0.0.0.0:8000/static/board-release/'
                               '12345.6.7/test_update.gz.json', '--fail'],
           'log_output': True}]

      CrOS_LabTransfer.CheckPayloads()
      self.assertListEqual(retry_util.RunCurl.call_args_list,
                           [mock.call(**x) for x in expected])

  def testCheckPayloadsNoRootfsTransfer(self):
    """Test auto_updater_transfer.CheckPayloads.

    Test CheckPayloads() method when transfer_rootfs_update is False and
    transfer_stateful_update is set to True.
    """
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_LabTransfer = CreateLabTransferInstance(
          device, payload_dir='board-release/12345.6.7',
          transfer_rootfs_update=False)
      self.PatchObject(auto_updater_transfer, 'STATEFUL_FILENAME',
                       'test_stateful.tgz')
      self.PatchObject(retry_util, 'RunCurl')

      expected = [
          {'curl_args': ['-I', 'http://0.0.0.0:8000/static/board-release/'
                               '12345.6.7/test_stateful.tgz', '--fail'],
           'log_output': True}]

      CrOS_LabTransfer.CheckPayloads()
      self.assertListEqual(retry_util.RunCurl.call_args_list,
                           [mock.call(**x) for x in expected])

  def testCheckPayloadsDownloadError(self):
    """Test auto_updater_transfer.CheckPayloads.

    Test CheckPayloads() for exceptions raised when payloads are not available
    on the staging server.
    """
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_LabTransfer = CreateLabTransferInstance(device)

      self.PatchObject(retry_util, 'RunCurl',
                       side_effect=retry_util.DownloadError())

      self.assertRaises(auto_updater_transfer.ChromiumOSTransferError,
                        CrOS_LabTransfer.CheckPayloads)

  def testLabTransferGetPayloadUrlStandard(self):
    """Test auto_updater_transfer._GetStagedUrl.

    Tests the typical usage of the _GetStagedUrl() method.
    """
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_LabTransfer = CreateLabTransferInstance(device)
      expected_url = ('http://0.0.0.0:8000/static/board-release/12345.0.0/'
                      'payload_filename.ext')
      url = CrOS_LabTransfer._GetStagedUrl(
          staged_filename='payload_filename.ext',
          build_id='board-release/12345.0.0/')
      self.assertEqual(url, expected_url)

  def testLabTransferGetPayloadUrlNoImageName(self):
    """Test auto_updater_transfer._GetStagedUrl.

    Tests when the build_id parameter is defaulted to None.
    """
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_LabTransfer = CreateLabTransferInstance(device)
      expected_url = 'http://0.0.0.0:8000/static/payload_filename.ext'
      url = CrOS_LabTransfer._GetStagedUrl(
          staged_filename='payload_filename.ext')
      self.assertEqual(url, expected_url)

  def testGetPayloadPropsFile(self):
    """Test LabTransfer.GetPayloadPropsFile()."""
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_LabTransfer = CreateLabTransferInstance(
          device, tempdir=self.tempdir, payload_name='test_update.gz')
      self.PatchObject(retry_util, 'RunCurl')
      self.PatchObject(os.path, 'isfile', return_value=True)
      CrOS_LabTransfer.GetPayloadPropsFile()
      self.assertEqual(CrOS_LabTransfer._local_payload_props_path,
                       os.path.join(self.tempdir, 'test_update.gz.json'))

  def testGetPayloadPropsFileError(self):
    """Test LabTransfer.GetPayloadPropsFile().

    Test when the GetPayloadPropsFile() method throws an error.
    """
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_LabTransfer = CreateLabTransferInstance(
          device, tempdir=self.tempdir, payload_name='test_update.gz')
      self.PatchObject(retry_util, 'RunCurl',
                       side_effect=retry_util.DownloadError())
      self.assertRaises(auto_updater_transfer.ChromiumOSTransferError,
                        CrOS_LabTransfer.GetPayloadPropsFile)
      self.assertIsNone(CrOS_LabTransfer._local_payload_props_path)

  def testGetPayloadSize(self):
    """Test auto_updater_transfer._GetPayloadSize()."""
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_LabTransfer = CreateLabTransferInstance(
          device, payload_name='test_update.gz',
          payload_dir='/test/payload/dir')
      expected_size = 75810234
      output = 'Content-Length: %s' % str(expected_size)
      self.PatchObject(retry_util, 'RunCurl',
                       return_value=cros_build_lib.CommandResult(output=output))
      size = CrOS_LabTransfer._GetPayloadSize()
      self.assertEqual(size, expected_size)

  def testGetPayloadSizeNoRegexMatchError(self):
    """Test errors thrown by auto_updater_transfer._GetPayloadSize().

    Test error thrown when the output received from the curl command does not
    contain expected fields.
    """
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_LabTransfer = CreateLabTransferInstance(
          device, payload_name='test_update.gz',
          payload_dir='/test/payload/dir')
      output = 'No Match Output'
      self.PatchObject(retry_util, 'RunCurl',
                       return_value=cros_build_lib.CommandResult(output=output))
      self.assertRaises(auto_updater_transfer.ChromiumOSTransferError,
                        CrOS_LabTransfer._GetPayloadSize)

  def testGetPayloadSizeCurlRunCommandError(self):
    """Test errors thrown by auto_updater_transfer.GetPayloadSize().

    Test error thrown when cros_build_lib.run raises a RunCommandError.
    """
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_LabTransfer = CreateLabTransferInstance(
          device, payload_name='test_update.gz',
          payload_dir='/test/payload/dir')
      self.PatchObject(retry_util, 'RunCurl',
                       side_effect=cros_build_lib.RunCommandError(msg=''))
      self.assertRaises(auto_updater_transfer.ChromiumOSTransferError,
                        CrOS_LabTransfer._GetPayloadSize)

  def testGetPayloadPropsLab(self):
    """Test LabTransfer.GetPayloadProps()."""
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_LabTransfer = CreateLabTransferInstance(
          device, payload_dir='test-board-name/R77-12345.0.0')
      self.PatchObject(auto_updater_transfer.LabTransfer, '_GetPayloadSize',
                       return_value=123)
      expected = {'image_version': '12345.0.0', 'size': 123}
      self.assertDictEqual(CrOS_LabTransfer.GetPayloadProps(), expected)

  def testGetPayloadPropsLabError(self):
    """Test error thrown by LabTransfer.GetPayloadProps().

    Test error thrown when payload_dir is not in the expected format of
    <board-name>/Rxx-12345.6.7.
    """
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_LabTransfer = CreateLabTransferInstance(
          device, payload_dir='/wrong/format/will/fail')
      self.PatchObject(auto_updater_transfer.LabTransfer, '_GetPayloadSize')
      self.assertRaises(ValueError, CrOS_LabTransfer.GetPayloadProps)
