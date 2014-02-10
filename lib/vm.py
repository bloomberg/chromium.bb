# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""VM-related helper functions/classes."""

import logging
import os
import time

from chromite.buildbot import constants
from chromite.lib import cros_build_lib
from chromite.lib import git
from chromite.lib import osutils
from chromite.lib import remote_access


class VMError(Exception):
  """A base exception for VM errors."""


class VMCreationError(VMError):
  """Raised when failed to create a VM image."""


def CreateVMImage(image=None, board=None, updatable=True):
  """Returns the path of the image built to run in a VM.

  By default, the returned VM is a test image that can run full update
  testing on it. If there exists a VM image with the matching
  |updatable| setting, this method does not return a new image.

  Args:
    image: Path to the image. Defaults to None to use the latest image
      for the board.
    board: Board that the image was built with. If None, attempts to use the
           configured default board.
    updatable: Create a VM image that supports AU.
  """
  if not image and not board:
    raise VMCreationError(
        'Cannot create VM when both image and board are None.')

  image_dir = os.path.dirname(image)
  vm_image_path = os.path.join(image_dir, constants.VM_IMAGE_BIN)
  # Do not create a new VM image if it already exists.
  if os.path.exists(vm_image_path):
    if updatable:
      # Check if the existing VM image is updatable.
      table = cros_build_lib.GetImageDiskPartitionInfo(vm_image_path, unit='MB')
      if table['ROOT-B'].size == table['ROOT-A'].size:
        # Assume if size of the two root partitions match, the image
        # is updatable.
        return vm_image_path
    else:
      return vm_image_path

  logging.info('Creating %s', vm_image_path)
  cmd = ['./image_to_vm.sh', '--test_image']

  if image:
    cmd.append(
        '--from=%s' % git.ReinterpretPathForChroot(image_dir))

  if updatable:
    cmd.extend(['--disk_layout', '2gb-rootfs-updatable'])

  if board:
    cmd.extend(['--board', board])

  msg = 'Failed to create the VM image.'
  try:
    cros_build_lib.RunCommand(cmd, enter_chroot=True, cwd=constants.SOURCE_ROOT)
  except:
    logging.error(msg)
    raise VMCreationError(msg)

  if not os.path.exists(vm_image_path):
    raise VMCreationError(msg)

  return vm_image_path


class VMStartupError(VMError):
  """Raised when failed to start a VM instance."""


class VMStopError(VMError):
  """Raised when failed to stop a VM instance."""


class VMInstance(object):
  """This is a wrapper of a VM instance."""

  MAX_LAUNCH_ATTEMPTS = 5
  TIME_BETWEEN_LAUNCH_ATTEMPTS = 30

  # VM needs a longer timeout.
  SSH_CONNECT_TIMEOUT = 120

  def __init__(self, image_path, port=None, tempdir=None,
               debug_level=logging.DEBUG):
    """Initializes VMWrapper with a VM image path.

    Args:
      image_path: Path to the VM image.
      port: SSH port of the VM.
      tempdir: Temporary working directory.
      debug_level: Debug level for logging.
    """
    self.image_path = image_path
    self.tempdir = tempdir
    self._tempdir_obj = None
    if not self.tempdir:
      self._tempdir_obj = osutils.TempDir(prefix='vm_wrapper', sudo_rm=True)
      self.tempdir = self._tempdir_obj.tempdir
    self.kvm_pid_path = os.path.join(self.tempdir, 'kvm.pid')
    self.port = (remote_access.GetUnusedPort() if port is None
                 else remote_access.NormalizePort(port))
    self.debug_level = debug_level
    self.ssh_settings = remote_access.CompileSSHConnectSettings(
        ConnectTimeout=self.SSH_CONNECT_TIMEOUT)
    self.agent = remote_access.RemoteAccess(
        remote_access.LOCALHOST, self.tempdir, self.port,
        debug_level=self.debug_level, interactive=False)

  def _Start(self):
    """Run the command to start VM."""
    cmd = [os.path.join(constants.CROSUTILS_DIR, 'bin', 'cros_start_vm'),
           '--ssh_port', str(self.port),
           '--image_path', self.image_path,
           '--no_graphics',
           '--kvm_pid', self.kvm_pid_path]
    try:
      self._RunCommand(cmd, capture_output=True)
    except cros_build_lib.RunCommandError:
      raise VMStartupError('VM failed to start.')

  def Connect(self):
    """Returns True if we can connect to VM via SSH."""
    try:
      self.agent.RemoteSh(['true'], connect_settings=self.ssh_settings)
    except Exception:
      return False

    return True

  def Stop(self, ignore_error=False):
    """Stops a running VM."""
    cmd = [os.path.join(constants.CROSUTILS_DIR, 'bin', 'cros_stop_vm'),
           '--kvm_pid', self.kvm_pid_path]
    result = self._RunCommand(cmd, capture_output=True, error_code_ok=True)
    if result.returncode:
      msg = 'Failed to stop VM'
      if ignore_error:
        logging.warning(msg)
      else:
        raise VMStopError(msg)

  def Start(self):
    """Start VM and wait until we can ssh into it.

    This command is more robust than just naively starting the VM as it will
    try to start the VM multiple times if the VM fails to start up. This is
    inspired by retry_until_ssh in crosutils/lib/cros_vm_lib.sh.
    """
    for _ in range(self.MAX_LAUNCH_ATTEMPTS):
      try:
        self._Start()
      except VMStartupError:
        logging.warning('VM failed to start.')
        continue

      if self.Connect():
        # VM is started up successfully if we can connect to it.
        break

      logging.warning('Cannot connect to VM...')
      self.Stop(ignore_error=True)
      time.sleep(self.TIME_BETWEEN_LAUNCH_ATTEMPTS)
    else:
      raise VMStartupError('Max attempts (%d) to start VM exceeded.'
                           % self.MAX_LAUNCH_ATTEMPTS)

    logging.info('VM started at port %d', self.port)

  def _RunCommand(self, *args, **kwargs):
    """Runs a commmand on the host machine."""
    kwargs.setdefault('debug_level', self.debug_level)
    return cros_build_lib.RunCommand(*args, **kwargs)
