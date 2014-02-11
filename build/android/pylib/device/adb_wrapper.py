# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module wraps Android's adb tool.

This is a thin wrapper around the adb interface. Any additional complexity
should be delegated to a higher level (ex. DeviceUtils).
"""

import errno
import logging
import os

from pylib import cmd_helper

from pylib.utils import reraiser_thread
from pylib.utils import timeout_retry

_DEFAULT_TIMEOUT = 30
_DEFAULT_RETRIES = 2


class BaseError(Exception):
  """Base exception for all device and command errors."""
  pass


class CommandFailedError(BaseError):
  """Exception for command failures."""

  def __init__(self, cmd, msg, device=None):
    super(CommandFailedError, self).__init__(
        (('device %s: ' % device) if device else '') +
        'adb command \'%s\' failed with message: \'%s\'' % (' '.join(cmd), msg))


class CommandTimeoutError(BaseError):
  """Exception for command timeouts."""
  pass


def _VerifyLocalFileExists(path):
  """Verifies a local file exists.

  Args:
    path: Path to the local file.

  Raises:
    IOError: If the file doesn't exist.
  """
  if not os.path.exists(path):
    raise IOError(errno.ENOENT, os.strerror(errno.ENOENT), path)


class AdbWrapper(object):
  """A wrapper around a local Android Debug Bridge executable."""

  def __init__(self, device_serial):
    """Initializes the AdbWrapper.

    Args:
      device_serial: The device serial number as a string.
    """
    self._device_serial = str(device_serial)

  @classmethod
  def _AdbCmd(cls, arg_list, timeout, retries, check_error=True):
    """Runs an adb command with a timeout and retries.

    Args:
      arg_list: A list of arguments to adb.
      timeout: Timeout in seconds.
      retries: Number of retries.
      check_error: Check that the command doesn't return an error message. This
        does NOT check the return code of shell commands.

    Returns:
      The output of the command.
    """
    cmd = ['adb'] + arg_list

    # This method runs inside the timeout/retries.
    def RunCmd():
      exit_code, output = cmd_helper.GetCmdStatusAndOutput(cmd)
      if exit_code != 0:
        raise CommandFailedError(
            cmd, 'returned non-zero exit code %s, output: %s' %
            (exit_code, output))
      # This catches some errors, including when the device drops offline;
      # unfortunately adb is very inconsistent with error reporting so many
      # command failures present differently.
      if check_error and output[:len('error:')] == 'error:':
        raise CommandFailedError(arg_list, output)
      return output

    try:
      return timeout_retry.Run(RunCmd, timeout, retries)
    except reraiser_thread.TimeoutError as e:
      raise CommandTimeoutError(str(e))

  def _DeviceAdbCmd(self, arg_list, timeout, retries, check_error=True):
    """Runs an adb command on the device associated with this object.

    Args:
      arg_list: A list of arguments to adb.
      timeout: Timeout in seconds.
      retries: Number of retries.
      check_error: Check that the command doesn't return an error message. This
        does NOT check the return code of shell commands.

    Returns:
      The output of the command.
    """
    return self._AdbCmd(
        ['-s', self._device_serial] + arg_list, timeout, retries,
        check_error=check_error)

  def __eq__(self, other):
    """Consider instances equal if they refer to the same device.

    Args:
      other: The instance to compare equality with.

    Returns:
      True if the instances are considered equal, false otherwise.
    """
    return self._device_serial == str(other)

  def __str__(self):
    """The string representation of an instance.

    Returns:
      The device serial number as a string.
    """
    return self._device_serial

  def __repr__(self):
    return '%s(\'%s\')' % (self.__class__.__name__, self)

  # TODO(craigdh): Determine the filter criteria that should be supported.
  @classmethod
  def GetDevices(cls, timeout=_DEFAULT_TIMEOUT, retries=_DEFAULT_RETRIES):
    """Get the list of active attached devices.

    Args:
      timeout: (optional) Timeout per try in seconds.
      retries: (optional) Number of retries to attempt.

    Yields:
      AdbWrapper instances.
    """
    output = cls._AdbCmd(['devices'], timeout, retries)
    lines = [line.split() for line in output.split('\n')]
    return [AdbWrapper(line[0]) for line in lines
            if len(line) == 2 and line[1] == 'device'] 

  def GetDeviceSerial(self):
    """Gets the device serial number associated with this object.

    Returns:
      Device serial number as a string.
    """
    return self._device_serial

  def Push(self, local, remote, timeout=60*5, retries=_DEFAULT_RETRIES):
    """Pushes a file from the host to the device.

    Args:
      local: Path on the host filesystem.
      remote: Path on the device filesystem.
      timeout: (optional) Timeout per try in seconds.
      retries: (optional) Number of retries to attempt.
    """
    _VerifyLocalFileExists(local)
    self._DeviceAdbCmd(['push', local, remote], timeout, retries)

  def Pull(self, remote, local, timeout=60*5, retries=_DEFAULT_RETRIES):
    """Pulls a file from the device to the host.

    Args:
      remote: Path on the device filesystem.
      local: Path on the host filesystem.
      timeout: (optional) Timeout per try in seconds.
      retries: (optional) Number of retries to attempt.
    """
    self._DeviceAdbCmd(['pull', remote, local], timeout, retries)
    _VerifyLocalFileExists(local)

  def Shell(self, command, expect_rc=None, timeout=_DEFAULT_TIMEOUT,
            retries=_DEFAULT_RETRIES):
    """Runs a shell command on the device.

    Args:
      command: The shell command to run.
      expect_rc: (optional) If set checks that the command's return code matches
        this value.
      timeout: (optional) Timeout per try in seconds.
      retries: (optional) Number of retries to attempt.

    Returns:
      The output of the shell command as a string.

    Raises:
      CommandFailedError: If the return code doesn't match |expect_rc|.
    """
    if expect_rc is None:
      actual_command = command
    else:
      actual_command = '%s; echo $?;' % command
    output = self._DeviceAdbCmd(
        ['shell', actual_command], timeout, retries, check_error=False)
    if expect_rc is not None:
      output_end = output.rstrip().rfind('\n') + 1
      rc = output[output_end:].strip()
      output = output[:output_end]
      if int(rc) != expect_rc:
        raise CommandFailedError(
            ['shell', command],
            'shell command exited with code: %s' % rc,
            self._device_serial)
    return output

  def Logcat(self, filter_spec=None, timeout=_DEFAULT_TIMEOUT,
             retries=_DEFAULT_RETRIES):
    """Get the logcat output.

    Args:
      filter_spec: (optional) Spec to filter the logcat.
      timeout: (optional) Timeout per try in seconds.
      retries: (optional) Number of retries to attempt.

    Returns:
      logcat output as a string.
    """
    cmd = ['logcat']
    if filter_spec is not None:
      cmd.append(filter_spec)
    return self._DeviceAdbCmd(cmd, timeout, retries, check_error=False)

  def Forward(self, local, remote, timeout=_DEFAULT_TIMEOUT,
              retries=_DEFAULT_RETRIES):
    """Forward socket connections from the local socket to the remote socket.

    Sockets are specified by one of:
      tcp:<port>
      localabstract:<unix domain socket name>
      localreserved:<unix domain socket name>
      localfilesystem:<unix domain socket name>
      dev:<character device name>
      jdwp:<process pid> (remote only)

    Args:
      local: The host socket.
      remote: The device socket.
      timeout: (optional) Timeout per try in seconds.
      retries: (optional) Number of retries to attempt.
    """
    self._DeviceAdbCmd(['forward', str(local), str(remote)], timeout, retries)

  def JDWP(self, timeout=_DEFAULT_TIMEOUT, retries=_DEFAULT_RETRIES):
    """List of PIDs of processes hosting a JDWP transport.

    Args:
      timeout: (optional) Timeout per try in seconds.
      retries: (optional) Number of retries to attempt.

    Returns:
      A list of PIDs as strings.
    """
    return [a.strip() for a in
            self._DeviceAdbCmd(['jdwp'], timeout, retries).split('\n')]

  def Install(self, apk_path, forward_lock=False, reinstall=False,
              sd_card=False, timeout=60*2, retries=_DEFAULT_RETRIES):
    """Install an apk on the device.

    Args:
      apk_path: Host path to the APK file.
      forward_lock: (optional) If set forward-locks the app.
      reinstall: (optional) If set reinstalls the app, keeping its data.
      sd_card: (optional) If set installs on the SD card.
      timeout: (optional) Timeout per try in seconds.
      retries: (optional) Number of retries to attempt.
    """
    _VerifyLocalFileExists(apk_path)
    cmd = ['install']
    if forward_lock:
      cmd.append('-l')
    if reinstall:
      cmd.append('-r')
    if sd_card:
      cmd.append('-s')
    cmd.append(apk_path)
    output = self._DeviceAdbCmd(cmd, timeout, retries)
    if 'Success' not in output:
      raise CommandFailedError(cmd, output)

  def Uninstall(self, package, keep_data=False, timeout=_DEFAULT_TIMEOUT,
                retries=_DEFAULT_RETRIES):
    """Remove the app |package| from the device.

    Args:
      package: The package to uninstall.
      keep_data: (optional) If set keep the data and cache directories.
      timeout: (optional) Timeout per try in seconds.
      retries: (optional) Number of retries to attempt.
    """
    cmd = ['uninstall']
    if keep_data:
      cmd.append('-k')
    cmd.append(package)
    output = self._DeviceAdbCmd(cmd, timeout, retries)
    if 'Failure' in output:
      raise CommandFailedError(cmd, output)

  def Backup(self, path, packages=None, apk=False, shared=False,
             nosystem=True, include_all=False, timeout=_DEFAULT_TIMEOUT,
             retries=_DEFAULT_RETRIES):
    """Write an archive of the device's data to |path|.

    Args:
      path: Local path to store the backup file.
      packages: List of to packages to be backed up.
      apk: (optional) If set include the .apk files in the archive.
      shared: (optional) If set buckup the device's SD card.
      nosystem: (optional) If set exclude system applications.
      include_all: (optional) If set back up all installed applications and
        |packages| is optional.
      timeout: (optional) Timeout per try in seconds.
      retries: (optional) Number of retries to attempt.
    """
    cmd = ['backup', path]
    if apk:
      cmd.append('-apk')
    if shared:
      cmd.append('-shared')
    if nosystem:
      cmd.append('-nosystem')
    if include_all:
      cmd.append('-all')
    if packages:
      cmd.extend(packages)
    assert bool(packages) ^ bool(include_all), (
        'Provide \'packages\' or set \'include_all\' but not both.')
    ret = self._DeviceAdbCmd(cmd, timeout, retries)
    _VerifyLocalFileExists(path)
    return ret

  def Restore(self, path, timeout=_DEFAULT_TIMEOUT, retries=_DEFAULT_RETRIES):
    """Restore device contents from the backup archive.

    Args:
      path: Host path to the backup archive.
      timeout: (optional) Timeout per try in seconds.
      retries: (optional) Number of retries to attempt.
    """
    _VerifyLocalFileExists(path)
    self._DeviceAdbCmd(['restore'] + [path], timeout, retries)

  def WaitForDevice(self, timeout=60*5, retries=_DEFAULT_RETRIES):
    """Block until the device is online.

    Args:
      timeout: (optional) Timeout per try in seconds.
      retries: (optional) Number of retries to attempt.
    """
    self._DeviceAdbCmd(['wait-for-device'], timeout, retries)

  def GetState(self, timeout=_DEFAULT_TIMEOUT, retries=_DEFAULT_RETRIES):
    """Get device state.

    Args:
      timeout: (optional) Timeout per try in seconds.
      retries: (optional) Number of retries to attempt.

    Returns:
      One of 'offline', 'bootloader', or 'device'.
    """
    return self._DeviceAdbCmd(['get-state'], timeout, retries).strip()

  def GetDevPath(self, timeout=_DEFAULT_TIMEOUT, retries=_DEFAULT_RETRIES):
    """Gets the device path.

    Args:
      timeout: (optional) Timeout per try in seconds.
      retries: (optional) Number of retries to attempt.

    Returns:
      The device path (e.g. usb:3-4)
    """
    return self._DeviceAdbCmd(['get-devpath'], timeout, retries)

  def Remount(self, timeout=_DEFAULT_TIMEOUT, retries=_DEFAULT_RETRIES):
    """Remounts the /system partition on the device read-write."""
    self._DeviceAdbCmd(['remount'], timeout, retries)

  def Reboot(self, to_bootloader=False, timeout=60*5,
             retries=_DEFAULT_RETRIES):
    """Reboots the device.

    Args:
      to_bootloader: (optional) If set reboots to the bootloader.
      timeout: (optional) Timeout per try in seconds.
      retries: (optional) Number of retries to attempt.
    """
    if to_bootloader:
      cmd = ['reboot-bootloader']
    else:
      cmd = ['reboot']
    self._DeviceAdbCmd(cmd, timeout, retries)

  def Root(self, timeout=_DEFAULT_TIMEOUT, retries=_DEFAULT_RETRIES):
    """Restarts the adbd daemon with root permissions, if possible.

    Args:
      timeout: (optional) Timeout per try in seconds.
      retries: (optional) Number of retries to attempt.
    """
    output = self._DeviceAdbCmd(['root'], timeout, retries)
    if 'cannot' in output:
      raise CommandFailedError(['root'], output)

