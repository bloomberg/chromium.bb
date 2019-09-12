# -*- coding: utf-8 -*-
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""VM-related helper functions/classes."""

from __future__ import print_function

import distutils.version  # pylint: disable=import-error,no-name-in-module
import errno
import multiprocessing
import os
import re
import shutil
import socket

from chromite.cli.cros import cros_chrome_sdk
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import device
from chromite.lib import osutils
from chromite.lib import path_util
from chromite.lib import remote_access
from chromite.lib import retry_util
from chromite.utils import memoize


class VMError(device.DeviceError):
  """Exception for VM errors."""


def VMIsUpdatable(path):
  """Check if the existing VM image is updatable.

  Args:
    path: Path to the VM image.

  Returns:
    True if VM is updatable; False otherwise.
  """
  table = {p.name: p for p in cros_build_lib.GetImageDiskPartitionInfo(path)}
  # Assume if size of the two root partitions match, the image
  # is updatable.
  return table['ROOT-B'].size == table['ROOT-A'].size


def CreateVMImage(image=None, board=None, updatable=True, dest_dir=None):
  """Returns the path of the image built to run in a VM.

  By default, the returned VM is a test image that can run full update
  testing on it. If there exists a VM image with the matching
  |updatable| setting, this method returns the path to the existing
  image. If |dest_dir| is set, it will copy/create the VM image to the
  |dest_dir|.

  Args:
    image: Path to the (non-VM) image. Defaults to None to use the latest
      image for the board.
    board: Board that the image was built with. If None, attempts to use the
      configured default board.
    updatable: Create a VM image that supports AU.
    dest_dir: If set, create/copy the VM image to |dest|; otherwise,
      use the folder where |image| resides.
  """
  if not image and not board:
    raise VMError('Cannot create VM when both image and board are None.')

  image_dir = os.path.dirname(image)
  src_path = dest_path = os.path.join(image_dir, constants.VM_IMAGE_BIN)

  if dest_dir:
    dest_path = os.path.join(dest_dir, constants.VM_IMAGE_BIN)

  exists = False
  # Do not create a new VM image if a matching image already exists.
  exists = os.path.exists(src_path) and (
      not updatable or VMIsUpdatable(src_path))

  if exists and dest_dir:
    # Copy the existing VM image to dest_dir.
    shutil.copyfile(src_path, dest_path)

  if not exists:
    # No existing VM image that we can reuse. Create a new VM image.
    logging.info('Creating %s', dest_path)
    cmd = [os.path.join(constants.CROSUTILS_DIR, 'image_to_vm.sh'),
           '--test_image']

    if image:
      cmd.append('--from=%s' % path_util.ToChrootPath(image_dir))

    if updatable:
      cmd.extend(['--disk_layout', '2gb-rootfs-updatable'])

    if board:
      cmd.extend(['--board', board])

    # image_to_vm.sh only runs in chroot, but dest_dir may not be
    # reachable from chroot. In that case, we copy it to a temporary
    # directory in chroot, and then move it to dest_dir .
    tempdir = None
    if dest_dir:
      # Create a temporary directory in chroot to store the VM
      # image. This is to avoid the case where dest_dir is not
      # reachable within chroot.
      tempdir = cros_build_lib.RunCommand(
          ['mktemp', '-d'],
          capture_output=True,
          enter_chroot=True).output.strip()
      cmd.append('--to=%s' % tempdir)

    msg = 'Failed to create the VM image'
    try:
      cros_build_lib.RunCommand(cmd, enter_chroot=True,
                                cwd=constants.SOURCE_ROOT)
    except cros_build_lib.RunCommandError as e:
      logging.error('%s: %s', msg, e)
      if tempdir:
        osutils.RmDir(
            path_util.FromChrootPath(tempdir), ignore_missing=True)
      raise VMError(msg)

    if dest_dir:
      # Move VM from tempdir to dest_dir.
      shutil.move(
          path_util.FromChrootPath(
              os.path.join(tempdir, constants.VM_IMAGE_BIN)), dest_path)
      osutils.RmDir(path_util.FromChrootPath(tempdir), ignore_missing=True)

  if not os.path.exists(dest_path):
    raise VMError(msg)

  return dest_path


class VM(device.Device):
  """Class for managing a VM."""

  SSH_PORT = 9222
  IMAGE_FORMAT = 'raw'

  def __init__(self, opts):
    """Initialize VM.

    Args:
      opts: command line options.
    """
    super(VM, self).__init__(opts)

    self.qemu_path = opts.qemu_path
    self.qemu_img_path = opts.qemu_img_path
    self.qemu_bios_path = opts.qemu_bios_path
    self.qemu_m = opts.qemu_m
    self.qemu_cpu = opts.qemu_cpu
    self.qemu_smp = opts.qemu_smp
    if self.qemu_smp == 0:
      self.qemu_smp = min(8, multiprocessing.cpu_count)
    self.qemu_hostfwd = opts.qemu_hostfwd
    self.qemu_args = opts.qemu_args

    self.enable_kvm = opts.enable_kvm
    self.copy_on_write = opts.copy_on_write
    # We don't need sudo access for software emulation or if /dev/kvm is
    # writeable.
    self.use_sudo = self.enable_kvm and not os.access('/dev/kvm', os.W_OK)
    self.display = opts.display
    self.image_path = opts.image_path
    self.image_format = opts.image_format

    self.device = remote_access.LOCALHOST
    self.ssh_port = opts.ssh_port

    self.start = opts.start
    self.stop = opts.stop

    self.chroot_path = opts.chroot_path

    self.cache_dir = os.path.abspath(opts.cache_dir)
    assert os.path.isdir(self.cache_dir), "Cache directory doesn't exist"

    self.vm_dir = opts.vm_dir
    if not self.vm_dir:
      self.vm_dir = os.path.join(osutils.GetGlobalTempDir(),
                                 'cros_vm_%d' % self.ssh_port)
    self._CreateVMDir()

    self.pidfile = os.path.join(self.vm_dir, 'kvm.pid')
    self.kvm_monitor = os.path.join(self.vm_dir, 'kvm.monitor')
    self.kvm_pipe_in = '%s.in' % self.kvm_monitor  # to KVM
    self.kvm_pipe_out = '%s.out' % self.kvm_monitor  # from KVM
    self.kvm_serial = '%s.serial' % self.kvm_monitor

    self.InitRemote()

  def _CreateVMDir(self):
    """Safely create vm_dir."""
    if not osutils.SafeMakedirs(self.vm_dir):
      # For security, ensure that vm_dir is not a symlink, and is owned by us.
      error_str = ('VM state dir is misconfigured; please recreate: %s'
                   % self.vm_dir)
      assert os.path.isdir(self.vm_dir), error_str
      assert not os.path.islink(self.vm_dir), error_str
      assert os.stat(self.vm_dir).st_uid == os.getuid(), error_str

  def _CreateQcow2Image(self):
    """Creates a qcow2-formatted image in the temporary VM dir.

    This image will get removed on VM shutdown.
    """
    cow_image_path = os.path.join(self.vm_dir, 'qcow2.img')
    qemu_img_args = [
        self.qemu_img_path,
        'create', '-f', 'qcow2',
        '-o', 'backing_file=%s' % self.image_path,
        cow_image_path,
    ]
    self.RunCommand(qemu_img_args)
    logging.info('qcow2 image created at %s.', cow_image_path)
    self.image_path = cow_image_path
    self.image_format = 'qcow2'

  def _RmVMDir(self):
    """Cleanup vm_dir."""
    osutils.RmDir(self.vm_dir, ignore_missing=True, sudo=self.use_sudo)

  @memoize.MemoizedSingleCall
  def QemuVersion(self):
    """Determine QEMU version.

    Returns:
      QEMU version.
    """
    version_str = self.RunCommand([self.qemu_path, '--version'],
                                  capture_output=True).output
    # version string looks like one of these:
    # QEMU emulator version 2.0.0 (Debian 2.0.0+dfsg-2ubuntu1.36), Copyright (c)
    # 2003-2008 Fabrice Bellard
    #
    # QEMU emulator version 2.6.0, Copyright (c) 2003-2008 Fabrice Bellard
    #
    # qemu-x86_64 version 2.10.1
    # Copyright (c) 2003-2017 Fabrice Bellard and the QEMU Project developers
    m = re.search(r'version ([0-9.]+)', version_str)
    if not m:
      raise VMError('Unable to determine QEMU version from:\n%s.' % version_str)
    return m.group(1)

  def _CheckQemuMinVersion(self):
    """Ensure minimum QEMU version."""
    if self.dry_run:
      return
    min_qemu_version = '2.6.0'
    logging.info('QEMU version %s', self.QemuVersion())
    LooseVersion = distutils.version.LooseVersion
    if LooseVersion(self.QemuVersion()) < LooseVersion(min_qemu_version):
      raise VMError('QEMU %s is the minimum supported version. You have %s.'
                    % (min_qemu_version, self.QemuVersion()))

  def _SetQemuPath(self):
    """Find a suitable Qemu executable."""
    qemu_exe = 'qemu-system-x86_64'
    qemu_exe_path = os.path.join('usr/bin', qemu_exe)

    # Check SDK cache.
    if not self.qemu_path:
      qemu_dir = cros_chrome_sdk.SDKFetcher.GetCachePath(
          cros_chrome_sdk.SDKFetcher.QEMU_BIN_PATH, self.cache_dir, self.board)
      if qemu_dir:
        qemu_path = os.path.join(qemu_dir, qemu_exe_path)
        if os.path.isfile(qemu_path):
          self.qemu_path = qemu_path

    # Check chroot.
    if not self.qemu_path:
      qemu_path = os.path.join(self.chroot_path, qemu_exe_path)
      if os.path.isfile(qemu_path):
        self.qemu_path = qemu_path

    # Check system.
    if not self.qemu_path:
      self.qemu_path = osutils.Which(qemu_exe)

    if not self.qemu_path or not os.path.isfile(self.qemu_path):
      raise VMError('QEMU not found.')

    if self.copy_on_write:
      if not self.qemu_img_path:
        # Look for qemu-img right next to qemu-system-x86_64.
        self.qemu_img_path = os.path.join(
            os.path.dirname(self.qemu_path), 'qemu-img')
      if not os.path.isfile(self.qemu_img_path):
        raise VMError('qemu-img not found. (Needed to create qcow2 image).')

    logging.debug('QEMU path: %s', self.qemu_path)
    self._CheckQemuMinVersion()

  def _GetBuiltVMImagePath(self):
    """Get path of a locally built VM image."""
    vm_image_path = os.path.join(
        constants.SOURCE_ROOT, 'src/build/images',
        cros_build_lib.GetBoard(self.board, strict=True),
        'latest', constants.VM_IMAGE_BIN)
    return vm_image_path if os.path.isfile(vm_image_path) else None

  def _GetCacheVMImagePath(self):
    """Get path of a cached VM image."""
    cache_path = cros_chrome_sdk.SDKFetcher.GetCachePath(
        constants.VM_IMAGE_TAR, self.cache_dir, self.board)
    if cache_path:
      vm_image = os.path.join(cache_path, constants.VM_IMAGE_BIN)
      if os.path.isfile(vm_image):
        return vm_image
    return None

  def _SetVMImagePath(self):
    """Detect VM image path in SDK and chroot."""
    if not self.image_path:
      self.image_path = (self._GetCacheVMImagePath() or
                         self._GetBuiltVMImagePath())
    if not self.image_path:
      raise VMError('No VM image found. Use cros chrome-sdk --download-vm.')
    if not os.path.isfile(self.image_path):
      # Checks if the image path points to a directory containing the bin file.
      image_path = os.path.join(self.image_path, constants.VM_IMAGE_BIN)
      if os.path.isfile(image_path):
        self.image_path = image_path
      else:
        raise VMError('VM image does not exist: %s' % self.image_path)
    logging.debug('VM image path: %s', self.image_path)

  def _WaitForSSHPort(self, sleep=5):
    """Wait for SSH port to become available."""
    class _SSHPortInUseError(Exception):
      """Exception for _CheckSSHPortBusy to throw."""

    def _CheckSSHPortBusy(ssh_port):
      """Check if the SSH port is in use."""
      sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
      try:
        sock.bind((remote_access.LOCALHOST_IP, ssh_port))
      except socket.error as e:
        if e.errno == errno.EADDRINUSE:
          logging.info('SSH port %d in use...', self.ssh_port)
          raise _SSHPortInUseError()
      finally:
        sock.close()

    try:
      retry_util.RetryException(
          exception=_SSHPortInUseError,
          max_retry=10,
          functor=lambda: _CheckSSHPortBusy(self.ssh_port),
          sleep=sleep)
    except _SSHPortInUseError:
      raise VMError('SSH port %d in use' % self.ssh_port)

  def Run(self):
    """Performs an action, one of start, stop, or run a command in the VM."""
    if not self.start and not self.stop and not self.cmd:
      raise VMError('Must specify one of start, stop, or cmd.')

    if self.start:
      self.Start()
    if self.cmd:
      if not self.IsRunning():
        raise VMError('VM not running.')
      self.RemoteCommand(self.cmd, stream_output=True)
    if self.stop:
      self.Stop()

  def Start(self):
    """Start the VM."""

    self.Stop()

    logging.debug('Start VM')
    self._SetQemuPath()
    self._SetVMImagePath()

    self._CreateVMDir()
    if self.copy_on_write:
      self._CreateQcow2Image()
    # Make sure we can read these files later on by creating them as ourselves.
    osutils.Touch(self.kvm_serial)
    for pipe in [self.kvm_pipe_in, self.kvm_pipe_out]:
      os.mkfifo(pipe, 0o600)
    osutils.Touch(self.pidfile)

    qemu_args = [self.qemu_path]
    if self.qemu_bios_path:
      if not os.path.isdir(self.qemu_bios_path):
        raise VMError('Invalid QEMU bios path: %s' % self.qemu_bios_path)
      qemu_args += ['-L', self.qemu_bios_path]

    qemu_args += [
        '-m', self.qemu_m, '-smp', str(self.qemu_smp), '-vga', 'virtio',
        '-daemonize', '-usbdevice', 'tablet',
        '-pidfile', self.pidfile,
        '-chardev', 'pipe,id=control_pipe,path=%s' % self.kvm_monitor,
        '-serial', 'file:%s' % self.kvm_serial,
        '-mon', 'chardev=control_pipe',
        # Append 'check' to warn if the requested CPU is not fully supported.
        '-cpu', self.qemu_cpu + ',check',
        '-device', 'virtio-net,netdev=eth0',
        '-device', 'virtio-scsi-pci,id=scsi',
        '-device', 'scsi-hd,drive=hd',
        '-drive', 'if=none,id=hd,file=%s,cache=unsafe,format=%s'
        % (self.image_path, self.image_format),
    ]
    # netdev args, including hostfwds.
    netdev_args = ('user,id=eth0,net=10.0.2.0/27,hostfwd=tcp:%s:%d-:%d'
                   % (remote_access.LOCALHOST_IP, self.ssh_port,
                      remote_access.DEFAULT_SSH_PORT))
    if self.qemu_hostfwd:
      for hostfwd in self.qemu_hostfwd:
        netdev_args += ',hostfwd=%s' % hostfwd
    qemu_args += ['-netdev', netdev_args]

    if self.qemu_args:
      for arg in self.qemu_args:
        qemu_args += arg.split()
    if self.enable_kvm:
      qemu_args += ['-enable-kvm']
    if not self.display:
      qemu_args += ['-display', 'none']
    logging.info('Pid file: %s', self.pidfile)
    self.RunCommand(qemu_args)
    self.WaitForBoot()

  def _GetVMPid(self):
    """Get the pid of the VM.

    Returns:
      pid of the VM.
    """
    if not os.path.exists(self.vm_dir):
      logging.debug('%s not present.', self.vm_dir)
      return 0

    if not os.path.exists(self.pidfile):
      logging.info('%s does not exist.', self.pidfile)
      return 0

    pid = osutils.ReadFile(self.pidfile).rstrip()
    if not pid.isdigit():
      # Ignore blank/empty files.
      if pid:
        logging.error('%s in %s is not a pid.', pid, self.pidfile)
      return 0

    return int(pid)

  def IsRunning(self):
    """Returns True if there's a running VM.

    Returns:
      True if there's a running VM.
    """
    pid = self._GetVMPid()
    if not pid:
      return False

    # Make sure the process actually exists.
    return os.path.isdir('/proc/%i' % pid)

  def _KillVM(self):
    """Kill the VM process."""
    pid = self._GetVMPid()
    if pid:
      self.RunCommand(['kill', '-9', str(pid)], error_code_ok=True)

  def Stop(self):
    """Stop the VM."""
    logging.debug('Stop VM')

    self._KillVM()
    self._WaitForSSHPort()
    self._RmVMDir()

  def _WaitForProcs(self, sleep=2):
    """Wait for expected processes to launch."""
    class _TooFewPidsException(Exception):
      """Exception for _GetRunningPids to throw."""

    def _GetRunningPids(exe, numpids):
      pids = self.remote.GetRunningPids(exe, full_path=False)
      logging.info('%s pids: %s', exe, repr(pids))
      if len(pids) < numpids:
        raise _TooFewPidsException()

    def _WaitForProc(exe, numpids):
      try:
        retry_util.RetryException(
            exception=_TooFewPidsException,
            max_retry=5,
            functor=lambda: _GetRunningPids(exe, numpids),
            sleep=sleep)
      except _TooFewPidsException:
        raise VMError('_WaitForProcs failed: timed out while waiting for '
                      '%d %s processes to start.' % (numpids, exe))

    # We could also wait for session_manager, nacl_helper, etc, but chrome is
    # the long pole. We expect the parent, 2 zygotes, gpu-process,
    # utility-process, 3 renderers.
    _WaitForProc('chrome', 8)

  def WaitForBoot(self, sleep=5):
    """Wait for the VM to boot up.

    Wait for ssh connection to become active, and wait for all expected chrome
    processes to be launched.
    """
    if not os.path.exists(self.vm_dir):
      self.Start()

    super(VM, self).WaitForBoot(sleep=sleep)

    # Chrome can take a while to start with software emulation.
    if not self.enable_kvm:
      self._WaitForProcs()

  @staticmethod
  def GetParser():
    """Parse a list of args.

    Args:
      argv: list of command line arguments.

    Returns:
      List of parsed opts.
    """
    parser = device.Device.GetParser()
    parser.add_argument('--start', action='store_true', default=False,
                        help='Start the VM.')
    parser.add_argument('--stop', action='store_true', default=False,
                        help='Stop the VM.')
    parser.add_argument('--image-path', type='path',
                        help='Path to VM image to launch with --start.')
    parser.add_argument('--image-format', default=VM.IMAGE_FORMAT,
                        help='Format of the VM image (raw, qcow2, ...).')
    parser.add_argument('--qemu-path', type='path',
                        help='Path of qemu binary to launch with --start.')
    parser.add_argument('--qemu-m', type=str, default='8G',
                        help='Memory argument that will be passed to qemu.')
    parser.add_argument('--qemu-smp', type=int, default='0',
                        help='SMP argument that will be passed to qemu. (0 '
                             'means auto-detection.)')
    # TODO(pwang): replace SandyBridge to Haswell-noTSX once lab machine
    # running VMTest all migrate to GCE.
    parser.add_argument('--qemu-cpu', type=str,
                        default='SandyBridge,-invpcid,-tsc-deadline',
                        help='CPU argument that will be passed to qemu.')
    parser.add_argument('--qemu-bios-path', type='path',
                        help='Path of directory with qemu bios files.')
    parser.add_argument('--qemu-hostfwd', action='append',
                        help='Ports to forward from the VM to the host in the '
                        'QEMU hostfwd format, eg tcp:127.0.0.1:12345-:54321 to '
                        'forward port 54321 on the VM to 12345 on the host.')
    parser.add_argument('--qemu-args', action='append',
                        help='Additional args to pass to qemu.')
    parser.add_argument('--copy-on-write', action='store_true', default=False,
                        help='Generates a temporary copy-on-write image backed '
                             'by the normal boot image. All filesystem changes '
                             'will instead be reflected in the temporary '
                             'image.')
    parser.add_argument('--qemu-img-path', type='path',
                        help='Path to qemu-img binary used to create temporary '
                             'copy-on-write images.')
    parser.add_argument('--disable-kvm', dest='enable_kvm',
                        action='store_false', default=True,
                        help='Disable KVM, use software emulation.')
    parser.add_argument('--no-display', dest='display',
                        action='store_false', default=True,
                        help='Do not display video output.')
    parser.add_argument('--ssh-port', type=int, default=VM.SSH_PORT,
                        help='ssh port to communicate with VM.')
    parser.add_argument('--chroot-path', type='path',
                        default=os.path.join(constants.SOURCE_ROOT,
                                             constants.DEFAULT_CHROOT_DIR))
    parser.add_argument('--cache-dir', type='path',
                        default=path_util.GetCacheDir(),
                        help='Cache directory to use.')
    parser.add_argument('--vm-dir', type='path',
                        help='Temp VM directory to use.')
    return parser
