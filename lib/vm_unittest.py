# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for VM."""

from __future__ import print_function

import mock
import os
import stat

from chromite.cli.cros import cros_chrome_sdk
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import partial_mock
from chromite.lib import vm


# pylint: disable=protected-access
class VMTester(cros_test_lib.RunCommandTempDirTestCase):
  """Test vm.VM."""

  def setUp(self):
    """Common set up method for all tests."""
    opts = vm.VM.GetParser().parse_args([])
    self._vm = vm.VM(opts)
    self._vm.board = 'amd64-generic'
    self._vm.cache_dir = self.tempdir
    self._vm.image_path = self.TempFilePath('chromiumos_qemu_image.bin')
    osutils.Touch(self._vm.image_path)

    # Satisfy QEMU version check.
    version_str = ('QEMU emulator version 2.6.0, Copyright (c) '
                   '2003-2008 Fabrice Bellard')
    self.rc.AddCmdResult(partial_mock.In('--version'), output=version_str)

    self.ssh_port = self._vm.ssh_port

  def TempFilePath(self, file_path):
    return os.path.join(self.tempdir, file_path)

  def TempVMPath(self, kvm_file):
    return self.TempFilePath(os.path.join('cros_vm_%d' % self.ssh_port,
                                          kvm_file))

  def FindPathInArgs(self, args, path):
    """Checks the called commands to see if the path is present.

    Args:
      args: List of called commands.
      path: Path to check if present in the called commands.

    Returns:
      Whether the path is found in the called commands.
    """
    for call in args:
      # A typical call looks like:
      # call(['.../chroot/usr/bin/qemu-system-x86_64', '--version'],
      #      capture_output=True)
      if any(path in a for a in call[0][0]):
        return True
    return False

  def testStart(self):
    self._vm.Start()
    self.assertCommandContains([self._vm.qemu_path])
    self.assertCommandContains(['-m', '8G', '-smp', '8', '-vga', 'virtio',
                                '-daemonize', '-usbdevice', 'tablet',])
    self.assertCommandContains([
        '-pidfile', self.TempVMPath('kvm.pid'),
        '-chardev', 'pipe,id=control_pipe,path=%s'
        % self.TempVMPath('kvm.monitor'),
        '-serial', 'file:%s' % self.TempVMPath('kvm.monitor.serial'),
        '-mon', 'chardev=control_pipe',
    ])
    self.assertCommandContains([
        '-cpu', 'SandyBridge,-invpcid,-tsc-deadline,check',
    ])
    self.assertCommandContains([
        '-device', 'virtio-net,netdev=eth0',
        '-netdev', 'user,id=eth0,net=10.0.2.0/27,hostfwd=tcp:127.0.0.1:9222-:22'
    ])
    self.assertCommandContains([
        '-device', 'virtio-scsi-pci,id=scsi', '-device', 'scsi-hd,drive=hd',
        '-drive', 'if=none,id=hd,file=%s,cache=unsafe,format=raw'
        % self.TempFilePath('chromiumos_qemu_image.bin'),
    ])
    self.assertCommandContains(['-enable-kvm'])

  def testStop(self):
    pid = '12345'
    self.assertEqual(self._vm.pidfile, self.TempVMPath('kvm.pid'))
    osutils.WriteFile(self._vm.pidfile, pid)
    self._vm.Stop()
    self.assertCommandContains(['kill', '-9', pid])

  def testBuiltVMImagePath(self):
    """Verify locally built VM image path is picked up by vm.VM."""
    self._vm.image_path = None
    expected_vm_image_path = os.path.join(constants.SOURCE_ROOT,
                                          'src/build/images/%s/latest/'
                                          'chromiumos_qemu_image.bin'
                                          % self._vm.board)
    osutils.Touch(expected_vm_image_path, makedirs=True)
    self._vm.Start()
    self.assertTrue(self.FindPathInArgs(self.rc.call_args_list,
                                        expected_vm_image_path))

  def testSDKVMImagePath(self):
    """Verify vm.VM picks up the downloaded VM in the SDK."""
    self._vm.image_path = None
    cache_dir = cros_test_lib.FakeSDKCache(
        self._vm.cache_dir).CreateCacheReference(self._vm.board,
                                                 constants.VM_IMAGE_TAR)
    vm_cache_path = os.path.join(cache_dir, constants.VM_IMAGE_BIN)
    osutils.Touch(vm_cache_path, makedirs=True)
    self._vm.Start()
    expected_vm_image_path = os.path.join(
        self._vm.cache_dir,
        'chrome-sdk/tarballs/%s+12225.0.0+chromiumos_qemu_image.tar.xz/'
        'chromiumos_qemu_image.bin' % self._vm.board)
    self.assertTrue(self.FindPathInArgs(self.rc.call_args_list,
                                        expected_vm_image_path))

  def testVMImageNotFound(self):
    """Verify that VMError is raised when a fake board image cannot be found."""
    self._vm.image_path = None
    self._vm.board = 'fake_board_name'
    self.assertRaises(vm.VMError, self._vm.Start)

  def testVMImageDoesNotExist(self):
    """Verify that VMError is raised when image path is not real."""
    self._vm.image_path = '/fake/path/to/the/vm/image'
    self.assertRaises(vm.VMError, self._vm.Start)

  def testAppendBinFile(self):
    """When image-path points to a directory, we should append the bin file."""
    self._vm.image_path = self.tempdir
    self._vm.Start()
    self.assertEqual(self._vm.image_path,
                     self.TempFilePath('chromiumos_qemu_image.bin'))

  def testChrootQemuPath(self):
    """Verify that QEMU in the chroot is picked up by vm.VM."""
    if cros_build_lib.IsInsideChroot():
      self._vm._SetQemuPath()
      self.assertTrue('usr/bin/qemu-system-x86_64' in self._vm.qemu_path)

  def testSDKQemuPath(self):
    """Verify vm.VM picks up the downloaded QEMU in the SDK."""
    self._vm.qemu_path = None
    qemu_dir_path = cros_chrome_sdk.SDKFetcher.QEMU_BIN_PATH
    # Creates a fake SDK cache with the QEMU binary.
    qemu_path = cros_test_lib.FakeSDKCache(
        self._vm.cache_dir).CreateCacheReference(self._vm.board, qemu_dir_path)
    qemu_path = os.path.join(qemu_path,
                             'usr', 'bin', 'qemu-system-x86_64')
    osutils.Touch(qemu_path, makedirs=True)
    self._vm._SetQemuPath()
    self.assertEqual(self._vm.qemu_path, qemu_path)

  @mock.patch('chromite.lib.vm.VM._CheckQemuMinVersion')
  def testSystemQemuPath(self, check_min_version_mock):
    """Verify that QEMU in the system is picked up by vm.VM."""
    # Skip the SDK Cache.
    os.environ[cros_chrome_sdk.SDKFetcher.SDK_VERSION_ENV] = 'None'

    # Skip the Chroot.
    constants.SOURCE_ROOT = '/temp'

    # Checks the QEMU path in the system.
    qemu_path = self.TempFilePath('qemu-system-x86_64')
    osutils.Touch(qemu_path, mode=stat.S_IRWXU, makedirs=True)
    qemu_dir = os.path.dirname(qemu_path)

    os.environ['PATH'] = qemu_dir
    self._vm._SetQemuPath()

    # SetQemuPath() calls _CheckQemuMinVersion().
    # Checks that mock version has been called.
    check_min_version_mock.assert_called()

    self.assertEqual(self._vm.qemu_path, qemu_path)

  def testInvalidQemuBiosPath(self):
    """Verify that VMError is raised for nonexistent qemu bios path."""
    self._vm.qemu_bios_path = '/invalid/qemu/bios/path/'
    self.assertRaises(vm.VMError, self._vm.Start)

