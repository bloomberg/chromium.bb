# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Provides a variety of device interactions based on adb.

Eventually, this will be based on adb_wrapper.
"""
# pylint: disable=W0613

import time

import pylib.android_commands
from pylib.device import adb_wrapper
from pylib.device import decorators
from pylib.device import device_errors
from pylib.utils import apk_helper
from pylib.utils import parallelizer

_DEFAULT_TIMEOUT = 30
_DEFAULT_RETRIES = 3


@decorators.WithExplicitTimeoutAndRetries(
    _DEFAULT_TIMEOUT, _DEFAULT_RETRIES)
def GetAVDs():
  """Returns a list of Android Virtual Devices.

  Returns:
    A list containing the configured AVDs.
  """
  return pylib.android_commands.GetAVDs()


@decorators.WithExplicitTimeoutAndRetries(
    _DEFAULT_TIMEOUT, _DEFAULT_RETRIES)
def RestartServer():
  """Restarts the adb server.

  Raises:
    CommandFailedError if we fail to kill or restart the server.
  """
  pylib.android_commands.AndroidCommands().RestartAdbServer()


class DeviceUtils(object):

  def __init__(self, device, default_timeout=_DEFAULT_TIMEOUT,
               default_retries=_DEFAULT_RETRIES):
    """DeviceUtils constructor.

    Args:
      device: Either a device serial, an existing AdbWrapper instance, an
              an existing AndroidCommands instance, or nothing.
      default_timeout: An integer containing the default number of seconds to
                       wait for an operation to complete if no explicit value
                       is provided.
      default_retries: An integer containing the default number or times an
                       operation should be retried on failure if no explicit
                       value is provided.
    """
    self.old_interface = None
    if isinstance(device, basestring):
      self.old_interface = pylib.android_commands.AndroidCommands(device)
    elif isinstance(device, adb_wrapper.AdbWrapper):
      self.old_interface = pylib.android_commands.AndroidCommands(str(device))
    elif isinstance(device, pylib.android_commands.AndroidCommands):
      self.old_interface = device
    elif not device:
      self.old_interface = pylib.android_commands.AndroidCommands()
    else:
      raise ValueError('Unsupported type passed for argument "device"')
    self._default_timeout = default_timeout
    self._default_retries = default_retries
    assert(hasattr(self, decorators.DEFAULT_TIMEOUT_ATTR))
    assert(hasattr(self, decorators.DEFAULT_RETRIES_ATTR))

  @decorators.WithTimeoutAndRetriesFromInstance()
  def IsOnline(self, timeout=None, retries=None):
    """Checks whether the device is online.

    Args:
      timeout: An integer containing the number of seconds to wait for the
               operation to complete.
      retries: An integer containing the number of times the operation should
               be retried if it fails.
    Returns:
      True if the device is online, False otherwise.
    """
    return self.old_interface.IsOnline()

  @decorators.WithTimeoutAndRetriesFromInstance()
  def HasRoot(self, timeout=None, retries=None):
    """Checks whether or not adbd has root privileges.

    Args:
      timeout: Same as for |IsOnline|.
      retries: Same as for |IsOnline|.
    Returns:
      True if adbd has root privileges, False otherwise.
    """
    return self._HasRootImpl()

  def _HasRootImpl(self):
    """ Implementation of HasRoot.

    This is split from HasRoot to allow other DeviceUtils methods to call
    HasRoot without spawning a new timeout thread.

    Returns:
      Same as for |HasRoot|.
    """
    return self.old_interface.IsRootEnabled()

  @decorators.WithTimeoutAndRetriesFromInstance()
  def EnableRoot(self, timeout=None, retries=None):
    """Restarts adbd with root privileges.

    Args:
      timeout: Same as for |IsOnline|.
      retries: Same as for |IsOnline|.
    Raises:
      CommandFailedError if root could not be enabled.
    """
    if not self.old_interface.EnableAdbRoot():
      raise device_errors.CommandFailedError(
          ['adb', 'root'], 'Could not enable root.')

  @decorators.WithTimeoutAndRetriesFromInstance()
  def GetExternalStoragePath(self, timeout=None, retries=None):
    """Get the device's path to its SD card.

    Args:
      timeout: Same as for |IsOnline|.
      retries: Same as for |IsOnline|.
    Returns:
      The device's path to its SD card.
    """
    try:
      return self.old_interface.GetExternalStorage()
    except AssertionError as e:
      raise device_errors.CommandFailedError(
          ['adb', 'shell', 'echo', '$EXTERNAL_STORAGE'], str(e))

  @decorators.WithTimeoutAndRetriesFromInstance()
  def WaitUntilFullyBooted(self, wifi=False, timeout=None, retries=None):
    """Wait for the device to fully boot.

    This means waiting for the device to boot, the package manager to be
    available, and the SD card to be ready. It can optionally mean waiting
    for wifi to come up, too.

    Args:
      wifi: A boolean indicating if we should wait for wifi to come up or not.
      timeout: Same as for |IsOnline|.
      retries: Same as for |IsOnline|.
    Raises:
      CommandTimeoutError if one of the component waits times out.
      DeviceUnreachableError if the device becomes unresponsive.
    """
    self._WaitUntilFullyBootedImpl(wifi=wifi, timeout=timeout)

  def _WaitUntilFullyBootedImpl(self, wifi=False, timeout=None):
    """ Implementation of WaitUntilFullyBooted.

    This is split from WaitUntilFullyBooted to allow other DeviceUtils methods
    to call WaitUntilFullyBooted without spawning a new timeout thread.

    TODO(jbudorick) Remove the timeout parameter once this is no longer
    implemented via AndroidCommands.

    Args:
      wifi: Same as for |WaitUntilFullyBooted|.
      timeout: Same as for |IsOnline|.
    Raises:
      Same as for |WaitUntilFullyBooted|.
    """
    if timeout is None:
      timeout = self._default_timeout
    self.old_interface.WaitForSystemBootCompleted(timeout)
    self.old_interface.WaitForDevicePm()
    self.old_interface.WaitForSdCardReady(timeout)
    if wifi:
      while not 'Wi-Fi is enabled' in (
          self._RunShellCommandImpl('dumpsys wifi')):
        time.sleep(0.1)

  @decorators.WithTimeoutAndRetriesDefaults(
      10 * _DEFAULT_TIMEOUT, _DEFAULT_RETRIES)
  def Reboot(self, block=True, timeout=None, retries=None):
    """Reboot the device.

    Args:
      block: A boolean indicating if we should wait for the reboot to complete.
      timeout: Same as for |IsOnline|.
      retries: Same as for |IsOnline|.
    """
    self.old_interface.Reboot()
    if block:
      self._WaitUntilFullyBootedImpl(timeout=timeout)

  @decorators.WithTimeoutAndRetriesDefaults(
      4 * _DEFAULT_TIMEOUT, _DEFAULT_RETRIES)
  def Install(self, apk_path, reinstall=False, timeout=None, retries=None):
    """Install an APK.

    Noop if an identical APK is already installed.

    Args:
      apk_path: A string containing the path to the APK to install.
      reinstall: A boolean indicating if we should keep any existing app data.
      timeout: Same as for |IsOnline|.
      retries: Same as for |IsOnline|.
    Raises:
      CommandFailedError if the installation fails.
      CommandTimeoutError if the installation times out.
    """
    package_name = apk_helper.GetPackageName(apk_path)
    device_path = self.old_interface.GetApplicationPath(package_name)
    if device_path is not None:
      files_changed = self.old_interface.GetFilesChanged(
          apk_path, device_path, ignore_filenames=True)
      if len(files_changed) > 0:
        should_install = True
        if not reinstall:
          out = self.old_interface.Uninstall(package_name)
          for line in out.splitlines():
            if 'Failure' in line:
              raise device_errors.CommandFailedError(
                  ['adb', 'uninstall', package_name], line.strip())
      else:
        should_install = False
    else:
      should_install = True
    if should_install:
      try:
        out = self.old_interface.Install(apk_path, reinstall=reinstall)
        for line in out.splitlines():
          if 'Failure' in line:
            raise device_errors.CommandFailedError(
                ['adb', 'install', apk_path], line.strip())
      except AssertionError as e:
        raise device_errors.CommandFailedError(
            ['adb', 'install', apk_path], str(e))

  @decorators.WithTimeoutAndRetriesFromInstance()
  def RunShellCommand(self, cmd, check_return=False, root=False, timeout=None,
                      retries=None):
    """Run an ADB shell command.

    TODO(jbudorick) Switch the default value of check_return to True after
    AndroidCommands is gone.

    Args:
      cmd: A list containing the command to run on the device and any arguments.
      check_return: A boolean indicating whether or not the return code should
                    be checked.
      timeout: Same as for |IsOnline|.
      retries: Same as for |IsOnline|.
    Raises:
      CommandFailedError if check_return is True and the return code is nozero.
    Returns:
      The output of the command.
    """
    return self._RunShellCommandImpl(cmd, check_return=check_return, root=root,
                                     timeout=timeout)

  def _RunShellCommandImpl(self, cmd, check_return=False, root=False,
                           timeout=None):
    """Implementation of RunShellCommand.

    This is split from RunShellCommand to allow other DeviceUtils methods to
    call RunShellCommand without spawning a new timeout thread.

    TODO(jbudorick) Remove the timeout parameter once this is no longer
    implemented via AndroidCommands.

    Args:
      cmd: Same as for |RunShellCommand|.
      check_return: Same as for |RunShellCommand|.
      timeout: Same as for |IsOnline|.
    Raises:
      Same as for |RunShellCommand|.
    Returns:
      Same as for |RunShellCommand|.
    """
    if isinstance(cmd, list):
      cmd = ' '.join(cmd)
    if root and not self.HasRoot():
      cmd = 'su -c %s' % cmd
    if check_return:
      code, output = self.old_interface.GetShellCommandStatusAndOutput(
          cmd, timeout_time=timeout)
      if int(code) != 0:
        raise device_errors.CommandFailedError(
            cmd, 'Nonzero exit code (%d)' % code)
    else:
      output = self.old_interface.RunShellCommand(cmd, timeout_time=timeout)
    return output

  def __str__(self):
    """Returns the device serial."""
    return self.old_interface.GetDevice()

  @staticmethod
  def parallel(devices=None, async=False):
    """Creates a Parallelizer to operate over the provided list of devices.

    If |devices| is either |None| or an empty list, the Parallelizer will
    operate over all attached devices.

    Args:
      devices: A list of either DeviceUtils instances or objects from
               from which DeviceUtils instances can be constructed. If None,
               all attached devices will be used.
      async: If true, returns a Parallelizer that runs operations
             asynchronously.
    Returns:
      A Parallelizer operating over |devices|.
    """
    if not devices or len(devices) == 0:
      devices = pylib.android_commands.GetAttachedDevices()
    parallelizer_type = (parallelizer.Parallelizer if async
                         else parallelizer.SyncParallelizer)
    return parallelizer_type([
        d if isinstance(d, DeviceUtils) else DeviceUtils(d)
        for d in devices])

