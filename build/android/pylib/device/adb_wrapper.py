# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module wraps Android's adb tool.

This is a thin wrapper around the adb interface. Any additional complexity
should be delegated to a higher level (ex. DeviceUtils).
"""

import errno
import os

from pylib import cmd_helper
from pylib import constants
from pylib.device import decorators
from pylib.device import device_errors
from pylib.utils import timeout_retry


_DEFAULT_TIMEOUT = 30
_DEFAULT_RETRIES = 2


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

  # pylint: disable=unused-argument
  @classmethod
  @decorators.WithTimeoutAndRetries
  def _RunAdbCmd(cls, args, timeout=None, retries=None, device_serial=None,
                 check_error=True):
    cmd = [constants.GetAdbPath()]
    if device_serial is not None:
      cmd.extend(['-s', device_serial])
    cmd.extend(args)
    status, output = cmd_helper.GetCmdStatusAndOutputWithTimeout(
        cmd, timeout_retry.CurrentTimeoutThread().GetRemainingTime())
    if status != 0:
      raise device_errors.AdbCommandFailedError(
          args, output, status, device_serial)
    # This catches some errors, including when the device drops offline;
    # unfortunately adb is very inconsistent with error reporting so many
    # command failures present differently.
    if check_error and output.startswith('error:'):
      raise device_errors.AdbCommandFailedError(args, output)
    return output
  # pylint: enable=unused-argument

  def _RunDeviceAdbCmd(self, args, timeout, retries, check_error=True):
    """Runs an adb command on the device associated with this object.

    Args:
      args: A list of arguments to adb.
      timeout: Timeout in seconds.
      retries: Number of retries.
      check_error: Check that the command doesn't return an error message. This
        does NOT check the exit status of shell commands.

    Returns:
      The output of the command.
    """
    return self._RunAdbCmd(args, timeout=timeout, retries=retries,
                           device_serial=self._device_serial,
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
    output = cls._RunAdbCmd(['devices'], timeout=timeout, retries=retries)
    lines = [line.split() for line in output.splitlines()]
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
    self._RunDeviceAdbCmd(['push', local, remote], timeout, retries)

  def Pull(self, remote, local, timeout=60*5, retries=_DEFAULT_RETRIES):
    """Pulls a file from the device to the host.

    Args:
      remote: Path on the device filesystem.
      local: Path on the host filesystem.
      timeout: (optional) Timeout per try in seconds.
      retries: (optional) Number of retries to attempt.
    """
    self._RunDeviceAdbCmd(['pull', remote, local], timeout, retries)
    _VerifyLocalFileExists(local)

  def Shell(self, command, expect_status=0, timeout=_DEFAULT_TIMEOUT,
            retries=_DEFAULT_RETRIES):
    """Runs a shell command on the device.

    Args:
      command: A string with the shell command to run.
      expect_status: (optional) Check that the command's exit status matches
        this value. Default is 0. If set to None the test is skipped.
      timeout: (optional) Timeout per try in seconds.
      retries: (optional) Number of retries to attempt.

    Returns:
      The output of the shell command as a string.

    Raises:
      device_errors.AdbCommandFailedError: If the exit status doesn't match
        |expect_status|.
    """
    if expect_status is None:
      args = ['shell', command]
    else:
      args = ['shell', '%s; echo %%$?;' % command.rstrip()]
    output = self._RunDeviceAdbCmd(args, timeout, retries, check_error=False)
    if expect_status is not None:
      output_end = output.rfind('%')
      if output_end < 0:
        # causes the status string to become empty and raise a ValueError
        output_end = len(output)

      try:
        status = int(output[output_end+1:])
      except ValueError:
        output = '\n'.join([output.rstrip(),
                            'adb shell error: exit status missing!'])
        raise device_errors.AdbCommandFailedError(
            args, output, device_serial=self._device_serial)
      output = output[:output_end]
      if status != expect_status:
        raise device_errors.AdbCommandFailedError(
            args, output, status, self._device_serial)
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
    return self._RunDeviceAdbCmd(cmd, timeout, retries, check_error=False)

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
    self._RunDeviceAdbCmd(['forward', str(local), str(remote)], timeout,
                          retries)

  def JDWP(self, timeout=_DEFAULT_TIMEOUT, retries=_DEFAULT_RETRIES):
    """List of PIDs of processes hosting a JDWP transport.

    Args:
      timeout: (optional) Timeout per try in seconds.
      retries: (optional) Number of retries to attempt.

    Returns:
      A list of PIDs as strings.
    """
    return [a.strip() for a in
            self._RunDeviceAdbCmd(['jdwp'], timeout, retries).split('\n')]

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
    output = self._RunDeviceAdbCmd(cmd, timeout, retries)
    if 'Success' not in output:
      raise device_errors.AdbCommandFailedError(
          cmd, output, device_serial=self._device_serial)

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
    output = self._RunDeviceAdbCmd(cmd, timeout, retries)
    if 'Failure' in output:
      raise device_errors.AdbCommandFailedError(
          cmd, output, device_serial=self._device_serial)

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
    ret = self._RunDeviceAdbCmd(cmd, timeout, retries)
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
    self._RunDeviceAdbCmd(['restore'] + [path], timeout, retries)

  def WaitForDevice(self, timeout=60*5, retries=_DEFAULT_RETRIES):
    """Block until the device is online.

    Args:
      timeout: (optional) Timeout per try in seconds.
      retries: (optional) Number of retries to attempt.
    """
    self._RunDeviceAdbCmd(['wait-for-device'], timeout, retries)

  def GetState(self, timeout=_DEFAULT_TIMEOUT, retries=_DEFAULT_RETRIES):
    """Get device state.

    Args:
      timeout: (optional) Timeout per try in seconds.
      retries: (optional) Number of retries to attempt.

    Returns:
      One of 'offline', 'bootloader', or 'device'.
    """
    return self._RunDeviceAdbCmd(['get-state'], timeout, retries).strip()

  def GetDevPath(self, timeout=_DEFAULT_TIMEOUT, retries=_DEFAULT_RETRIES):
    """Gets the device path.

    Args:
      timeout: (optional) Timeout per try in seconds.
      retries: (optional) Number of retries to attempt.

    Returns:
      The device path (e.g. usb:3-4)
    """
    return self._RunDeviceAdbCmd(['get-devpath'], timeout, retries)

  def Remount(self, timeout=_DEFAULT_TIMEOUT, retries=_DEFAULT_RETRIES):
    """Remounts the /system partition on the device read-write."""
    self._RunDeviceAdbCmd(['remount'], timeout, retries)

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
    self._RunDeviceAdbCmd(cmd, timeout, retries)

  def Root(self, timeout=_DEFAULT_TIMEOUT, retries=_DEFAULT_RETRIES):
    """Restarts the adbd daemon with root permissions, if possible.

    Args:
      timeout: (optional) Timeout per try in seconds.
      retries: (optional) Number of retries to attempt.
    """
    output = self._RunDeviceAdbCmd(['root'], timeout, retries)
    if 'cannot' in output:
      raise device_errors.AdbCommandFailedError(
          ['root'], output, device_serial=self._device_serial)
