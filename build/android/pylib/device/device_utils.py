# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Provides a variety of device interactions based on adb.

Eventually, this will be based on adb_wrapper.
"""
# pylint: disable=W0613

import logging
import multiprocessing
import os
import re
import sys
import tempfile
import time
import zipfile

import pylib.android_commands
from pylib import cmd_helper
from pylib.device import adb_wrapper
from pylib.device import decorators
from pylib.device import device_errors
from pylib.device.commands import install_commands
from pylib.utils import apk_helper
from pylib.utils import device_temp_file
from pylib.utils import host_utils
from pylib.utils import md5sum
from pylib.utils import parallelizer
from pylib.utils import timeout_retry

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

  _VALID_SHELL_VARIABLE = re.compile('^[a-zA-Z_][a-zA-Z0-9_]*$')

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
    self.adb = None
    self.old_interface = None
    if isinstance(device, basestring):
      self.adb = adb_wrapper.AdbWrapper(device)
      self.old_interface = pylib.android_commands.AndroidCommands(device)
    elif isinstance(device, adb_wrapper.AdbWrapper):
      self.adb = device
      self.old_interface = pylib.android_commands.AndroidCommands(str(device))
    elif isinstance(device, pylib.android_commands.AndroidCommands):
      self.adb = adb_wrapper.AdbWrapper(device.GetDevice())
      self.old_interface = device
    elif not device:
      self.adb = adb_wrapper.AdbWrapper('')
      self.old_interface = pylib.android_commands.AndroidCommands()
    else:
      raise ValueError('Unsupported type passed for argument "device"')
    self._commands_installed = None
    self._default_timeout = default_timeout
    self._default_retries = default_retries
    self._cache = {}
    assert hasattr(self, decorators.DEFAULT_TIMEOUT_ATTR)
    assert hasattr(self, decorators.DEFAULT_RETRIES_ATTR)

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
    try:
      return self.adb.GetState() == 'device'
    except device_errors.BaseError as exc:
      logging.info('Failed to get state: %s', exc)
      return False

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
    try:
      self.RunShellCommand('ls /root', check_return=True)
      return True
    except device_errors.AdbCommandFailedError:
      return False

  def NeedsSU(self, timeout=None, retries=None):
    """Checks whether 'su' is needed to access protected resources.

    Args:
      timeout: timeout in seconds
      retries: number of retries

    Returns:
      True if 'su' is available on the device and is needed to to access
        protected resources; False otherwise if either 'su' is not available
        (e.g. because the device has a user build), or not needed (because adbd
        already has root privileges).

    Raises:
      CommandTimeoutError on timeout.
      DeviceUnreachableError on missing device.
    """
    if 'needs_su' not in self._cache:
      try:
        self.RunShellCommand('su -c ls /root && ! ls /root', check_return=True,
                             timeout=timeout, retries=retries)
        self._cache['needs_su'] = True
      except device_errors.AdbCommandFailedError:
        self._cache['needs_su'] = False
    return self._cache['needs_su']


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
    if 'needs_su' in self._cache:
      del self._cache['needs_su']
    if not self.old_interface.EnableAdbRoot():
      raise device_errors.CommandFailedError(
          'Could not enable root.', str(self))

  @decorators.WithTimeoutAndRetriesFromInstance()
  def IsUserBuild(self, timeout=None, retries=None):
    """Checks whether or not the device is running a user build.

    Args:
      timeout: timeout in seconds
      retries: number of retries

    Returns:
      True if the device is running a user build, False otherwise (i.e. if
        it's running a userdebug build).

    Raises:
      CommandTimeoutError on timeout.
      DeviceUnreachableError on missing device.
    """
    return self.build_type == 'user'

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
    if 'external_storage' in self._cache:
      return self._cache['external_storage']

    value = self.RunShellCommand('echo $EXTERNAL_STORAGE',
                                 single_line=True,
                                 check_return=True)
    if not value:
      raise device_errors.CommandFailedError('$EXTERNAL_STORAGE is not set',
                                             str(self))
    self._cache['external_storage'] = value
    return value

  @decorators.WithTimeoutAndRetriesFromInstance()
  def GetApplicationPath(self, package, timeout=None, retries=None):
    """Get the path of the installed apk on the device for the given package.

    Args:
      package: Name of the package.

    Returns:
      Path to the apk on the device if it exists, None otherwise.
    """
    output = self.RunShellCommand(['pm', 'path', package], single_line=True,
                                  check_return=True)
    if not output:
      return None
    if not output.startswith('package:'):
      raise device_errors.CommandFailedError('pm path returned: %r' % output,
                                             str(self))
    return output[len('package:'):]

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
    def sd_card_ready():
      try:
        self.RunShellCommand(['test', '-d', self.GetExternalStoragePath()],
                             check_return=True)
        return True
      except device_errors.AdbCommandFailedError:
        return False

    def pm_ready():
      try:
        return self.GetApplicationPath('android')
      except device_errors.CommandFailedError:
        return False

    def boot_completed():
      return self.GetProp('sys.boot_completed') == '1'

    def wifi_enabled():
      return 'Wi-Fi is enabled' in self.RunShellCommand(['dumpsys', 'wifi'],
                                                        check_return=False)

    self.adb.WaitForDevice()
    timeout_retry.WaitFor(sd_card_ready)
    timeout_retry.WaitFor(pm_ready)
    timeout_retry.WaitFor(boot_completed)
    if wifi:
      timeout_retry.WaitFor(wifi_enabled)

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
    def device_offline():
      return not self.IsOnline()

    self.adb.Reboot()
    self._cache = {}
    timeout_retry.WaitFor(device_offline, wait_period=1)
    if block:
      self.WaitUntilFullyBooted()

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
              raise device_errors.CommandFailedError(line.strip(), str(self))
      else:
        should_install = False
    else:
      should_install = True
    if should_install:
      try:
        out = self.old_interface.Install(apk_path, reinstall=reinstall)
        for line in out.splitlines():
          if 'Failure' in line:
            raise device_errors.CommandFailedError(line.strip(), str(self))
      except AssertionError as e:
        raise device_errors.CommandFailedError(
            str(e), str(self)), None, sys.exc_info()[2]

  @decorators.WithTimeoutAndRetriesFromInstance()
  def RunShellCommand(self, cmd, check_return=False, cwd=None, env=None,
                      as_root=False, single_line=False,
                      timeout=None, retries=None):
    """Run an ADB shell command.

    The command to run |cmd| should be a sequence of program arguments or else
    a single string.

    When |cmd| is a sequence, it is assumed to contain the name of the command
    to run followed by its arguments. In this case, arguments are passed to the
    command exactly as given, without any further processing by the shell. This
    allows to easily pass arguments containing spaces or special characters
    without having to worry about getting quoting right. Whenever possible, it
    is recomended to pass |cmd| as a sequence.

    When |cmd| is given as a string, it will be interpreted and run by the
    shell on the device.

    This behaviour is consistent with that of command runners in cmd_helper as
    well as Python's own subprocess.Popen.

    TODO(perezju) Change the default of |check_return| to True when callers
      have switched to the new behaviour.

    Args:
      cmd: A string with the full command to run on the device, or a sequence
        containing the command and its arguments.
      check_return: A boolean indicating whether or not the return code should
        be checked.
      cwd: The device directory in which the command should be run.
      env: The environment variables with which the command should be run.
      as_root: A boolean indicating whether the shell command should be run
        with root privileges.
      single_line: A boolean indicating if only a single line of output is
        expected.
      timeout: timeout in seconds
      retries: number of retries

    Returns:
      If single_line is False, the output of the command as a list of lines,
      otherwise, a string with the unique line of output emmited by the command
      (with the optional newline at the end stripped).

    Raises:
      AdbCommandFailedError if check_return is True and the exit code of
        the command run on the device is non-zero.
      CommandFailedError if single_line is True but the output contains two or
        more lines.
      CommandTimeoutError on timeout.
      DeviceUnreachableError on missing device.
    """
    def env_quote(key, value):
      if not DeviceUtils._VALID_SHELL_VARIABLE.match(key):
        raise KeyError('Invalid shell variable name %r' % key)
      # using double quotes here to allow interpolation of shell variables
      return '%s=%s' % (key, cmd_helper.DoubleQuote(value))

    if not isinstance(cmd, basestring):
      cmd = ' '.join(cmd_helper.SingleQuote(s) for s in cmd)
    if env:
      env = ' '.join(env_quote(k, v) for k, v in env.iteritems())
      cmd = '%s %s' % (env, cmd)
    if cwd:
      cmd = 'cd %s && %s' % (cmd_helper.SingleQuote(cwd), cmd)
    if as_root and self.NeedsSU():
      # "su -c sh -c" allows using shell features in |cmd|
      cmd = 'su -c sh -c %s' % cmd_helper.SingleQuote(cmd)
    if timeout is None:
      timeout = self._default_timeout

    try:
      output = self.adb.Shell(cmd)
    except device_errors.AdbCommandFailedError as e:
      if check_return:
        raise
      else:
        output = e.output

    output = output.splitlines()
    if single_line:
      if not output:
        return ''
      elif len(output) == 1:
        return output[0]
      else:
        msg = 'one line of output was expected, but got: %s'
        raise device_errors.CommandFailedError(msg % output, str(self))
    else:
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
    pids = self._GetPidsImpl(process_name)
    if not pids:
      raise device_errors.CommandFailedError(
          'No process "%s"' % process_name, str(self))

    cmd = ['kill', '-%d' % signum] + pids.values()
    self.RunShellCommand(cmd, as_root=as_root, check_return=True)

    if blocking:
      wait_period = 0.1
      while self._GetPidsImpl(process_name):
        time.sleep(wait_period)

    return len(pids)

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
        raise device_errors.CommandFailedError(l, str(self))

  @decorators.WithTimeoutAndRetriesFromInstance()
  def StartInstrumentation(self, component, finish=True, raw=False,
                           extras=None, timeout=None, retries=None):
    if extras is None:
      extras = {}

    cmd = ['am', 'instrument']
    if finish:
      cmd.append('-w')
    if raw:
      cmd.append('-r')
    for k, v in extras.iteritems():
      cmd.extend(['-e', k, v])
    cmd.append(component)
    return self.RunShellCommand(cmd, check_return=True)

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
  def PushChangedFiles(self, host_device_tuples, timeout=None,
                       retries=None):
    """Push files to the device, skipping files that don't need updating.

    Args:
      host_device_tuples: A list of (host_path, device_path) tuples, where
        |host_path| is an absolute path of a file or directory on the host
        that should be minimially pushed to the device, and |device_path| is
        an absolute path of the destination on the device.
      timeout: timeout in seconds
      retries: number of retries

    Raises:
      CommandFailedError on failure.
      CommandTimeoutError on timeout.
      DeviceUnreachableError on missing device.
    """

    files = []
    for h, d in host_device_tuples:
      if os.path.isdir(h):
        self.RunShellCommand(['mkdir', '-p', d], check_return=True)
      files += self._GetChangedFilesImpl(h, d)

    if not files:
      return

    size = sum(host_utils.GetRecursiveDiskUsage(h) for h, _ in files)
    file_count = len(files)
    dir_size = sum(host_utils.GetRecursiveDiskUsage(h)
                   for h, _ in host_device_tuples)
    dir_file_count = 0
    for h, _ in host_device_tuples:
      if os.path.isdir(h):
        dir_file_count += sum(len(f) for _r, _d, f in os.walk(h))
      else:
        dir_file_count += 1

    push_duration = self._ApproximateDuration(
        file_count, file_count, size, False)
    dir_push_duration = self._ApproximateDuration(
        len(host_device_tuples), dir_file_count, dir_size, False)
    zip_duration = self._ApproximateDuration(1, 1, size, True)

    self._InstallCommands()

    if dir_push_duration < push_duration and (
        dir_push_duration < zip_duration or not self._commands_installed):
      self._PushChangedFilesIndividually(host_device_tuples)
    elif push_duration < zip_duration or not self._commands_installed:
      self._PushChangedFilesIndividually(files)
    else:
      self._PushChangedFilesZipped(files)
      self.RunShellCommand(
          ['chmod', '-R', '777'] + [d for _, d in host_device_tuples],
          as_root=True, check_return=True)

  def _GetChangedFilesImpl(self, host_path, device_path):
    real_host_path = os.path.realpath(host_path)
    try:
      real_device_path = self.RunShellCommand(
          ['realpath', device_path], single_line=True, check_return=True)
    except device_errors.CommandFailedError:
      real_device_path = None
    if not real_device_path:
      return [(host_path, device_path)]

    host_hash_tuples = md5sum.CalculateHostMd5Sums([real_host_path])
    device_paths_to_md5 = (
        real_device_path if os.path.isfile(real_host_path)
        else ('%s/%s' % (real_device_path, os.path.relpath(p, real_host_path))
              for _, p in host_hash_tuples))
    device_hash_tuples = md5sum.CalculateDeviceMd5Sums(
        device_paths_to_md5, self)

    if os.path.isfile(host_path):
      if (not device_hash_tuples
          or device_hash_tuples[0].hash != host_hash_tuples[0].hash):
        return [(host_path, device_path)]
      else:
        return []
    else:
      device_tuple_dict = dict((d.path, d.hash) for d in device_hash_tuples)
      to_push = []
      for host_hash, host_abs_path in (
          (h.hash, h.path) for h in host_hash_tuples):
        device_abs_path = '%s/%s' % (
            real_device_path, os.path.relpath(host_abs_path, real_host_path))
        if (device_abs_path not in device_tuple_dict
            or device_tuple_dict[device_abs_path] != host_hash):
          to_push.append((host_abs_path, device_abs_path))
      return to_push

  def _InstallCommands(self):
    if self._commands_installed is None:
      try:
        if not install_commands.Installed(self):
          install_commands.InstallCommands(self)
        self._commands_installed = True
      except Exception as e:
        logging.warning('unzip not available: %s' % str(e))
        self._commands_installed = False

  @staticmethod
  def _ApproximateDuration(adb_calls, file_count, byte_count, is_zipping):
    # We approximate the time to push a set of files to a device as:
    #   t = c1 * a + c2 * f + c3 + b / c4 + b / (c5 * c6), where
    #     t: total time (sec)
    #     c1: adb call time delay (sec)
    #     a: number of times adb is called (unitless)
    #     c2: push time delay (sec)
    #     f: number of files pushed via adb (unitless)
    #     c3: zip time delay (sec)
    #     c4: zip rate (bytes/sec)
    #     b: total number of bytes (bytes)
    #     c5: transfer rate (bytes/sec)
    #     c6: compression ratio (unitless)

    # All of these are approximations.
    ADB_CALL_PENALTY = 0.1 # seconds
    ADB_PUSH_PENALTY = 0.01 # seconds
    ZIP_PENALTY = 2.0 # seconds
    ZIP_RATE = 10000000.0 # bytes / second
    TRANSFER_RATE = 2000000.0 # bytes / second
    COMPRESSION_RATIO = 2.0 # unitless

    adb_call_time = ADB_CALL_PENALTY * adb_calls
    adb_push_setup_time = ADB_PUSH_PENALTY * file_count
    if is_zipping:
      zip_time = ZIP_PENALTY + byte_count / ZIP_RATE
      transfer_time = byte_count / (TRANSFER_RATE * COMPRESSION_RATIO)
    else:
      zip_time = 0
      transfer_time = byte_count / TRANSFER_RATE
    return adb_call_time + adb_push_setup_time + zip_time + transfer_time

  def _PushChangedFilesIndividually(self, files):
    for h, d in files:
      self.adb.Push(h, d)

  def _PushChangedFilesZipped(self, files):
    if not files:
      return

    with tempfile.NamedTemporaryFile(suffix='.zip') as zip_file:
      zip_proc = multiprocessing.Process(
          target=DeviceUtils._CreateDeviceZip,
          args=(zip_file.name, files))
      zip_proc.start()
      zip_proc.join()

      zip_on_device = '%s/tmp.zip' % self.GetExternalStoragePath()
      try:
        self.adb.Push(zip_file.name, zip_on_device)
        self.RunShellCommand(
            ['unzip', zip_on_device],
            as_root=True,
            env={'PATH': '$PATH:%s' % install_commands.BIN_DIR},
            check_return=True)
      finally:
        if zip_proc.is_alive():
          zip_proc.terminate()
        if self.IsOnline():
          self.RunShellCommand(['rm', zip_on_device], check_return=True)

  @staticmethod
  def _CreateDeviceZip(zip_path, host_device_tuples):
    with zipfile.ZipFile(zip_path, 'w') as zip_file:
      for host_path, device_path in host_device_tuples:
        if os.path.isfile(host_path):
          zip_file.write(host_path, device_path, zipfile.ZIP_DEFLATED)
        else:
          for hd, _, files in os.walk(host_path):
            dd = '%s/%s' % (device_path, os.path.relpath(host_path, hd))
            zip_file.write(hd, dd, zipfile.ZIP_STORED)
            for f in files:
              zip_file.write(os.path.join(hd, f), '%s/%s' % (dd, f),
                             zipfile.ZIP_DEFLATED)

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
    try:
      self.RunShellCommand(['test', '-e', device_path], check_return=True)
      return True
    except device_errors.AdbCommandFailedError:
      return False

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
          str(e), str(self)), None, sys.exc_info()[2]

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
    # TODO(jbudorick) Evaluate whether we want to return a list of lines after
    # the implementation switch, and if file not found should raise exception.
    if as_root:
      if not self.old_interface.CanAccessProtectedFileContents():
        raise device_errors.CommandFailedError(
          'Cannot read from %s with root privileges.' % device_path)
      return self.old_interface.GetProtectedFileContents(device_path)
    else:
      return self.old_interface.GetFileContents(device_path)

  @decorators.WithTimeoutAndRetriesFromInstance()
  def WriteFile(self, device_path, contents, as_root=False, force_push=False,
                timeout=None, retries=None):
    """Writes |contents| to a file on the device.

    Args:
      device_path: A string containing the absolute path to the file to write
          on the device.
      contents: A string containing the data to write to the device.
      as_root: A boolean indicating whether the write should be executed with
          root privileges (if available).
      force_push: A boolean indicating whether to force the operation to be
          performed by pushing a file to the device. The default is, when the
          contents are short, to pass the contents using a shell script instead.
      timeout: timeout in seconds
      retries: number of retries

    Raises:
      CommandFailedError if the file could not be written on the device.
      CommandTimeoutError on timeout.
      DeviceUnreachableError on missing device.
    """
    if len(contents) < 512 and not force_push:
      cmd = 'echo -n %s > %s' % (cmd_helper.SingleQuote(contents),
                                 cmd_helper.SingleQuote(device_path))
      self.RunShellCommand(cmd, as_root=as_root, check_return=True)
    else:
      with tempfile.NamedTemporaryFile() as host_temp:
        host_temp.write(contents)
        host_temp.flush()
        if as_root and self.NeedsSU():
          with device_temp_file.DeviceTempFile(self) as device_temp:
            self.adb.Push(host_temp.name, device_temp.name)
            # Here we need 'cp' rather than 'mv' because the temp and
            # destination files might be on different file systems (e.g.
            # on internal storage and an external sd card)
            self.RunShellCommand(['cp', device_temp.name, device_path],
                                 as_root=True, check_return=True)
        else:
          self.adb.Push(host_temp.name, device_path)

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

    Returns:
      True if the device-side property changed and a restart is required as a
      result, False otherwise.

    Raises:
      CommandTimeoutError on timeout.
    """
    return self.old_interface.SetJavaAssertsEnabled(enabled)

  @property
  def build_type(self):
    """Returns the build type of the system (e.g. userdebug)."""
    return self.GetProp('ro.build.type', cache=True)

  def GetProp(self, property_name, cache=False, timeout=None, retries=None):
    """Gets a property from the device.

    Args:
      property_name: A string containing the name of the property to get from
                     the device.
      cache: A boolean indicating whether to cache the value of this property.
      timeout: timeout in seconds
      retries: number of retries

    Returns:
      The value of the device's |property_name| property.

    Raises:
      CommandTimeoutError on timeout.
    """
    assert isinstance(property_name, basestring), (
        "property_name is not a string: %r" % property_name)

    cache_key = '_prop:' + property_name
    if cache and cache_key in self._cache:
      return self._cache[cache_key]
    else:
      # timeout and retries are handled down at run shell, because we don't
      # want to apply them in the other branch when reading from the cache
      value = self.RunShellCommand(['getprop', property_name],
                                   single_line=True, check_return=True,
                                   timeout=timeout, retries=retries)
      if cache or cache_key in self._cache:
        self._cache[cache_key] = value
      return value

  @decorators.WithTimeoutAndRetriesFromInstance()
  def SetProp(self, property_name, value, check=False, timeout=None,
              retries=None):
    """Sets a property on the device.

    Args:
      property_name: A string containing the name of the property to set on
                     the device.
      value: A string containing the value to set to the property on the
             device.
      check: A boolean indicating whether to check that the property was
             successfully set on the device.
      timeout: timeout in seconds
      retries: number of retries

    Raises:
      CommandFailedError if check is true and the property was not correctly
        set on the device (e.g. because it is not rooted).
      CommandTimeoutError on timeout.
    """
    assert isinstance(property_name, basestring), (
        "property_name is not a string: %r" % property_name)
    assert isinstance(value, basestring), "value is not a string: %r" % value

    self.RunShellCommand(['setprop', property_name, value], check_return=True)
    if property_name in self._cache:
      del self._cache[property_name]
    # TODO(perezju) remove the option and make the check mandatory, but using a
    # single shell script to both set- and getprop.
    if check and value != self.GetProp(property_name):
      raise device_errors.CommandFailedError(
          'Unable to set property %r on the device to %r'
          % (property_name, value), str(self))

  @decorators.WithTimeoutAndRetriesFromInstance()
  def GetABI(self, timeout=None, retries=None):
    """Gets the device main ABI.

    Args:
      timeout: timeout in seconds
      retries: number of retries

    Returns:
      The device's main ABI name.

    Raises:
      CommandTimeoutError on timeout.
    """
    return self.GetProp('ro.product.cpu.abi')

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
    return self._GetPidsImpl(process_name)

  def _GetPidsImpl(self, process_name):
    procs_pids = {}
    for line in self.RunShellCommand('ps', check_return=True):
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

  @decorators.WithTimeoutAndRetriesFromInstance()
  def GetMemoryUsageForPid(self, pid, timeout=None, retries=None):
    """Gets the memory usage for the given PID.

    Args:
      pid: PID of the process.
      timeout: timeout in seconds
      retries: number of retries

    Returns:
      A 2-tuple containing:
        - A dict containing the overall memory usage statistics for the PID.
        - A dict containing memory usage statistics broken down by mapping.

    Raises:
      CommandTimeoutError on timeout.
    """
    return self.old_interface.GetMemoryUsageForPid(pid)

  def __str__(self):
    """Returns the device serial."""
    s = self.old_interface.GetDevice()
    if not s:
      s = self.old_interface.Adb().GetSerialNumber()
      if s == 'unknown':
        raise device_errors.NoDevicesError()
    return s

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
