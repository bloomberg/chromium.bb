# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""VM-related helper functions/classes."""

import logging
import os

from chromite.buildbot import constants
from chromite.lib import cros_build_lib
from chromite.lib import git
from chromite.lib import osutils
from chromite.lib import remote_access
from chromite.lib import timeout_util


class VMError(Exception):
  """A base exception for VM errors."""


class VMCreationError(VMError):
  """Raised when failed to create a VM image."""


# TODO(yjhong): This function was adapted from crostestutils.lib.test_helper.
# We want to migrate most of the callers to use this function instead.
def CreateVMImage(image=None, board=None, full=True):
  """Returns the path of the image built to run in a VM.

  By default, the returned VM is a test image that can run full update
  testing on it.  This method does not return a new image if one
  already existed.

  Args:
    image: Path to the image. Defaults to None to use the latest image
      for the board.
    board: Board that the image was built with. If None, attempts to use the
           configured default board.
    full: If the vm image doesn't exist, create a "full" one which supports AU.
  """
  if not image and not board:
    raise VMCreationError(
        'Cannot create VM when both image and board are None.')

  image_dir = os.path.dirname(image)
  vm_image_path = os.path.join(image_dir, constants.VM_IMAGE_BIN)
  if os.path.exists(vm_image_path):
    return vm_image_path

  logging.info('Creating %s', vm_image_path)
  cmd = ['./image_to_vm.sh', '--test_image']

  if image:
    cmd.append(
        '--from=%s' % git.ReinterpretPathForChroot(image_dir))

  if full:
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

  STARTUP_TIMEOUT = 60 * 5
  STARTUP_CHECK_INTERNAL = 30

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
      self.agent.RemoteSh(['true'])
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
    self._Start()

    def _log():
      logging.warning('Cannot connect to VM...')

    try:
      timeout_util.WaitForReturnTrue(self.Connect,
                                     self.STARTUP_TIMEOUT,
                                     period=self.STARTUP_CHECK_INTERNAL,
                                     side_effect_func=_log)
    except timeout_util.TimeoutError:
      self.Stop(ignore_error=True)
      raise timeout_util.TimeoutError(
          'Timeout connecting VM after %s seconds.' % self.STARTUP_TIMEOUT)

    logging.info('VM started at port %d', self.port)

  def _RunCommand(self, *args, **kwargs):
    """Runs a commmand on the host machine."""
    kwargs.setdefault('debug_level', self.debug_level)
    return cros_build_lib.RunCommand(*args, **kwargs)
