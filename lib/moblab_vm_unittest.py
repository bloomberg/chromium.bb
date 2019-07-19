# -*- coding: utf-8 -*-
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for the moblab_vm.py module."""

from __future__ import print_function

import mock
import os
import shutil
import unittest

from chromite.lib import cros_test_lib
from chromite.lib import moblab_vm
from chromite.lib import osutils


class MoblabVmTestCase(cros_test_lib.MockTempDirTestCase):
  """Tests for MoablabVm class."""

  _PORT = 937

  def setUp(self):
    # Mock out all file-system and KVM operations. Most of these are channeled
    # though a private helper function in moblab_vm which we mock. Where
    # meaningful, these helper functions are tested in a separate TestCase.
    self._mock_create_vm_image = self.PatchObject(
        moblab_vm, '_CreateVMImage', autospec=True)
    self._mock_create_moblab_disk = self.PatchObject(
        moblab_vm, '_CreateMoblabDisk', autospec=True)
    self._mock_try_create_bridge_device = self.PatchObject(
        moblab_vm, '_TryCreateBridgeDevice', autospec=True)
    self._mock_create_tap_device = self.PatchObject(
        moblab_vm, '_CreateTapDevice', autospec=True)
    self._mock_connect_device_to_bridge = self.PatchObject(
        moblab_vm, '_ConnectDeviceToBridge', autospec=True)
    self._mock_device_up = self.PatchObject(
        moblab_vm, '_DeviceUp', autospec=True)
    self._mock_start_vm = self.PatchObject(
        moblab_vm, '_StartVM', autospec=True)
    self._mock_remove_network_bridge = self.PatchObject(
        moblab_vm, '_RemoveNetworkBridgeIgnoringErrors', autospec=True)
    self._mock_remove_tap_device = self.PatchObject(
        moblab_vm, '_RemoveTapDeviceIgnoringErrors', autospec=True)
    self._mock_stop_vm = self.PatchObject(
        moblab_vm, '_StopVM', autospec=True)

    self._mock_mount = self.PatchObject(osutils, 'MountDir', autospec=True)
    self._mock_umount = self.PatchObject(osutils, 'UmountDir', autospec=True)

    self._InitializeCommonAttributes()

  def _InitializeCommonAttributes(self):
    """Initialize attributes used by chained tests.

    To allow us to test the full flow of MoblabVm without repeating ourselves
    too much, we chain some of the tests together. These tests use attributes on
    the TestCase instance that we must initialize here for sanity.

    These paths are realistic, to serve as documentation for how the workspace
    is laid out, and also to minimize consequences of actually updating the
    paths during a unittest.
    """
    # pylint:disable=protected-access
    self.bridge_name = 'network_bridge'
    self.moblab_from_path = os.path.join(self.tempdir, 'moblab_from')
    self.dut_from_path = os.path.join(self.tempdir, 'dut_from')
    self.moblab_tap_dev = 'moblabtap'
    self.dut_tap_dev = 'duttap'
    self._InitializeWorkspaceAttributes()

  def _InitializeWorkspaceAttributes(self, workspace='workspace'):
    """Initialize attributes related to workspace paths."""
    # pylint:disable=protected-access,attribute-defined-outside-init
    self.workspace = os.path.join(self.tempdir, workspace)
    self.chroot = os.path.join(self.tempdir, 'chroot')
    osutils.SafeMakedirs(self.workspace)
    osutils.SafeMakedirs(self.chroot)
    self.moblab_workdir_path = os.path.join(self.workspace,
                                            moblab_vm._WORKSPACE_MOBLAB_DIR)
    self.moblab_image_path = os.path.join(self.moblab_workdir_path,
                                          'chromiumos_qemu_image.bin')
    self.moblab_disk_path = os.path.join(self.moblab_workdir_path, 'disk')
    self.moblab_ssh_port = self._PORT + 2
    self.dut_workdir_path = os.path.join(self.workspace,
                                         moblab_vm._WORKSPACE_DUT_DIR)
    self.dut_image_path = os.path.join(self.dut_workdir_path,
                                       'chromiumos_qemu_image.bin')
    self.dut_ssh_port = self._PORT + 6

  def testInstanceInitialization(self):
    """Sanity check attributes before using the instance for anything."""
    vmsetup = moblab_vm.MoblabVm(self.workspace)
    self.assertFalse(vmsetup.initialized)
    self.assertFalse(vmsetup.running)
    self.assertFalse(vmsetup.dut_running)

  def testSuccessfulCreateNoDutDir(self):
    """A successful call to .Create not using a DUT image."""
    self._mock_create_vm_image.return_value = os.path.join(
        self.moblab_workdir_path, 'chromiumos_qemu_image.bin')
    self._mock_create_moblab_disk.return_value = self.moblab_disk_path

    vmsetup = moblab_vm.MoblabVm(self.workspace)
    vmsetup.Create(self.moblab_from_path)
    self._mock_create_vm_image.assert_called_once_with(
        self.moblab_from_path, self.moblab_workdir_path)
    self._mock_create_moblab_disk.assert_called_once_with(
        self.moblab_workdir_path)
    self.assertExists(self.moblab_workdir_path)
    self.assertNotExists(self.dut_workdir_path)
    self.assertTrue(vmsetup.initialized)
    self.assertFalse(vmsetup.running)
    self.assertFalse(vmsetup.dut_running)

  def testSuccessfulCreateWithDutDir(self):
    """A successful call to .Create using a DUT image."""
    self._mock_create_vm_image.side_effect = iter([self.moblab_image_path,
                                                   self.dut_image_path])
    self._mock_create_moblab_disk.return_value = os.path.join(
        self.moblab_workdir_path, 'disk')

    vmsetup = moblab_vm.MoblabVm(self.workspace)
    vmsetup.Create(self.moblab_from_path, self.dut_from_path)
    self.assertEqual(
        self._mock_create_vm_image.call_args_list,
        [
            mock.call(self.moblab_from_path, self.moblab_workdir_path),
            mock.call(self.dut_from_path, self.dut_workdir_path),
        ],
    )
    self.assertExists(self.moblab_workdir_path)
    self.assertExists(self.dut_workdir_path)
    self._mock_create_moblab_disk.assert_called_once_with(
        self.moblab_workdir_path)
    self.assertTrue(vmsetup.initialized)
    self.assertFalse(vmsetup.running)
    self.assertFalse(vmsetup.dut_running)

  def testCreateNoCreateVmRaisesWhenSourceImageMissing(self):
    """A call to .Create w/o create_vm_images expects source image to exist."""
    vmsetup = moblab_vm.MoblabVm(self.workspace)
    with self.assertRaises(moblab_vm.SetupError):
      vmsetup.Create(self.moblab_from_path, create_vm_images=False)

  def testSuccessfulCreateNoCreateVm(self):
    """A successful call to .Create w/o create_vm_images."""
    self._mock_create_moblab_disk.return_value = self.moblab_disk_path
    osutils.SafeMakedirsNonRoot(self.moblab_from_path)
    osutils.WriteFile(os.path.join(self.moblab_from_path,
                                   'chromiumos_qemu_image.bin'),
                      'fake_image')

    vmsetup = moblab_vm.MoblabVm(self.workspace)
    vmsetup.Create(self.moblab_from_path, create_vm_images=False)
    self.assertEqual(self._mock_create_vm_image.call_count, 0)
    self._mock_create_moblab_disk.assert_called_once_with(
        self.moblab_workdir_path)
    self.assertTrue(vmsetup.initialized)
    self.assertFalse(vmsetup.running)
    self.assertFalse(vmsetup.dut_running)

  def testStartWithoutCreateRaises(self):
    """Calling .Start before .Create is an error."""
    vmsetup = moblab_vm.MoblabVm(self.workspace)
    with self.assertRaises(moblab_vm.StartError):
      vmsetup.Start()

  def testSuccessfulStartAfterCreateWithoutDut(self):
    """A successful call to .Start, without a dut attached."""
    self.testSuccessfulCreateNoDutDir()
    self._testSuccessfulStartAfterCreate(False)

  def testSuccessfulStartAfterCreateWithDut(self):
    """A successful call to .Start, with a dut attached."""
    self.testSuccessfulCreateWithDutDir()
    self._testSuccessfulStartAfterCreate(True)

  def _testSuccessfulStartAfterCreate(self, with_dut):
    """A successful call to .Start, with or without a dut attached.

    This helper methods lets us 'parameterize' a longish test. Only change
    expectations based on the arguments. DO NOT change the interactions with
    MolabVm.
    """
    self._mock_try_create_bridge_device.return_value = (self._PORT,
                                                        self.bridge_name)
    self._mock_create_tap_device.side_effect = iter([self.moblab_tap_dev,
                                                     self.dut_tap_dev])

    # Create a new moblabvm instance. MoblabVm persists everything to disk an
    # must work seamlessly across instance discards.
    vmsetup = moblab_vm.MoblabVm(self.workspace, chroot_dir=self.chroot)
    vmsetup.Start()
    self.assertTrue(vmsetup.initialized)
    self.assertTrue(vmsetup.running)
    self.assertEqual(vmsetup.dut_running, with_dut)

    self.assertEqual(self._mock_try_create_bridge_device.call_count, 1)
    self.assertEqual(self._mock_create_tap_device.call_count, 2)
    # Different suffixes are used for tap devices.
    self.assertNotEqual(
        self._mock_create_tap_device.call_args_list[0][0][0],
        self._mock_create_tap_device.call_args_list[1][0][0],
    )
    self.assertEqual(
        self._mock_connect_device_to_bridge.call_args_list,
        [
            mock.call('moblabtap', self.bridge_name),
            mock.call('duttap', self.bridge_name),
        ])

    vm_calls = [mock.call(
        self.moblab_image_path, self._PORT + 2, self._PORT + 6,
        self.moblab_tap_dev, mock.ANY, is_moblab=True,
        disk_path=self.moblab_disk_path, chroot_path=self.chroot)]
    if with_dut:
      vm_calls.append(mock.call(
          self.dut_image_path, self._PORT + 6, self._PORT + 7,
          self.dut_tap_dev, mock.ANY, is_moblab=False, chroot_path=self.chroot))
    self.assertEqual(self._mock_start_vm.call_args_list, vm_calls)
    if with_dut:
      # Different SSH ports are used for the two VMs
      self.assertNotEqual(
          self._mock_start_vm.call_args_list[0][0][1],
          self._mock_start_vm.call_args_list[1][0][1],
      )
      # Different MAC addresses are used for secondary networks of two VMs.
      self.assertNotEqual(
          self._mock_start_vm.call_args_list[0][0][3],
          self._mock_start_vm.call_args_list[1][0][3],
      )

  def testStopAfterStartUndoesEverythingInStackWithDut(self):
    """.Stop undoes what .Start did in reverse order."""
    self.testSuccessfulCreateWithDutDir()

    # Allow .Start to run, without expectation checking (done in other tests)
    self._mock_try_create_bridge_device.return_value = (self._PORT,
                                                        self.bridge_name)
    self._mock_create_tap_device.side_effect = iter([self.moblab_tap_dev,
                                                     self.dut_tap_dev])
    vmsetup = moblab_vm.MoblabVm(self.workspace, chroot_dir=self.chroot)
    vmsetup.Start()

    # Verify .Stop releases global resources.
    vmsetup = moblab_vm.MoblabVm(self.workspace, chroot_dir=self.chroot)
    vmsetup.Stop()
    self.assertEqual(self._mock_stop_vm.call_args_list,
                     [mock.call(self.dut_ssh_port, chroot_path=self.chroot),
                      mock.call(self.moblab_ssh_port, chroot_path=self.chroot)])
    self.assertEqual(self._mock_remove_tap_device.call_args_list,
                     [mock.call(self.dut_tap_dev),
                      mock.call(self.moblab_tap_dev)])
    self.assertEqual(self._mock_remove_network_bridge.call_args_list,
                     [mock.call(self.bridge_name)])

    # Verify that final state is persisted across instance discards.
    vmsetup = moblab_vm.MoblabVm(self.workspace)
    self.assertFalse(vmsetup.running)
    self.assertFalse(vmsetup.dut_running)

  def testStopAfterStartingDutKvmFails(self):
    """Calling .Stop after a failed .Start undoes partial .Start."""
    self.testSuccessfulCreateWithDutDir()

    # Allow .Start to run, without expectation checking (done in other tests)
    self._mock_try_create_bridge_device.return_value = (self._PORT,
                                                        self.bridge_name)
    self._mock_create_tap_device.side_effect = iter([self.moblab_tap_dev,
                                                     self.dut_tap_dev])
    self._mock_start_vm.side_effect = iter([self.moblab_ssh_port,
                                            _TestException])
    vmsetup = moblab_vm.MoblabVm(self.workspace)
    with self.assertRaises(_TestException):
      vmsetup.Start()

    # Verify .Stop releases global resources.
    vmsetup = moblab_vm.MoblabVm(self.workspace, chroot_dir=self.chroot)
    vmsetup.Stop()
    self.assertEqual(self._mock_stop_vm.call_args_list,
                     [mock.call(self.moblab_ssh_port, chroot_path=self.chroot)])
    self.assertEqual(self._mock_remove_tap_device.call_args_list,
                     [mock.call(self.dut_tap_dev),
                      mock.call(self.moblab_tap_dev)])
    self.assertEqual(self._mock_remove_network_bridge.call_args_list,
                     [mock.call(self.bridge_name)])

    # Verify that final state is persisted across instance discards.
    vmsetup = moblab_vm.MoblabVm(self.workspace)
    self.assertFalse(vmsetup.running)
    self.assertFalse(vmsetup.dut_running)

  def testSuccessfulStartAfterCreateAndMoveWorkspace(self):
    """A successful call to .Start, without a dut attached."""
    self.testSuccessfulCreateNoDutDir()
    new_workspace = os.path.join(self.tempdir, 'new_workspace')
    shutil.move(self.workspace, new_workspace)
    self._InitializeWorkspaceAttributes(new_workspace)
    self._testSuccessfulStartAfterCreate(False)

  def testRunVmsContextNoFailure(self):
    """Calls .Start and .Stop in a basic use case."""
    vmsetup = moblab_vm.MoblabVm(self.workspace)
    start = self.PatchObject(moblab_vm.MoblabVm, 'Start')
    destroy = self.PatchObject(moblab_vm.MoblabVm, 'Destroy')
    with vmsetup.RunVmsContext():
      pass
    self.assertEqual(start.call_count, 1)
    self.assertEqual(destroy.call_count, 1)

  def testRunVmsContextWithFailure(self):
    """Calls .Start and .Stop when exception is raised within context."""
    vmsetup = moblab_vm.MoblabVm(self.workspace)
    start = self.PatchObject(moblab_vm.MoblabVm, 'Start')
    destroy = self.PatchObject(moblab_vm.MoblabVm, 'Destroy')
    try:
      with vmsetup.RunVmsContext():
        raise ValueError('uh oh!')
    except ValueError:
      self.assertEqual(start.call_count, 1)
      self.assertEqual(destroy.call_count, 1)

  def testMountMoblabDiskContextInUninitilizedWorkspaceRaises(self):
    """Cannot mount a disk if the workspace isn't even initialized."""
    vmsetup = moblab_vm.MoblabVm(self.workspace)
    with self.assertRaises(moblab_vm.MoblabVmError):
      with vmsetup.MountedMoblabDiskContext():
        pass

  def testMountMoblabDiskContextAfterStartRaises(self):
    """Cannot mount a disk if the workspace isn't even initialized."""
    self.testSuccessfulStartAfterCreateWithoutDut()
    vmsetup = moblab_vm.MoblabVm(self.workspace)
    with self.assertRaises(moblab_vm.MoblabVmError):
      with vmsetup.MountedMoblabDiskContext():
        pass

  def testSuccessfulMountMoblabDiskContextAfter(self):
    """MountMoblabDiskContext works OK for initialized, non-running VM."""
    self.testSuccessfulCreateNoDutDir()
    vmsetup = moblab_vm.MoblabVm(self.workspace)
    with vmsetup.MountedMoblabDiskContext() as mounted_path:
      self.assertStartsWith(mounted_path, self.tempdir)


class HelperFunctionsTestCase(unittest.TestCase):
  """Tests for the private functions in moblab_mv module for os interactions."""

  def testRunIgnoringErrors(self):
    """Really ignores errors, and warns the user."""


class _TestException(Exception):
  """Exception to raise from tests."""
