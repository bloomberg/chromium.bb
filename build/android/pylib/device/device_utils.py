# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Provides a variety of device interactions based on adb.

Eventually, this will be based on adb_wrapper.
"""
# pylint: disable=W0613

import sys
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
      timeout: timeout in seconds
      retries: number of retries
    Returns:
      True if the device is online, False otherwise.
    Raises:
      CommandTimeoutError on timeout.
    """
    return self.old_interface.IsOnline()

  @decorators.WithTimeoutAndRetriesFromInstance()
  def HasRoot(self, timeout=None, retries=None):
    """Checks whether or not adbd has root privileges.

    Args:
      timeout: timeout in seconds
      retries: number of retries
    Returns:
      True if adbd has root privileges, False otherwise.
    Raises:
      CommandTimeoutError on timeout.
      DeviceUnreachableError on missing device.
    """
    return self._HasRootImpl()

  def _HasRootImpl(self):
    """Implementation of HasRoot.

    This is split from HasRoot to allow other DeviceUtils methods to call
    HasRoot without spawning a new timeout thread.

    Returns:
      Same as for |HasRoot|.
    Raises:
      Same as for |HasRoot|.
    """
    return self.old_interface.IsRootEnabled()

  @decorators.WithTimeoutAndRetriesFromInstance()
  def EnableRoot(self, timeout=None, retries=None):
    """Restarts adbd with root privileges.

    Args:
      timeout: timeout in seconds
      retries: number of retries
    Raises:
      CommandFailedError if root could not be enabled.
      CommandTimeoutError on timeout.
    """
    if not self.old_interface.EnableAdbRoot():
      raise device_errors.CommandFailedError(
          'Could not enable root.', device=str(self))

  @decorators.WithTimeoutAndRetriesFromInstance()
  def GetExternalStoragePath(self, timeout=None, retries=None):
    """Get the device's path to its SD card.

    Args:
      timeout: timeout in seconds
      retries: number of retries
    Returns:
      The device's path to its SD card.
    Raises:
      CommandFailedError if the external storage path could not be determined.
      CommandTimeoutError on timeout.
      DeviceUnreachableError on missing device.
    """
    try:
      return self.old_interface.GetExternalStorage()
    except AssertionError as e:
      raise device_errors.CommandFailedError(
          str(e), device=str(self)), None, sys.exc_info()[2]

  @decorators.WithTimeoutAndRetriesFromInstance()
  def WaitUntilFullyBooted(self, wifi=False, timeout=None, retries=None):
    """Wait for the device to fully boot.

    This means waiting for the device to boot, the package manager to be
    available, and the SD card to be ready. It can optionally mean waiting
    for wifi to come up, too.

    Args:
      wifi: A boolean indicating if we should wait for wifi to come up or not.
      timeout: timeout in seconds
      retries: number of retries
    Raises:
      CommandFailedError on failure.
      CommandTimeoutError if one of the component waits times out.
      DeviceUnreachableError if the device becomes unresponsive.
    """
    self._WaitUntilFullyBootedImpl(wifi=wifi, timeout=timeout)

  def _WaitUntilFullyBootedImpl(self, wifi=False, timeout=None):
    """Implementation of WaitUntilFullyBooted.

    This is split from WaitUntilFullyBooted to allow other DeviceUtils methods
    to call WaitUntilFullyBooted without spawning a new timeout thread.

    TODO(jbudorick) Remove the timeout parameter once this is no longer
    implemented via AndroidCommands.

    Args:
      wifi: Same as for |WaitUntilFullyBooted|.
      timeout: timeout in seconds
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
        time.sleep(1)

  REBOOT_DEFAULT_TIMEOUT = 10 * _DEFAULT_TIMEOUT
  REBOOT_DEFAULT_RETRIES = _DEFAULT_RETRIES

  @decorators.WithTimeoutAndRetriesDefaults(
      REBOOT_DEFAULT_TIMEOUT,
      REBOOT_DEFAULT_RETRIES)
  def Reboot(self, block=True, timeout=None, retries=None):
    """Reboot the device.

    Args:
      block: A boolean indicating if we should wait for the reboot to complete.
      timeout: timeout in seconds
      retries: number of retries
    Raises:
      CommandTimeoutError on timeout.
      DeviceUnreachableError on missing device.
    """
    self.old_interface.Reboot()
    if block:
      self._WaitUntilFullyBootedImpl(timeout=timeout)

  INSTALL_DEFAULT_TIMEOUT = 4 * _DEFAULT_TIMEOUT
  INSTALL_DEFAULT_RETRIES = _DEFAULT_RETRIES

  @decorators.WithTimeoutAndRetriesDefaults(
      INSTALL_DEFAULT_TIMEOUT,
      INSTALL_DEFAULT_RETRIES)
  def Install(self, apk_path, reinstall=False, timeout=None, retries=None):
    """Install an APK.

    Noop if an identical APK is already installed.

    Args:
      apk_path: A string containing the path to the APK to install.
      reinstall: A boolean indicating if we should keep any existing app data.
      timeout: timeout in seconds
      retries: number of retries
    Raises:
      CommandFailedError if the installation fails.
      CommandTimeoutError if the installation times out.
      DeviceUnreachableError on missing device.
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
                  line.strip(), device=str(self))
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
                line.strip(), device=str(self))
      except AssertionError as e:
        raise device_errors.CommandFailedError(
            str(e), device=str(self)), None, sys.exc_info()[2]

  @decorators.WithTimeoutAndRetriesFromInstance()
  def RunShellCommand(self, cmd, check_return=False, as_root=False,
                      timeout=None, retries=None):
    """Run an ADB shell command.

    TODO(jbudorick) Switch the default value of check_return to True after
    AndroidCommands is gone.

    Args:
      cmd: A list containing the command to run on the device and any arguments.
      check_return: A boolean indicating whether or not the return code should
                    be checked.
      as_root: A boolean indicating whether the shell command should be run
               with root privileges.
      timeout: timeout in seconds
      retries: number of retries
    Returns:
      The output of the command.
    Raises:
      CommandFailedError if check_return is True and the return code is nozero.
      CommandTimeoutError on timeout.
      DeviceUnreachableError on missing device.
    """
    return self._RunShellCommandImpl(cmd, check_return=check_return,
                                     as_root=as_root, timeout=timeout)

  def _RunShellCommandImpl(self, cmd, check_return=False, as_root=False,
                           timeout=None):
    """Implementation of RunShellCommand.

    This is split from RunShellCommand to allow other DeviceUtils methods to
    call RunShellCommand without spawning a new timeout thread.

    TODO(jbudorick) Remove the timeout parameter once this is no longer
    implemented via AndroidCommands.

    Args:
      cmd: Same as for |RunShellCommand|.
      check_return: Same as for |RunShellCommand|.
      as_root: Same as for |RunShellCommand|.
      timeout: timeout in seconds
    Raises:
      Same as for |RunShellCommand|.
    Returns:
      Same as for |RunShellCommand|.
    """
    if isinstance(cmd, list):
      cmd = ' '.join(cmd)
    if as_root and not self.HasRoot():
      cmd = 'su -c %s' % cmd
    if check_return:
      code, output = self.old_interface.GetShellCommandStatusAndOutput(
          cmd, timeout_time=timeout)
      if int(code) != 0:
        raise device_errors.AdbCommandFailedError(
            cmd.split(), 'Nonzero exit code (%d)' % code, device=str(self))
    else:
      output = self.old_interface.RunShellCommand(cmd, timeout_time=timeout)
    return output

  @decorators.WithTimeoutAndRetriesFromInstance()
  def KillAll(self, process_name, signum=9, as_root=False, blocking=False,
              timeout=None, retries=None):
    """Kill all processes with the given name on the device.

    Args:
      process_name: A string containing the name of the process to kill.
      signum: An integer containing the signal number to send to kill. Defaults
              to 9 (SIGKILL).
      as_root: A boolean indicating whether the kill should be executed with
               root privileges.
      blocking: A boolean indicating whether we should wait until all processes
                with the given |process_name| are dead.
      timeout: timeout in seconds
      retries: number of retries
    Raises:
      CommandFailedError if no process was killed.
      CommandTimeoutError on timeout.
      DeviceUnreachableError on missing device.
    """
    pids = self.old_interface.ExtractPid(process_name)
    if len(pids) == 0:
      raise device_errors.CommandFailedError(
          'No process "%s"' % process_name, device=str(self))

    if blocking:
      total_killed = self.old_interface.KillAllBlocking(
          process_name, signum=signum, with_su=as_root, timeout_sec=timeout)
    else:
      total_killed = self.old_interface.KillAll(
          process_name, signum=signum, with_su=as_root)
    if total_killed == 0:
      raise device_errors.CommandFailedError(
          'Failed to kill "%s"' % process_name, device=str(self))

  @decorators.WithTimeoutAndRetriesFromInstance()
  def StartActivity(self, intent, blocking=False, trace_file_name=None,
                    force_stop=False, timeout=None, retries=None):
    """Start package's activity on the device.

    Args:
      intent: An Intent to send.
      blocking: A boolean indicating whether we should wait for the activity to
                finish launching.
      trace_file_name: If present, a string that both indicates that we want to
                       profile the activity and contains the path to which the
                       trace should be saved.
      force_stop: A boolean indicating whether we should stop the activity
                  before starting it.
      timeout: timeout in seconds
      retries: number of retries
    Raises:
      CommandFailedError if the activity could not be started.
      CommandTimeoutError on timeout.
      DeviceUnreachableError on missing device.
    """
    single_category = (intent.category[0] if isinstance(intent.category, list)
                                          else intent.category)
    output = self.old_interface.StartActivity(
        intent.package, intent.activity, wait_for_completion=blocking,
        action=intent.action, category=single_category, data=intent.data,
        extras=intent.extras, trace_file_name=trace_file_name,
        force_stop=force_stop, flags=intent.flags)
    for l in output:
      if l.startswith('Error:'):
        raise device_errors.CommandFailedError(l, device=str(self))

  @decorators.WithTimeoutAndRetriesFromInstance()
  def BroadcastIntent(self, intent, timeout=None, retries=None):
    """Send a broadcast intent.

    Args:
      intent: An Intent to broadcast.
      timeout: timeout in seconds
      retries: number of retries
    Raises:
      CommandTimeoutError on timeout.
      DeviceUnreachableError on missing device.
    """
    package, old_intent = intent.action.rsplit('.', 1)
    if intent.extras is None:
      args = []
    else:
      args = ['-e %s%s' % (k, ' "%s"' % v if v else '')
              for k, v in intent.extras.items() if len(k) > 0]
    self.old_interface.BroadcastIntent(package, old_intent, *args)

  @decorators.WithTimeoutAndRetriesFromInstance()
  def GoHome(self, timeout=None, retries=None):
    """Return to the home screen.

    Args:
      timeout: timeout in seconds
      retries: number of retries
    Raises:
      CommandTimeoutError on timeout.
      DeviceUnreachableError on missing device.
    """
    self.old_interface.GoHome()

  @decorators.WithTimeoutAndRetriesFromInstance()
  def ForceStop(self, package, timeout=None, retries=None):
    """Close the application.

    Args:
      package: A string containing the name of the package to stop.
      timeout: timeout in seconds
      retries: number of retries
    Raises:
      CommandTimeoutError on timeout.
      DeviceUnreachableError on missing device.
    """
    self.old_interface.CloseApplication(package)

  @decorators.WithTimeoutAndRetriesFromInstance()
  def ClearApplicationState(self, package, timeout=None, retries=None):
    """Clear all state for the given package.

    Args:
      package: A string containing the name of the package to stop.
      timeout: timeout in seconds
      retries: number of retries
    Raises:
      CommandTimeoutError on timeout.
      DeviceUnreachableError on missing device.
    """
    self.old_interface.ClearApplicationState(package)

  @decorators.WithTimeoutAndRetriesFromInstance()
  def SendKeyEvent(self, keycode, timeout=None, retries=None):
    """Sends a keycode to the device.

    See: http://developer.android.com/reference/android/view/KeyEvent.html

    Args:
      keycode: A integer keycode to send to the device.
      timeout: timeout in seconds
      retries: number of retries
    Raises:
      CommandTimeoutError on timeout.
      DeviceUnreachableError on missing device.
    """
    self.old_interface.SendKeyEvent(keycode)

  PUSH_CHANGED_FILES_DEFAULT_TIMEOUT = 10 * _DEFAULT_TIMEOUT
  PUSH_CHANGED_FILES_DEFAULT_RETRIES = _DEFAULT_RETRIES

  @decorators.WithTimeoutAndRetriesDefaults(
      PUSH_CHANGED_FILES_DEFAULT_TIMEOUT,
      PUSH_CHANGED_FILES_DEFAULT_RETRIES)
  def PushChangedFiles(self, host_path, device_path, timeout=None,
                       retries=None):
    """Push files to the device, skipping files that don't need updating.

    Args:
      host_path: A string containing the absolute path to the file or directory
                 on the host that should be minimally pushed to the device.
      device_path: A string containing the absolute path of the destination on
                   the device.
      timeout: timeout in seconds
      retries: number of retries
    Raises:
      CommandFailedError on failure.
      CommandTimeoutError on timeout.
      DeviceUnreachableError on missing device.
    """
    self.old_interface.PushIfNeeded(host_path, device_path)

  @decorators.WithTimeoutAndRetriesFromInstance()
  def FileExists(self, device_path, timeout=None, retries=None):
    """Checks whether the given file exists on the device.

    Args:
      device_path: A string containing the absolute path to the file on the
                   device.
      timeout: timeout in seconds
      retries: number of retries
    Returns:
      True if the file exists on the device, False otherwise.
    Raises:
      CommandTimeoutError on timeout.
      DeviceUnreachableError on missing device.
    """
    return self._FileExistsImpl(device_path)

  def _FileExistsImpl(self, device_path):
    """Implementation of FileExists.

    This is split from FileExists to allow other DeviceUtils methods to call
    FileExists without spawning a new timeout thread.

    Args:
      device_path: Same as for |FileExists|.
    Returns:
      True if the file exists on the device, False otherwise.
    Raises:
      Same as for |FileExists|.
    """
    return self.old_interface.FileExistsOnDevice(device_path)

  @decorators.WithTimeoutAndRetriesFromInstance()
  def PullFile(self, device_path, host_path, timeout=None, retries=None):
    """Pull a file from the device.

    Args:
      device_path: A string containing the absolute path of the file to pull
                   from the device.
      host_path: A string containing the absolute path of the destination on
                 the host.
      timeout: timeout in seconds
      retries: number of retries
    Raises:
      CommandFailedError on failure.
      CommandTimeoutError on timeout.
    """
    try:
      self.old_interface.PullFileFromDevice(device_path, host_path)
    except AssertionError as e:
      raise device_errors.CommandFailedError(
          str(e), device=str(self)), None, sys.exc_info()[2]

  @decorators.WithTimeoutAndRetriesFromInstance()
  def ReadFile(self, device_path, as_root=False, timeout=None, retries=None):
    """Reads the contents of a file from the device.

    Args:
      device_path: A string containing the absolute path of the file to read
                   from the device.
      as_root: A boolean indicating whether the read should be executed with
               root privileges.
      timeout: timeout in seconds
      retries: number of retries
    Returns:
      The contents of the file at |device_path| as a list of lines.
    Raises:
      CommandFailedError if the file can't be read.
      CommandTimeoutError on timeout.
      DeviceUnreachableError on missing device.
    """
    # TODO(jbudorick) Evaluate whether we actually want to return a list of
    # lines after the implementation switch.
    if as_root:
      if not self.old_interface.CanAccessProtectedFileContents():
        raise device_errors.CommandFailedError(
          'Cannot read from %s with root privileges.' % device_path)
      return self.old_interface.GetProtectedFileContents(device_path)
    else:
      return self.old_interface.GetFileContents(device_path)

  @decorators.WithTimeoutAndRetriesFromInstance()
  def WriteFile(self, device_path, contents, as_root=False, timeout=None,
                retries=None):
    """Writes |contents| to a file on the device.

    Args:
      device_path: A string containing the absolute path to the file to write
                   on the device.
      contents: A string containing the data to write to the device.
      as_root: A boolean indicating whether the write should be executed with
               root privileges.
      timeout: timeout in seconds
      retries: number of retries
    Raises:
      CommandFailedError if the file could not be written on the device.
      CommandTimeoutError on timeout.
      DeviceUnreachableError on missing device.
    """
    if as_root:
      if not self.old_interface.CanAccessProtectedFileContents():
        raise device_errors.CommandFailedError(
            'Cannot write to %s with root privileges.' % device_path)
      self.old_interface.SetProtectedFileContents(device_path, contents)
    else:
      self.old_interface.SetFileContents(device_path, contents)

  @decorators.WithTimeoutAndRetriesFromInstance()
  def Ls(self, device_path, timeout=None, retries=None):
    """Lists the contents of a directory on the device.

    Args:
      device_path: A string containing the path of the directory on the device
                   to list.
      timeout: timeout in seconds
      retries: number of retries
    Returns:
      The contents of the directory specified by |device_path|.
    Raises:
      CommandTimeoutError on timeout.
      DeviceUnreachableError on missing device.
    """
    return self.old_interface.ListPathContents(device_path)

  @decorators.WithTimeoutAndRetriesFromInstance()
  def SetJavaAsserts(self, enabled, timeout=None, retries=None):
    """Enables or disables Java asserts.

    Args:
      enabled: A boolean indicating whether Java asserts should be enabled
               or disabled.
      timeout: timeout in seconds
      retries: number of retries
    Raises:
      CommandTimeoutError on timeout.
    """
    self.old_interface.SetJavaAssertsEnabled(enabled)

  @decorators.WithTimeoutAndRetriesFromInstance()
  def GetProp(self, property_name, timeout=None, retries=None):
    """Gets a property from the device.

    Args:
      property_name: A string containing the name of the property to get from
                     the device.
      timeout: timeout in seconds
      retries: number of retries
    Returns:
      The value of the device's |property_name| property.
    Raises:
      CommandTimeoutError on timeout.
    """
    return self.old_interface.system_properties[property_name]

  @decorators.WithTimeoutAndRetriesFromInstance()
  def SetProp(self, property_name, value, timeout=None, retries=None):
    """Sets a property on the device.

    Args:
      property_name: A string containing the name of the property to set on
                     the device.
      value: A string containing the value to set to the property on the
             device.
      timeout: timeout in seconds
      retries: number of retries
    Raises:
      CommandTimeoutError on timeout.
    """
    self.old_interface.system_properties[property_name] = value

  @decorators.WithTimeoutAndRetriesFromInstance()
  def GetPids(self, process_name, timeout=None, retries=None):
    """Returns the PIDs of processes with the given name.

    Note that the |process_name| is often the package name.

    Args:
      process_name: A string containing the process name to get the PIDs for.
      timeout: timeout in seconds
      retries: number of retries
    Returns:
      A dict mapping process name to PID for each process that contained the
      provided |process_name|.
    Raises:
      CommandTimeoutError on timeout.
      DeviceUnreachableError on missing device.
    """
    procs_pids = {}
    for line in self._RunShellCommandImpl('ps'):
      try:
        ps_data = line.split()
        if process_name in ps_data[-1]:
          procs_pids[ps_data[-1]] = ps_data[1]
      except IndexError:
        pass
    return procs_pids

  @decorators.WithTimeoutAndRetriesFromInstance()
  def TakeScreenshot(self, host_path=None, timeout=None, retries=None):
    """Takes a screenshot of the device.

    Args:
      host_path: A string containing the path on the host to save the
                 screenshot to. If None, a file name will be generated.
      timeout: timeout in seconds
      retries: number of retries
    Returns:
      The name of the file on the host to which the screenshot was saved.
    Raises:
      CommandFailedError on failure.
      CommandTimeoutError on timeout.
      DeviceUnreachableError on missing device.
    """
    return self.old_interface.TakeScreenshot(host_path)

  @decorators.WithTimeoutAndRetriesFromInstance()
  def GetIOStats(self, timeout=None, retries=None):
    """Gets cumulative disk IO stats since boot for all processes.

    Args:
      timeout: timeout in seconds
      retries: number of retries
    Returns:
      A dict containing |num_reads|, |num_writes|, |read_ms|, and |write_ms|.
    Raises:
      CommandTimeoutError on timeout.
      DeviceUnreachableError on missing device.
    """
    return self.old_interface.GetIoStats()

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

