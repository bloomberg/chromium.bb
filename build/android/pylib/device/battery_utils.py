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

  def GetBatteryInfo(self, timeout=None, retries=None):
    """Gets battery info for the device.

    Args:
      timeout: timeout in seconds
      retries: number of retries
    Returns:
      A dict containing various battery information as reported by dumpsys
      battery.
    """
    result = {}
    # Skip the first line, which is just a header.
    for line in self._device.RunShellCommand(
        ['dumpsys', 'battery'], check_return=True)[1:]:
      # If usb charging has been disabled, an extra line of header exists.
      if 'UPDATES STOPPED' in line:
        logging.warning('Dumpsys battery not receiving updates. '
                        'Run dumpsys battery reset if this is in error.')
      elif ':' not in line:
        logging.warning('Unknown line found in dumpsys battery: "%s"', line)
      else:
        k, v = line.split(':', 1)
        result[k.strip()] = v.strip()
    return result

  def GetCharging(self, timeout=None, retries=None):
    """Gets the charging state of the device.

    Args:
      timeout: timeout in seconds
      retries: number of retries
    Returns:
      True if the device is charging, false otherwise.
    """
    battery_info = self.GetBatteryInfo()
    for k in ('AC powered', 'USB powered', 'Wireless powered'):
      if (k in battery_info and
          battery_info[k].lower() in ('true', '1', 'yes')):
        return True
    return False

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
    if 'charging_config' not in self._cache:
      for c in _CONTROL_CHARGING_COMMANDS:
        if self._device.FileExists(c['witness_file']):
          self._cache['charging_config'] = c
          break
      else:
        raise device_errors.CommandFailedError(
            'Unable to find charging commands.')

    if enabled:
      command = self._cache['charging_config']['enable_command']
    else:
      command = self._cache['charging_config']['disable_command']

    def set_and_verify_charging():
      self._device.RunShellCommand(command, check_return=True)
      return self.GetCharging() == enabled

    timeout_retry.WaitFor(set_and_verify_charging, wait_period=1)

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
    def battery_updates_disabled():
      return self.GetCharging() is False

    self._device.RunShellCommand(
        ['dumpsys', 'battery', 'set', 'usb', '1'], check_return=True)
    self._device.RunShellCommand(
        ['dumpsys', 'batterystats', '--reset'], check_return=True)
    battery_data = self._device.RunShellCommand(
        ['dumpsys', 'batterystats', '--charged', '--checkin'],
        check_return=True)
    ROW_TYPE_INDEX = 3
    PWI_POWER_INDEX = 5
    for line in battery_data:
      l = line.split(',')
      if (len(l) > PWI_POWER_INDEX and l[ROW_TYPE_INDEX] == 'pwi'
          and l[PWI_POWER_INDEX] != 0):
        raise device_errors.CommandFailedError(
            'Non-zero pmi value found after reset.')
    self._device.RunShellCommand(['dumpsys', 'battery', 'set', 'usb', '0'],
                                 check_return=True)
    timeout_retry.WaitFor(battery_updates_disabled, wait_period=1)

  # TODO(rnephew): Make private when all use cases can use the context manager.
  def EnableBatteryUpdates(self, timeout=None, retries=None):
    """ Restarts device charging so that dumpsys no longer collects power data.

    Args:
      timeout: timeout in seconds
      retries: number of retries
    """
    def battery_updates_enabled():
      return self.GetCharging() is True

    self._device.RunShellCommand(['dumpsys', 'battery', 'set', 'usb', '1'],
                                 check_return=True)
    self._device.RunShellCommand(['dumpsys', 'battery', 'reset'],
                                 check_return=True)
    timeout_retry.WaitFor(battery_updates_enabled, wait_period=1)

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

  def ChargeDeviceToLevel(self, level, wait_period=60):
    """Enables charging and waits for device to be charged to given level.

    Args:
      level: level of charge to wait for.
      wait_period: time in seconds to wait between checking.
    """
    self.SetCharging(True)

    def device_charged():
      battery_level = self.GetBatteryInfo().get('level')
      if battery_level is None:
        logging.warning('Unable to find current battery level.')
        battery_level = 100
      else:
        logging.info('current battery level: %s', battery_level)
        battery_level = int(battery_level)
      return battery_level >= level

    timeout_retry.WaitFor(device_charged, wait_period=wait_period)
