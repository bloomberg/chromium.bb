# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Provides a variety of device interactions based on adb.

Eventually, this will be based on adb_wrapper.
"""
# pylint: disable=W0613

import time

import pylib.android_commands
from pylib.device import adb_wrapper
from pylib.device import decorators
from pylib.device import device_errors
from pylib.utils import parallelizer

_DEFAULT_TIMEOUT = 30
_DEFAULT_RETRIES = 3


@decorators.WithExplicitTimeoutAndRetries(
    _DEFAULT_TIMEOUT, _DEFAULT_RETRIES)
def GetAVDs():
  """ Returns a list of Android Virtual Devices.

  Returns:
    A list containing the configured AVDs.
  """
  return pylib.android_commands.GetAVDs()


@decorators.WithExplicitTimeoutAndRetries(
    _DEFAULT_TIMEOUT, _DEFAULT_RETRIES)
def RestartServer():
  """ Restarts the adb server.

  Raises:
    CommandFailedError if we fail to kill or restart the server.
  """
  pylib.android_commands.AndroidCommands().RestartAdbServer()


class DeviceUtils(object):

  def __init__(self, device, default_timeout=_DEFAULT_TIMEOUT,
               default_retries=_DEFAULT_RETRIES):
    """ DeviceUtils constructor.

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
    """ Checks whether the device is online.

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
    """ Checks whether or not adbd has root privileges.

    Args:
      timeout: Same as for |IsOnline|.
      retries: Same as for |IsOnline|.
    Returns:
      True if adbd has root privileges, False otherwise.
    """
    return self.old_interface.IsRootEnabled()

  @decorators.WithTimeoutAndRetriesFromInstance()
  def EnableRoot(self, timeout=None, retries=None):
    """ Restarts adbd with root privileges.

    Args:
      timeout: Same as for |IsOnline|.
      retries: Same as for |IsOnline|.
    Raises:
      CommandFailedError if root could not be enabled.
    """
    if not self.old_interface.EnableAdbRoot():
      raise device_errors.CommandFailedError(
          'adb root', 'Could not enable root.')

  @decorators.WithTimeoutAndRetriesFromInstance()
  def GetExternalStoragePath(self, timeout=None, retries=None):
    """ Get the device's path to its SD card.

    Args:
      timeout: Same as for |IsOnline|.
      retries: Same as for |IsOnline|.
    Returns:
      The device's path to its SD card.
    """
    try:
      return self.old_interface.GetExternalStorage()
    except AssertionError as e:
      raise device_errors.CommandFailedError(str(e))

  @decorators.WithTimeoutAndRetriesFromInstance()
  def WaitUntilFullyBooted(self, wifi=False, timeout=None, retries=None):
    """ Wait for the device to fully boot.

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
    self.old_interface.WaitForSystemBootCompleted(timeout)
    self.old_interface.WaitForDevicePm()
    self.old_interface.WaitForSdCardReady(timeout)
    if wifi:
      while not 'Wi-Fi is enabled' in (
          self.old_interface.RunShellCommand('dumpsys wifi')):
        time.sleep(0.1)

  def __str__(self):
    """Returns the device serial."""
    return self.old_interface.GetDevice()

  @staticmethod
  def parallel(devices=None, async=False):
    """ Creates a Parallelizer to operate over the provided list of devices.

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

