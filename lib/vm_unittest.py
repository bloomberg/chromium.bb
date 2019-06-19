# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for VM."""

from __future__ import print_function

import os

from chromite.cli.cros import cros_chrome_sdk
from chromite.lib import cache
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

  def CreateFakeSDKCache(self, key):
    """Creates a fake SDK Cache.

    Args:
      key: Key of item in the tarball cache.

    Returns:
      Path to the cache directory.
    """
    # Sets SDK Version.
    sdk_version = '12225.0.0'
    os.environ[cros_chrome_sdk.SDKFetcher.SDK_VERSION_ENV] = sdk_version
    # Defines the path for the fake SDK cache.
    tarball_cache_path = os.path.join(self._vm.cache_dir,
                                      cros_chrome_sdk.COMMAND_NAME,
                                      cros_chrome_sdk.SDKFetcher.TARBALL_CACHE)
    # Creates a SDK TarballCache instance.
    tarball_cache = cache.TarballCache(tarball_cache_path)
    # Creates the cache key required for accessing the fake SDK cache.
    cache_key = (self._vm.board, sdk_version, key)
    # Adds the cache path at the key.
    cache.CacheReference(tarball_cache, cache_key).Assign(tarball_cache_path)
    cache_path = tarball_cache.Lookup(cache_key).path
    expected_cache_path = os.path.join(self._vm.cache_dir,
                                       'chrome-sdk/tarballs',
                                       '+'.join(cache_key))
    self.assertEqual(cache_path, expected_cache_path)
    return cache_path

  def Touch(self, path):
    """Creates the parent directories and the file for the path."""
    assert path.startswith(path), 'All files must be in tempdir.'
    osutils.SafeMakedirs(os.path.dirname(path))
    osutils.Touch(path)

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

  def testAppendBinFile(self):
    """When image-path points to a directory, we should append the bin file."""
    self._vm.image_path = self.tempdir
    self._vm.Start()
    self.assertEqual(self._vm.image_path,
                     self.TempFilePath('chromiumos_qemu_image.bin'))
