# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Provides a variety of device interactions with power.
"""
# pylint: disable=unused-argument

import collections
import contextlib
import csv
import logging

from pylib import constants
from pylib.device import decorators
from pylib.device import device_errors
from pylib.device import device_utils
from pylib.utils import timeout_retry

_DEFAULT_TIMEOUT = 30
_DEFAULT_RETRIES = 3

_CONTROL_CHARGING_COMMANDS = [
  {
    # Nexus 4
    'witness_file': '/sys/module/pm8921_charger/parameters/disabled',
    'enable_command': 'echo 0 > /sys/module/pm8921_charger/parameters/disabled',
    'disable_command':
        'echo 1 > /sys/module/pm8921_charger/parameters/disabled',
  },
  {
    # Nexus 5
    # Setting the HIZ bit of the bq24192 causes the charger to actually ignore
    # energy coming from USB. Setting the power_supply offline just updates the
    # Android system to reflect that.
    'witness_file': '/sys/kernel/debug/bq24192/INPUT_SRC_CONT',
    'enable_command': (
        'echo 0x4A > /sys/kernel/debug/bq24192/INPUT_SRC_CONT && '
        'echo 1 > /sys/class/power_supply/usb/online'),
    'disable_command': (
        'echo 0xCA > /sys/kernel/debug/bq24192/INPUT_SRC_CONT && '
        'chmod 644 /sys/class/power_supply/usb/online && '
        'echo 0 > /sys/class/power_supply/usb/online'),
  },
]

# The list of useful dumpsys columns.
# Index of the column containing the format version.
_DUMP_VERSION_INDEX = 0
# Index of the column containing the type of the row.
_ROW_TYPE_INDEX = 3
# Index of the column containing the uid.
_PACKAGE_UID_INDEX = 4
# Index of the column containing the application package.
_PACKAGE_NAME_INDEX = 5
# The column containing the uid of the power data.
_PWI_UID_INDEX = 1
# The column containing the type of consumption. Only consumtion since last
# charge are of interest here.
_PWI_AGGREGATION_INDEX = 2
# The column containing the amount of power used, in mah.
_PWI_POWER_CONSUMPTION_INDEX = 5


class BatteryUtils(object):

  def __init__(self, device, default_timeout=_DEFAULT_TIMEOUT,
               default_retries=_DEFAULT_RETRIES):
    """BatteryUtils constructor.

      Args:
        device: A DeviceUtils instance.
        default_timeout: An integer containing the default number of seconds to
                         wait for an operation to complete if no explicit value
                         is provided.
        default_retries: An integer containing the default number or times an
                         operation should be retried on failure if no explicit
                         value is provided.

      Raises:
        TypeError: If it is not passed a DeviceUtils instance.
    """
    if not isinstance(device, device_utils.DeviceUtils):
      raise TypeError('Must be initialized with DeviceUtils object.')
    self._device = device
    self._cache = device.GetClientCache(self.__class__.__name__)
    self._default_timeout = default_timeout
    self._default_retries = default_retries

  @decorators.WithTimeoutAndRetriesFromInstance()
  def GetPowerData(self, timeout=None, retries=None):
    """ Get power data for device.
    Args:
      timeout: timeout in seconds
      retries: number of retries

    Returns:
      Dict of power data, keyed on package names.
      {
        package_name: {
          'uid': uid,
          'data': [1,2,3]
        },
      }
    """
    dumpsys_output = self._device.RunShellCommand(
        ['dumpsys', 'batterystats', '-c'], check_return=True)
    csvreader = csv.reader(dumpsys_output)
    uid_entries = {}
    pwi_entries = collections.defaultdict(list)
    for entry in csvreader:
      if entry[_DUMP_VERSION_INDEX] not in ['8', '9']:
        # Wrong dumpsys version.
        raise device_errors.DeviceVersionError(
            'Dumpsys version must be 8 or 9. %s found.'
            % entry[_DUMP_VERSION_INDEX])
      if _ROW_TYPE_INDEX >= len(entry):
        continue
      if entry[_ROW_TYPE_INDEX] == 'uid':
        current_package = entry[_PACKAGE_NAME_INDEX]
        if current_package in uid_entries:
          raise device_errors.CommandFailedError(
              'Package %s found multiple times' % (current_package))
        uid_entries[current_package] = entry[_PACKAGE_UID_INDEX]
      elif (_PWI_POWER_CONSUMPTION_INDEX < len(entry)
          and entry[_ROW_TYPE_INDEX] == 'pwi'
          and entry[_PWI_AGGREGATION_INDEX] == 'l'):
        pwi_entries[entry[_PWI_UID_INDEX]].append(
            float(entry[_PWI_POWER_CONSUMPTION_INDEX]))

    return {p: {'uid': uid, 'data': pwi_entries[uid]}
            for p, uid in uid_entries.iteritems()}

  def GetPackagePowerData(self, package, timeout=None, retries=None):
    """ Get power data for particular package.

    Args:
      package: Package to get power data on.

    returns:
      Dict of UID and power data.
      {
        'uid': uid,
        'data': [1,2,3]
      }
      None if the package is not found in the power data.
    """
    return self.GetPowerData().get(package)

  # TODO(rnephew): Move implementation from device_utils when this is used.
  def GetBatteryInfo(self, timeout=None, retries=None):
    """Gets battery info for the device.

    Args:
      timeout: timeout in seconds
      retries: number of retries
    Returns:
      A dict containing various battery information as reported by dumpsys
      battery.
    """
    return self._device.GetBatteryInfo(timeout=None, retries=None)

  # TODO(rnephew): Move implementation from device_utils when this is used.
  def GetCharging(self, timeout=None, retries=None):
    """Gets the charging state of the device.

    Args:
      timeout: timeout in seconds
      retries: number of retries
    Returns:
      True if the device is charging, false otherwise.
    """
    return self._device.GetCharging(timeout=None, retries=None)

  # TODO(rnephew): Move implementation from device_utils when this is used.
  def SetCharging(self, enabled, timeout=None, retries=None):
    """Enables or disables charging on the device.

    Args:
      enabled: A boolean indicating whether charging should be enabled or
        disabled.
      timeout: timeout in seconds
      retries: number of retries

    Raises:
      device_errors.CommandFailedError: If method of disabling charging cannot
        be determined.
    """
    self._device.SetCharging(enabled, timeout=None, retries=None)

  # TODO(rnephew): Move implementation from device_utils when this is used.
  # TODO(rnephew): Make private when all use cases can use the context manager.
  def DisableBatteryUpdates(self, timeout=None, retries=None):
    """ Resets battery data and makes device appear like it is not
    charging so that it will collect power data since last charge.

    Args:
      timeout: timeout in seconds
      retries: number of retries

    Raises:
      device_errors.CommandFailedError: When resetting batterystats fails to
        reset power values.
    """
    self._device.DisableBatteryUpdates(timeout=None, retries=None)

  # TODO(rnephew): Move implementation from device_utils when this is used.
  # TODO(rnephew): Make private when all use cases can use the context manager.
  def EnableBatteryUpdates(self, timeout=None, retries=None):
    """ Restarts device charging so that dumpsys no longer collects power data.

    Args:
      timeout: timeout in seconds
      retries: number of retries
    """
    self._device.EnableBatteryUpdates(timeout=None, retries=None)

  @contextlib.contextmanager
  def BatteryMeasurement(self, timeout=None, retries=None):
    """Context manager that enables battery data collection. It makes
    the device appear to stop charging so that dumpsys will start collecting
    power data since last charge. Once the with block is exited, charging is
    resumed and power data since last charge is no longer collected.

    Only for devices L and higher.

    Example usage:
      with BatteryMeasurement():
        browser_actions()
        get_power_data() # report usage within this block
      after_measurements() # Anything that runs after power
                           # measurements are collected

    Args:
      timeout: timeout in seconds
      retries: number of retries

    Raises:
      device_errors.CommandFailedError: If device is not L or higher.
    """
    if (self._device.build_version_sdk <
        constants.ANDROID_SDK_VERSION_CODES.LOLLIPOP):
      raise device_errors.DeviceVersionError('Device must be L or higher.')
    try:
      self.DisableBatteryUpdates(timeout=timeout, retries=retries)
      yield
    finally:
      self.EnableBatteryUpdates(timeout=timeout, retries=retries)
