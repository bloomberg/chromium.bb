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

import copy
import os

import mock
import six

from chromite.lib import auto_updater_transfer
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import partial_mock
from chromite.lib import remote_access
from chromite.lib import retry_util


_DEFAULT_ARGS = {
    'payload_dir': None, 'device_payload_dir': None, 'tempdir': None,
    'payload_name': None, 'cmd_kwargs': None, 'device_restore_dir': None,
}


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
  default_args = copy.deepcopy(_DEFAULT_ARGS)

  default_args.update(kwargs)
  return auto_updater_transfer.LocalTransfer(device=device, **default_args)


def CreateLabTransferInstance(device, **kwargs):
  """Create auto_updater_transfer.LabTransfer instance.

  Args:
    device: a remote_access.ChromiumOSDeviceHandler object.
    kwargs: contains parameter name and value pairs for any argument accepted
      by auto_updater_transfer.LabTransfer. The values provided through
      kwargs will supersede the defaults set within this function.

  Returns:
    An instance of auto_updater_transfer.LabTransfer.
  """
  default_args = copy.deepcopy(_DEFAULT_ARGS)
  default_args['staging_server'] = 'http://0.0.0.0:8000'

  default_args.update(kwargs)
  return auto_updater_transfer.LabTransfer(device=device, **default_args)


def CreateLabEndToEndPayloadTransferInstance(device, **kwargs):
  """Create auto_updater_transfer.LabEndToEndPayloadTransfer instance.

  Args:
    device: a remote_access.ChromiumOSDeviceHandler object.
    kwargs: contains parameter name and value pairs for any argument accepted
      by auto_updater_transfer.LabEndToEndPayloadTransfer. The values provided
      through kwargs will supersede the defaults set within this function.

  Returns:
    An instance of auto_updater_transfer.LabEndToEndPayloadTransfer.
  """
  default_args = copy.deepcopy(_DEFAULT_ARGS)
  default_args['staging_server'] = 'http://0.0.0.0:8000'

  default_args.update(kwargs)
  return auto_updater_transfer.LabEndToEndPayloadTransfer(device=device,
                                                          **default_args)


class CrOSLocalTransferPrivateMock(partial_mock.PartialCmdMock):
  """Mock out all transfer functions in auto_updater_transfer.LocalTransfer."""
  TARGET = 'chromite.lib.auto_updater_transfer.LocalTransfer'
  ATTRS = ('_TransferStatefulUpdate', '_TransferRootfsUpdate',
           '_TransferUpdateUtilsPackage', '_EnsureDeviceDirectory')

  def __init__(self):
    partial_mock.PartialCmdMock.__init__(self)

  def _TransferStatefulUpdate(self, _inst, *_args, **_kwargs):
    """Mock auto_updater_transfer.LocalTransfer._TransferStatefulUpdate."""

  def _TransferRootfsUpdate(self, _inst, *_args, **_kwargs):
    """Mock auto_updater_transfer.LocalTransfer._TransferRootfsUpdate."""

  def _TransferUpdateUtilsPackage(self, _inst, *_args, **_kwargs):
    """Mock auto_updater_transfer.LocalTransfer._TransferUpdateUtilsPackage."""

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

    Test whether auto_updater_transfer.LocalTransfer._EnsureDeviceDirectory()
    are being called correctly.
    """
    self.PatchObject(auto_updater_transfer.LocalTransfer,
                     '_EnsureDeviceDirectory')
    self.PatchObject(auto_updater_transfer, 'STATEFUL_FILENAME',
                     'test_stateful.tgz')
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_LocalTransfer = CreateLocalTransferInstance(
          device, cmd_kwargs={'test': 'args'}, payload_dir='/test/payload/dir')
      CrOS_LocalTransfer._TransferStatefulUpdate()
      self.assertFalse(
          auto_updater_transfer.LocalTransfer._EnsureDeviceDirectory.called)

  def testLocalTransferCheckPayloadsError(self):
    """Test auto_updater_transfer.CheckPayloads with raising exception.

    auto_updater_transfer.ChromiumOSTransferError is raised if it does not find
    payloads in its path.
    """
    self.PatchObject(os.path, 'exists', return_value=False)
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_LocalTransfer = CreateLocalTransferInstance(
          device, payload_name='does_not_exist',
          payload_dir='/test/payload/dir')
      self.assertRaises(
          auto_updater_transfer.ChromiumOSTransferError,
          CrOS_LocalTransfer.CheckPayloads)

  def testLocalTransferCheckPayloads(self):
    """Test auto_updater_transfer.CheckPayloads without raising exception.

    Test will fail if ChromiumOSTransferError is raised when payload exists.
    """
    self.PatchObject(os.path, 'exists', return_value=True)
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_LocalTransfer = CreateLocalTransferInstance(
          device, payload_name='exists', payload_dir='/test/payload/dir')
      CrOS_LocalTransfer.CheckPayloads()

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
      self.PatchObject(remote_access.ChromiumOSDevice, 'run')

      CrOS_LabTransfer._TransferUpdateUtilsPackage()
      self.assertListEqual(
          lab_xfer._GetCurlCmdForPayloadDownload.call_args_list,
          [mock.call(**x) for x in expected])

  def testTransferUpdateUtilsRunCmdCalls(self):
    """Test methods calls of LabTransfer._TransferUpdateUtilsPackage().

    Test whether remote_access.ChromiumOSDevice.run() is being called
    the correct number of times with the correct arguments.
    """
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_LabTransfer = CreateLabTransferInstance(device)
      lab_xfer = auto_updater_transfer.LabTransfer
      expected = [['curl', '-o', '/test/work/dir/src/nebraska.py',
                   'http://0.0.0.0:8000/static/nebraska.py']]

      self.PatchObject(lab_xfer, '_EnsureDeviceDirectory')
      self.PatchObject(remote_access.ChromiumOSDevice, 'run')

      CrOS_LabTransfer._TransferUpdateUtilsPackage()
      self.assertListEqual(
          remote_access.ChromiumOSDevice.run.call_args_list,
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
      self.PatchObject(remote_access.ChromiumOSDevice, 'run')

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
      self.PatchObject(remote_access.ChromiumOSDevice, 'run',
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
      self.PatchObject(remote_access.ChromiumOSDevice, 'run',
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
          device, device_payload_dir='/test/device/payload/dir',
          payload_dir='/test/payload/dir')
      lab_xfer = auto_updater_transfer.LabTransfer

      self.PatchObject(lab_xfer, '_EnsureDeviceDirectory')
      self.PatchObject(lab_xfer, '_GetCurlCmdForPayloadDownload')
      self.PatchObject(remote_access.ChromiumOSDevice, 'run')
      expected = [
          {'payload_dir': device.work_dir,
           'payload_filename': 'stateful.tgz',
           'build_id': '/test/payload/dir'}]

      CrOS_LabTransfer._TransferStatefulUpdate()
      self.assertListEqual(
          lab_xfer._GetCurlCmdForPayloadDownload.call_args_list,
          [mock.call(**x) for x in expected])

  def testTransferStatefulRunCmdCalls(self):
    """Test methods calls of LabTransfer._TransferStatefulUpdate().

    Test whether remote_access.ChromiumOSDevice.run() is being called
    the correct number of times with the correct arguments.
    """
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_LabTransfer = CreateLabTransferInstance(device)
      lab_xfer = auto_updater_transfer.LabTransfer
      expected = [
          ['curl', '-o', '/test/work/dir/stateful.tgz',
           'http://0.0.0.0:8000/static/stateful.tgz']]

      self.PatchObject(lab_xfer, '_EnsureDeviceDirectory')
      self.PatchObject(remote_access.ChromiumOSDevice, 'run')

      CrOS_LabTransfer._TransferStatefulUpdate()
      self.assertListEqual(
          remote_access.ChromiumOSDevice.run.call_args_list,
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
      self.PatchObject(remote_access.ChromiumOSDevice, 'run')

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
      self.PatchObject(lab_xfer, '_EnsureDeviceDirectory')
      self.PatchObject(lab_xfer, '_GetCurlCmdForPayloadDownload')
      self.PatchObject(remote_access.ChromiumOSDevice, 'run')

      expected = [
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

    Test whether remote_access.ChromiumOSDevice.run() is being called
    the correct number of times with the correct arguments when original
    payload directory exists.
    """
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_LabTransfer = CreateLabTransferInstance(
          device, original_payload_dir='/test/original/payload/dir',
          device_restore_dir='/test/device/restore/dir')
      lab_xfer = auto_updater_transfer.LabTransfer
      expected = [
          ['curl', '-o', '/test/device/restore/dir/stateful.tgz',
           'http://0.0.0.0:8000/test/original/payload/dir/stateful.tgz'],
          ['curl', '-o', '/test/work/dir/stateful.tgz',
           'http://0.0.0.0:8000/static/stateful.tgz']]

      self.PatchObject(lab_xfer, '_EnsureDeviceDirectory')
      self.PatchObject(remote_access.ChromiumOSDevice, 'run')

      CrOS_LabTransfer._TransferStatefulUpdate()
      self.assertListEqual(
          remote_access.ChromiumOSDevice.run.call_args_list,
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
      self.PatchObject(remote_access.ChromiumOSDevice, 'run')

      CrOS_LabTransfer._TransferStatefulUpdate()
      self.assertListEqual(lab_xfer._EnsureDeviceDirectory.call_args_list,
                           [mock.call(x) for x in expected])

  def testErrorTransferRootfsServerError(self):
    """Test errors thrown by LabTransfer._TransferRootfsUpdate()."""
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_LabTransfer = CreateLabTransferInstance(
          device, staging_server='http://wrong:server',
          device_payload_dir='/test/device/payload/dir',
          payload_name='test_update.gz')
      self.PatchObject(auto_updater_transfer.LabTransfer,
                       '_EnsureDeviceDirectory')
      self.PatchObject(remote_access.ChromiumOSDevice, 'run',
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
          payload_dir='test/payload/dir', payload_name='test_update.gz',
          cmd_kwargs={'test': 'args'})
      lab_xfer = auto_updater_transfer.LabTransfer
      expected = [
          {'payload_dir': CrOS_LabTransfer._device_payload_dir,
           'payload_filename': CrOS_LabTransfer._payload_name,
           'build_id': CrOS_LabTransfer._payload_dir}]

      self.PatchObject(lab_xfer, '_EnsureDeviceDirectory')
      self.PatchObject(lab_xfer, '_GetCurlCmdForPayloadDownload')
      self.PatchObject(remote_access.ChromiumOSDevice, 'run')
      self.PatchObject(remote_access.ChromiumOSDevice, 'CopyToWorkDir')

      CrOS_LabTransfer._TransferRootfsUpdate()

      self.assertListEqual(
          lab_xfer._GetCurlCmdForPayloadDownload.call_args_list,
          [mock.call(**x) for x in expected])

  def testTransferRootfsRunCmdCalls(self):
    """Test method calls of LabTransfer._TransferRootfsUpdate().

    Test whether remote_access.ChromiumOSDevice.run() is being called
    the correct number of times with the correct arguments.
    """
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_LabTransfer = CreateLabTransferInstance(
          device, payload_name='test_update.gz',
          device_payload_dir='/test/device/payload/dir',
          cmd_kwargs={'test': 'args'})
      lab_xfer = auto_updater_transfer.LabTransfer
      expected = [['curl', '-o', '/test/device/payload/dir/test_update.gz',
                   'http://0.0.0.0:8000/static/test_update.gz']]

      self.PatchObject(lab_xfer, '_EnsureDeviceDirectory')
      self.PatchObject(remote_access.ChromiumOSDevice, 'run')
      self.PatchObject(remote_access.ChromiumOSDevice, 'CopyToWorkDir')

      CrOS_LabTransfer._TransferRootfsUpdate()
      self.assertListEqual(
          remote_access.ChromiumOSDevice.run.call_args_list,
          [mock.call(x) for x in expected])

  def testTransferRootfsRunCmdLocalPayloadProps(self):
    """Test method calls of LabTransfer._TransferRootfsUpdate().

    Test whether remote_access.ChromiumOSDevice.run() is being called
    the correct number of times with the correct arguments when
    LabTransfer.LocalPayloadPropsFile is set to a valid local filepath.
    """
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_LabTransfer = CreateLabTransferInstance(
          device, payload_name='test_update.gz',
          device_payload_dir='/test/device/payload/dir',
          cmd_kwargs={'test': 'args'})

      self.PatchObject(os.path, 'isfile', return_value=True)
      CrOS_LabTransfer.LocalPayloadPropsFile = '/existent/test.gz.json'

      lab_xfer = auto_updater_transfer.LabTransfer
      expected = [
          ['curl', '-o', '/test/device/payload/dir/test_update.gz',
           'http://0.0.0.0:8000/static/test_update.gz']]

      self.PatchObject(lab_xfer, '_EnsureDeviceDirectory')
      self.PatchObject(remote_access.ChromiumOSDevice, 'run')
      self.PatchObject(remote_access.ChromiumOSDevice, 'CopyToWorkDir')

      CrOS_LabTransfer._TransferRootfsUpdate()
      self.assertListEqual(
          remote_access.ChromiumOSDevice.run.call_args_list,
          [mock.call(x) for x in expected])

  def testTransferRootfsEnsureDirCalls(self):
    """Test method calls of LabTransfer._TransferRootfsUpdate().

    Test whether LabTransfer._EnsureDeviceDirectory() is being called
    the correct number of times with the correct arguments.
    """
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_LabTransfer = CreateLabTransferInstance(
          device, device_payload_dir='/test/device/payload/dir',
          payload_name='test_update.gz', cmd_kwargs={'test': 'args'})
      lab_xfer = auto_updater_transfer.LabTransfer
      expected = [CrOS_LabTransfer._device_payload_dir]

      self.PatchObject(lab_xfer, '_EnsureDeviceDirectory')
      self.PatchObject(remote_access.ChromiumOSDevice, 'run')
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
      CrOS_LabTransfer = CreateLabTransferInstance(
          device, cmd_kwargs={'test': 'test'},
          device_payload_dir='/test/device/payload/dir',
          payload_name='test_update.gz')

      CrOS_LabTransfer._local_payload_props_path = '/existent/test.gz.json'

      lab_xfer = auto_updater_transfer.LabTransfer
      expected = [{'src': CrOS_LabTransfer._local_payload_props_path,
                   'dest': CrOS_LabTransfer.PAYLOAD_DIR_NAME,
                   'mode': CrOS_LabTransfer._payload_mode,
                   'log_output': True, 'test': 'test'}]

      self.PatchObject(lab_xfer, '_EnsureDeviceDirectory')
      self.PatchObject(remote_access.ChromiumOSDevice, 'run')
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
      lab_xfer = auto_updater_transfer.LabTransfer
      CrOS_LabTransfer = CreateLabTransferInstance(
          device, payload_name='test_update.gz',
          payload_dir='board-release/12345.6.7')
      self.PatchObject(auto_updater_transfer, 'STATEFUL_FILENAME',
                       'test_stateful.tgz')
      self.PatchObject(lab_xfer, '_RemoteDevserverCall')

      expected = [
          ['curl', '-I', 'http://0.0.0.0:8000/static/board-release/12345.6.7/'
                         'test_update.gz', '--fail'],
          ['curl', '-I', 'http://0.0.0.0:8000/static/board-release/12345.6.7/'
                         'test_update.gz.json', '--fail'],
          ['curl', '-I', 'http://0.0.0.0:8000/static/board-release/12345.6.7/'
                         'test_stateful.tgz', '--fail']]

      CrOS_LabTransfer.CheckPayloads()
      self.assertListEqual(lab_xfer._RemoteDevserverCall.call_args_list,
                           [mock.call(x) for x in expected])

  def testCheckPayloadsAllInRemoteDevserverCallError(self):
    """Test auto_updater_transfer.CheckPayloads.

    Test CheckPayloads() method's fallback when transfer_rootfs_update and
    transfer_stateful_update are both set to True and
    LabTransfer._RemoteDevserver() throws an error.
    """
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_LabTransfer = CreateLabTransferInstance(
          device, payload_name='test_update.gz',
          payload_dir='board-release/12345.6.7')

      self.PatchObject(auto_updater_transfer, 'STATEFUL_FILENAME',
                       'test_stateful.tgz')
      self.PatchObject(auto_updater_transfer.LabTransfer,
                       '_RemoteDevserverCall',
                       side_effect=cros_build_lib.RunCommandError(msg=''))
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
      lab_xfer = auto_updater_transfer.LabTransfer
      CrOS_LabTransfer = CreateLabTransferInstance(
          device, payload_name='test_update.gz',
          payload_dir='board-release/12345.6.7',
          transfer_stateful_update=False)

      self.PatchObject(lab_xfer, '_RemoteDevserverCall')

      expected = [['curl', '-I', 'http://0.0.0.0:8000/static/board-release/'
                                 '12345.6.7/test_update.gz', '--fail'],
                  ['curl', '-I', 'http://0.0.0.0:8000/static/board-release/'
                                 '12345.6.7/test_update.gz.json', '--fail']]

      CrOS_LabTransfer.CheckPayloads()
      self.assertListEqual(lab_xfer._RemoteDevserverCall.call_args_list,
                           [mock.call(x) for x in expected])

  def testCheckPayloadsNoStatefulTransferRemoteDevserverCallError(self):
    """Test auto_updater_transfer.CheckPayloads.

    Test CheckPayloads() method's fallback when transfer_rootfs_update is True
    and transfer_stateful_update is set to False and
    LabTransfer._RemoteDevserver() throws an error.
    """
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_LabTransfer = CreateLabTransferInstance(
          device, payload_name='test_update.gz',
          payload_dir='board-release/12345.6.7',
          transfer_stateful_update=False)
      self.PatchObject(auto_updater_transfer.LabTransfer,
                       '_RemoteDevserverCall',
                       side_effect=cros_build_lib.RunCommandError(msg=''))
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
      lab_xfer = auto_updater_transfer.LabTransfer
      CrOS_LabTransfer = CreateLabTransferInstance(
          device, payload_dir='board-release/12345.6.7',
          transfer_rootfs_update=False)
      self.PatchObject(auto_updater_transfer, 'STATEFUL_FILENAME',
                       'test_stateful.tgz')
      self.PatchObject(lab_xfer, '_RemoteDevserverCall')

      expected = [['curl', '-I', 'http://0.0.0.0:8000/static/board-release/'
                                 '12345.6.7/test_stateful.tgz', '--fail']]
      CrOS_LabTransfer.CheckPayloads()
      self.assertListEqual(lab_xfer._RemoteDevserverCall.call_args_list,
                           [mock.call(x) for x in expected])

  def testCheckPayloadsNoRootfsTransferRemoteDevserverCallError(self):
    """Test auto_updater_transfer.CheckPayloads.

    Test CheckPayloads() method's fallback when transfer_rootfs_update is False
    and transfer_stateful_update is set to True and
    LabTransfer._RemoteDevserver() throws an error.
    """
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_LabTransfer = CreateLabTransferInstance(
          device, payload_dir='board-release/12345.6.7',
          transfer_rootfs_update=False)
      self.PatchObject(auto_updater_transfer, 'STATEFUL_FILENAME',
                       'test_stateful.tgz')
      self.PatchObject(auto_updater_transfer.LabTransfer,
                       '_RemoteDevserverCall',
                       side_effect=cros_build_lib.RunCommandError(msg=''))
      self.PatchObject(retry_util, 'RunCurl')

      expected = [
          {'curl_args': ['-I', 'http://0.0.0.0:8000/static/board-release/'
                               '12345.6.7/test_stateful.tgz', '--fail'],
           'log_output': True}]

      CrOS_LabTransfer.CheckPayloads()
      self.assertListEqual(retry_util.RunCurl.call_args_list,
                           [mock.call(**x) for x in expected])

  def testCheckPayloadsNoPayloadError(self):
    """Test auto_updater_transfer.CheckPayloads.

    Test CheckPayloads() for exceptions raised when payloads are not available
    on the staging server.
    """
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_LabTransfer = CreateLabTransferInstance(device)

      self.PatchObject(auto_updater_transfer.LabTransfer,
                       '_RemoteDevserverCall',
                       side_effect=cros_build_lib.RunCommandError(msg=''))
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
      lab_xfer = auto_updater_transfer.LabTransfer
      payload_props_path = os.path.join(self.tempdir, 'test_update.gz.json')
      output = ('{"appid": "{0BB3F9E1-A066-9352-50B8-5C1356D09AEB}", '
                '"is_delta": false, "metadata_signature": null, '
                '"metadata_size": 57053, '
                '"sha256_hex": "aspPgQRWLu5wPM5NucqAYVmVCvL5lxQJ/n9ckhZS83Y=", '
                '"size": 998103540, '
                '"target_version": "99999.0.0", "version": 2}')
      bin_op = six.ensure_binary(output)

      CrOS_LabTransfer = CreateLabTransferInstance(
          device, tempdir=self.tempdir, payload_name='test_update.gz')
      self.PatchObject(lab_xfer, '_RemoteDevserverCall',
                       return_value=cros_build_lib.CommandResult(stdout=bin_op))
      CrOS_LabTransfer.GetPayloadPropsFile()
      props = osutils.ReadFile(payload_props_path)

      self.assertEqual(props, output)
      self.assertEqual(CrOS_LabTransfer._local_payload_props_path,
                       payload_props_path)

  def testGetPayloadPropsFileWrongFormat(self):
    """Test LabTransfer.GetPayloadPropsFile().

    Test when the payload is not in the correct json format. Also, test if the
    fallback is being called.
    """
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      lab_xfer = auto_updater_transfer.LabTransfer
      payload_props_path = os.path.join(self.tempdir, 'test_update.gz.json')
      output = 'Not in Json format'

      CrOS_LabTransfer = CreateLabTransferInstance(
          device, tempdir=self.tempdir, payload_name='test_update.gz')

      self.PatchObject(lab_xfer, '_RemoteDevserverCall',
                       return_value=cros_build_lib.CommandResult(stdout=output))
      self.PatchObject(retry_util, 'RunCurl',
                       return_value=cros_build_lib.CommandResult(stdout=''))
      CrOS_LabTransfer.GetPayloadPropsFile()

      self.assertEqual(CrOS_LabTransfer._local_payload_props_path,
                       payload_props_path)

  def testGetPayloadPropsFileRemoteDevserverCallError(self):
    """Test LabTransfer.GetPayloadPropsFile().

    Test when the LabTransfer._RemoteDevserverCall() method throws an error.
    """
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:

      lab_xfer = auto_updater_transfer.LabTransfer
      payload_props_path = os.path.join(self.tempdir, 'test_update.gz.json')
      output = ''

      CrOS_LabTransfer = CreateLabTransferInstance(
          device, tempdir=self.tempdir, payload_name='test_update.gz')

      self.PatchObject(lab_xfer, '_RemoteDevserverCall',
                       side_effect=cros_build_lib.RunCommandError(msg=''))
      self.PatchObject(retry_util, 'RunCurl',
                       return_value=cros_build_lib.CommandResult(stdout=output))

      CrOS_LabTransfer.GetPayloadPropsFile()

      self.assertEqual(CrOS_LabTransfer._local_payload_props_path,
                       payload_props_path)

  def testGetPayloadPropsFileError(self):
    """Test LabTransfer.GetPayloadPropsFile().

    Test when the payload is not available.
    """
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      lab_xfer = auto_updater_transfer.LabTransfer
      CrOS_LabTransfer = CreateLabTransferInstance(
          device, tempdir=self.tempdir, payload_name='test_update.gz',
          payload_dir='/test/dir')
      self.PatchObject(lab_xfer, '_RemoteDevserverCall',
                       side_effect=cros_build_lib.RunCommandError(msg=''))
      self.PatchObject(retry_util, 'RunCurl',
                       side_effect=retry_util.DownloadError())
      self.assertRaises(auto_updater_transfer.ChromiumOSTransferError,
                        CrOS_LabTransfer.GetPayloadPropsFile)
      self.assertIsNone(CrOS_LabTransfer._local_payload_props_path)

  def test_GetPayloadSize(self):
    """Test auto_updater_transfer._GetPayloadSize()."""
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      lab_xfer = auto_updater_transfer.LabTransfer
      CrOS_LabTransfer = CreateLabTransferInstance(
          device, payload_name='test_update.gz',
          payload_dir='/test/payload/dir')
      expected_size = 75810234
      output = 'Content-Length: %s' % str(expected_size)
      self.PatchObject(lab_xfer, '_RemoteDevserverCall',
                       return_value=cros_build_lib.CommandResult(stdout=output))
      size = CrOS_LabTransfer._GetPayloadSize()
      self.assertEqual(size, expected_size)

  def test_GetPayloadSizeRemoteDevserverError(self):
    """Test auto_updater_transfer._GetPayloadSize().

    Test when LabTransfer._RemoteDevserverCall throws an error.
    """
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      lab_xfer = auto_updater_transfer.LabTransfer
      CrOS_LabTransfer = CreateLabTransferInstance(
          device, payload_name='test_update.gz',
          payload_dir='/test/payload/dir')
      expected_size = 75810234
      output = 'Content-Length: %s' % str(expected_size)
      self.PatchObject(lab_xfer, '_RemoteDevserverCall',
                       side_effect=cros_build_lib.RunCommandError(msg=''))
      self.PatchObject(retry_util, 'RunCurl',
                       return_value=cros_build_lib.CommandResult(stdout=output))
      size = CrOS_LabTransfer._GetPayloadSize()
      self.assertEqual(size, expected_size)

  def test_GetPayloadSizeNoRegexMatchError(self):
    """Test errors thrown by auto_updater_transfer._GetPayloadSize().

    Test error thrown when the output received from the curl command does not
    contain expected fields.
    """
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      lab_xfer = auto_updater_transfer.LabTransfer
      CrOS_LabTransfer = CreateLabTransferInstance(
          device, payload_name='test_update.gz',
          payload_dir='/test/payload/dir')
      output = 'No Match Output'
      self.PatchObject(lab_xfer, '_RemoteDevserverCall',
                       return_value=cros_build_lib.CommandResult(stdout=output))
      self.assertRaises(auto_updater_transfer.ChromiumOSTransferError,
                        CrOS_LabTransfer._GetPayloadSize)

  def test_GetPayloadSizeNoRegexMatchRemoteDevserverCallError(self):
    """Test errors thrown by auto_updater_transfer._GetPayloadSize().

    Test error thrown when the output received from the curl command does not
    contain expected fields and LabTransfer._RemoteDevserverCall method throws
    an error.
    """
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      lab_xfer = auto_updater_transfer.LabTransfer
      CrOS_LabTransfer = CreateLabTransferInstance(
          device, payload_name='test_update.gz',
          payload_dir='/test/payload/dir')
      output = 'No Match Output'
      self.PatchObject(lab_xfer, '_RemoteDevserverCall',
                       side_effect=cros_build_lib.RunCommandError(msg=''))
      self.PatchObject(retry_util, 'RunCurl',
                       return_value=cros_build_lib.CommandResult(stdout=output))
      self.assertRaises(auto_updater_transfer.ChromiumOSTransferError,
                        CrOS_LabTransfer._GetPayloadSize)

  def test_GetPayloadSizeCurlRunCommandError(self):
    """Test errors thrown by auto_updater_transfer.GetPayloadSize().

    Test error thrown when cros_build_lib.run raises a RunCommandError.
    """
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      lab_xfer = auto_updater_transfer.LabTransfer
      CrOS_LabTransfer = CreateLabTransferInstance(
          device, payload_name='test_update.gz',
          payload_dir='/test/payload/dir')
      self.PatchObject(lab_xfer, '_RemoteDevserverCall',
                       side_effect=cros_build_lib.RunCommandError(msg=''))
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

  def test_RemoteDevserverCall(self):
    """Test LabTransfer._RemoteDevserverCall()."""
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_LabTransfer = CreateLabTransferInstance(device)

      self.PatchObject(cros_build_lib, 'run')
      cmd = ['test', 'command']

      CrOS_LabTransfer._RemoteDevserverCall(cmd=cmd)
      self.assertListEqual(
          cros_build_lib.run.call_args_list,
          [mock.call(['ssh', '0.0.0.0'] + cmd, log_output=True)])

  def test_RemoteDevserverCallError(self):
    """Test LabTransfer._RemoteDevserverCall().

    Test method when error is thrown by cros_build_lib.run() method.
    """
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_LabTransfer = CreateLabTransferInstance(device)

      self.PatchObject(cros_build_lib, 'run',
                       side_effect=cros_build_lib.RunCommandError(msg=''))

      self.assertRaises(cros_build_lib.RunCommandError,
                        CrOS_LabTransfer._RemoteDevserverCall,
                        cmd=['test', 'command'])


class CrosLabEndToEndPayloadTransferTest(cros_test_lib.MockTempDirTestCase):
  """Test all methods in auto_updater_transfer.LabEndToEndPayloadTransfer."""

  def setUp(self):
    """Mock remote_access.RemoteDevice/ChromiumOSDevice functions for update."""
    self.PatchObject(remote_access.RemoteDevice, 'work_dir', '/test/work/dir')

  def testGetPayloadPropsLabEndToEnd(self):
    """Test LabEndToEndPayloadTransfer.GetPayloadProps()."""
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      payload_name = 'payloads/chromeos_12345.0.0_board_channel_test.bin-blah'
      CrOS_LabETETransfer = CreateLabEndToEndPayloadTransferInstance(
          device, payload_name=payload_name)
      self.PatchObject(auto_updater_transfer.LabTransfer, '_GetPayloadSize',
                       return_value=123)
      expected = {'image_version': '12345.0.0', 'size': 123}
      self.assertDictEqual(CrOS_LabETETransfer.GetPayloadProps(), expected)

  def testGetPayloadPropsLabEndToEndError(self):
    """Test error thrown by LabEndToEndPayloadTransfer.GetPayloadProps().

    Test error thrown when payload_name is not in the expected format of
    payloads/chromeos_12345.0.0_board_channel_full_test.bin-blah.
    """
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_LabETETransfer = CreateLabEndToEndPayloadTransferInstance(
          device, payload_name='/wrong/format/will/fail')
      self.PatchObject(auto_updater_transfer.LabTransfer, '_GetPayloadSize')
      self.assertRaises(ValueError, CrOS_LabETETransfer.GetPayloadProps)

  def testLabTransferGetCurlCmdStandard(self):
    """Test LabEndToEndPayloadTransfer._GetCurlCmdForPayloadDownload.

    Tests the typical usage of the _GetCurlCmdForPayloadDownload() method.
    """
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      CrOS_LabETETransfer = CreateLabEndToEndPayloadTransferInstance(device)
      expected_cmd = ['curl', '-o',
                      '/tmp/test_payload_dir/payload_filename.ext',
                      'http://0.0.0.0:8000/static/stable-channel/board/'
                      '12345.0.0/payloads/payload_filename.ext']
      cmd = CrOS_LabETETransfer._GetCurlCmdForPayloadDownload(
          payload_dir='/tmp/test_payload_dir',
          payload_filename='payloads/payload_filename.ext',
          build_id='stable-channel/board/12345.0.0')
      self.assertEqual(cmd, expected_cmd)

  def testGetPayloadPropsFileLabETETransfer(self):
    """Test LabEndToEndPayloadTransfer.GetPayloadPropsFile()."""
    with remote_access.ChromiumOSDeviceHandler(remote_access.TEST_IP) as device:
      lab_ete_xfer = auto_updater_transfer.LabEndToEndPayloadTransfer
      payload_name = 'payloads/test_update.gz'
      payload_props_path = os.path.join(self.tempdir, payload_name + '.json')
      output = ('{"appid": "{0BB3F9E1-A066-9352-50B8-5C1356D09AEB}", '
                '"is_delta": false, "metadata_signature": null, '
                '"metadata_size": 57053, '
                '"sha256_hex": "aspPgQRWLu5wPM5NucqAYVmVCvL5lxQJ/n9ckhZS83Y=", '
                '"size": 998103540, '
                '"target_version": "99999.0.0", "version": 2}')
      bin_op = six.ensure_binary(output)

      CrOS_LabETETransfer = CreateLabEndToEndPayloadTransferInstance(
          device, tempdir=self.tempdir, payload_name=payload_name)

      self.PatchObject(lab_ete_xfer, '_RemoteDevserverCall',
                       return_value=cros_build_lib.CommandResult(stdout=bin_op))
      CrOS_LabETETransfer.GetPayloadPropsFile()
      props = osutils.ReadFile(payload_props_path)

      self.assertEqual(props, output)
      self.assertEqual(CrOS_LabETETransfer._local_payload_props_path,
                       payload_props_path)
