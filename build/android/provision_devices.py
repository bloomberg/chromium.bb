#!/usr/bin/env python
#
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Provisions Android devices with settings required for bots.

Usage:
  ./provision_devices.py [-d <device serial number>]
"""

import argparse
import logging
import os
import posixpath
import re
import subprocess
import sys
import time

from pylib import android_commands
from pylib import constants
from pylib import device_settings
from pylib.device import device_blacklist
from pylib.device import device_errors
from pylib.device import device_utils
from pylib.utils import run_tests_helper
from pylib.utils import timeout_retry

sys.path.append(os.path.join(constants.DIR_SOURCE_ROOT,
                             'third_party', 'android_testrunner'))
import errors


class _DEFAULT_TIMEOUTS(object):
  # L can take a while to reboot after a wipe.
  LOLLIPOP = 600
  PRE_LOLLIPOP = 180

  HELP_TEXT = '{}s on L, {}s on pre-L'.format(LOLLIPOP, PRE_LOLLIPOP)


def KillHostHeartbeat():
  ps = subprocess.Popen(['ps', 'aux'], stdout=subprocess.PIPE)
  stdout, _ = ps.communicate()
  matches = re.findall('\\n.*host_heartbeat.*', stdout)
  for match in matches:
    logging.info('An instance of host heart beart running... will kill')
    pid = re.findall(r'(\S+)', match)[1]
    subprocess.call(['kill', str(pid)])


def LaunchHostHeartbeat():
  # Kill if existing host_heartbeat
  KillHostHeartbeat()
  # Launch a new host_heartbeat
  logging.info('Spawning host heartbeat...')
  subprocess.Popen([os.path.join(constants.DIR_SOURCE_ROOT,
                                 'build/android/host_heartbeat.py')])


def PushAndLaunchAdbReboot(device, target):
  """Pushes and launches the adb_reboot binary on the device.

  Arguments:
    device: The DeviceUtils instance for the device to which the adb_reboot
            binary should be pushed.
    target: The build target (example, Debug or Release) which helps in
            locating the adb_reboot binary.
  """
  logging.info('Will push and launch adb_reboot on %s' % str(device))
  # Kill if adb_reboot is already running.
  try:
    # Don't try to kill adb_reboot more than once. We don't expect it to be
    # running at all.
    device.KillAll('adb_reboot', blocking=True, timeout=2, retries=0)
  except device_errors.CommandFailedError:
    # We can safely ignore the exception because we don't expect adb_reboot
    # to be running.
    pass
  # Push adb_reboot
  logging.info('  Pushing adb_reboot ...')
  adb_reboot = os.path.join(constants.DIR_SOURCE_ROOT,
                            'out/%s/adb_reboot' % target)
  device.PushChangedFiles([(adb_reboot, '/data/local/tmp/')])
  # Launch adb_reboot
  logging.info('  Launching adb_reboot ...')
  device.RunShellCommand([
      device.GetDevicePieWrapper(),
      '/data/local/tmp/adb_reboot'])


def _ConfigureLocalProperties(device, java_debug=True):
  """Set standard readonly testing device properties prior to reboot."""
  local_props = [
      'persist.sys.usb.config=adb',
      'ro.monkey=1',
      'ro.test_harness=1',
      'ro.audio.silent=1',
      'ro.setupwizard.mode=DISABLED',
      ]
  if java_debug:
    local_props.append('%s=all' % android_commands.JAVA_ASSERT_PROPERTY)
    local_props.append('debug.checkjni=1')
  try:
    device.WriteFile(
        constants.DEVICE_LOCAL_PROPERTIES_PATH,
        '\n'.join(local_props), as_root=True)
    # Android will not respect the local props file if it is world writable.
    device.RunShellCommand(
        ['chmod', '644', constants.DEVICE_LOCAL_PROPERTIES_PATH],
        as_root=True)
  except device_errors.CommandFailedError as e:
    logging.warning(str(e))

  # LOCAL_PROPERTIES_PATH = '/data/local.prop'

def WriteAdbKeysFile(device, adb_keys_string):
  dir_path = posixpath.dirname(constants.ADB_KEYS_FILE)
  device.RunShellCommand('mkdir -p %s' % dir_path, as_root=True)
  device.RunShellCommand('restorecon %s' % dir_path, as_root=True)
  device.WriteFile(constants.ADB_KEYS_FILE, adb_keys_string, as_root=True)
  device.RunShellCommand('restorecon %s' % constants.ADB_KEYS_FILE,
                         as_root=True)


def WipeDeviceData(device, options):
  """Wipes data from device, keeping only the adb_keys for authorization.

  After wiping data on a device that has been authorized, adb can still
  communicate with the device, but after reboot the device will need to be
  re-authorized because the adb keys file is stored in /data/misc/adb/.
  Thus, adb_keys file is rewritten so the device does not need to be
  re-authorized.

  Arguments:
    device: the device to wipe
  """
  device_authorized = device.FileExists(constants.ADB_KEYS_FILE)
  if device_authorized:
    adb_keys = device.ReadFile(constants.ADB_KEYS_FILE,
                               as_root=True).splitlines()
  device.RunShellCommand('wipe data', as_root=True)
  if device_authorized:
    adb_keys_set = set(adb_keys)
    for adb_key_file in options.adb_key_files or []:
      try:
        with open(adb_key_file, 'r') as f:
          adb_public_keys = f.readlines()
        adb_keys_set.update(adb_public_keys)
      except IOError:
        logging.warning('Unable to find adb keys file %s.' % adb_key_file)
    WriteAdbKeysFile(device, '\n'.join(adb_keys_set))


def WipeDeviceIfPossible(device, timeout, options):
  try:
    device.EnableRoot()
    WipeDeviceData(device, options)
    device.Reboot(True, timeout=timeout, retries=0)
  except (errors.DeviceUnresponsiveError, device_errors.CommandFailedError):
    pass


def ChargeDeviceToLevel(device, level):
  def device_charged():
    battery_level = device.GetBatteryInfo().get('level')
    if battery_level is None:
      logging.warning('Unable to find current battery level.')
      battery_level = 100
    else:
      logging.info('current battery level: %d', battery_level)
      battery_level = int(battery_level)
    return battery_level >= level

  timeout_retry.WaitFor(device_charged, wait_period=60)


def ProvisionDevice(device, options):
  if options.reboot_timeout:
    reboot_timeout = options.reboot_timeout
  elif (device.build_version_sdk >=
        constants.ANDROID_SDK_VERSION_CODES.LOLLIPOP):
    reboot_timeout = _DEFAULT_TIMEOUTS.LOLLIPOP
  else:
    reboot_timeout = _DEFAULT_TIMEOUTS.PRE_LOLLIPOP

  try:
    if not options.skip_wipe:
      WipeDeviceIfPossible(device, reboot_timeout, options)
    try:
      device.EnableRoot()
    except device_errors.CommandFailedError as e:
      logging.warning(str(e))
    _ConfigureLocalProperties(device, options.enable_java_debug)
    device_settings.ConfigureContentSettings(
        device, device_settings.DETERMINISTIC_DEVICE_SETTINGS)
    if options.disable_location:
      device_settings.ConfigureContentSettings(
          device, device_settings.DISABLE_LOCATION_SETTINGS)
    else:
      device_settings.ConfigureContentSettings(
          device, device_settings.ENABLE_LOCATION_SETTINGS)
    device_settings.SetLockScreenSettings(device)
    if options.disable_network:
      device_settings.ConfigureContentSettings(
          device, device_settings.NETWORK_DISABLED_SETTINGS)
    if options.min_battery_level is not None:
      try:
        device.SetCharging(True)
        ChargeDeviceToLevel(device, options.min_battery_level)
      except device_errors.CommandFailedError as e:
        logging.exception('Unable to charge device to specified level.')

    if not options.skip_wipe:
      device.Reboot(True, timeout=reboot_timeout, retries=0)
    device.RunShellCommand('date -s %s' % time.strftime('%Y%m%d.%H%M%S',
                                                        time.gmtime()),
                           as_root=True)
    props = device.RunShellCommand('getprop')
    for prop in props:
      logging.info('  %s' % prop)
    if options.auto_reconnect:
      PushAndLaunchAdbReboot(device, options.target)
  except (errors.WaitForResponseTimedOutError,
          device_errors.CommandTimeoutError):
    logging.info('Timed out waiting for device %s. Adding to blacklist.',
                 str(device))
    # Device black list is reset by bb_device_status_check.py per build.
    device_blacklist.ExtendBlacklist([str(device)])
  except device_errors.CommandFailedError:
    logging.exception('Failed to provision device %s. Adding to blacklist.',
                      str(device))
    device_blacklist.ExtendBlacklist([str(device)])


def ProvisionDevices(options):
  if options.device is not None:
    devices = [options.device]
  else:
    devices = android_commands.GetAttachedDevices()

  parallel_devices = device_utils.DeviceUtils.parallel(devices)
  parallel_devices.pMap(ProvisionDevice, options)
  if options.auto_reconnect:
    LaunchHostHeartbeat()
  blacklist = device_blacklist.ReadBlacklist()
  if all(d in blacklist for d in devices):
    raise device_errors.NoDevicesError
  return 0


def main():
  custom_handler = logging.StreamHandler(sys.stdout)
  custom_handler.setFormatter(run_tests_helper.CustomFormatter())
  logging.getLogger().addHandler(custom_handler)
  logging.getLogger().setLevel(logging.INFO)

  # Recommended options on perf bots:
  # --disable-network
  #     TODO(tonyg): We eventually want network on. However, currently radios
  #     can cause perfbots to drain faster than they charge.
  # --min-battery-level 95
  #     Some perf bots run benchmarks with USB charging disabled which leads
  #     to gradual draining of the battery. We must wait for a full charge
  #     before starting a run in order to keep the devices online.

  parser = argparse.ArgumentParser(
      description='Provision Android devices with settings required for bots.')
  parser.add_argument('-d', '--device', metavar='SERIAL',
                      help='the serial number of the device to be provisioned'
                      ' (the default is to provision all devices attached)')
  parser.add_argument('--skip-wipe', action='store_true', default=False,
                      help="don't wipe device data during provisioning")
  parser.add_argument('--reboot-timeout', metavar='SECS', type=int,
                      help='when wiping the device, max number of seconds to'
                      ' wait after each reboot '
                      '(default: %s)' % _DEFAULT_TIMEOUTS.HELP_TEXT)
  parser.add_argument('--min-battery-level', type=int, metavar='NUM',
                      help='wait for the device to reach this minimum battery'
                      ' level before trying to continue')
  parser.add_argument('--disable-location', action='store_true',
                      help='disable Google location services on devices')
  parser.add_argument('--disable-network', action='store_true',
                      help='disable network access on devices')
  parser.add_argument('--disable-java-debug', action='store_false',
                      dest='enable_java_debug', default=True,
                      help='disable Java property asserts and JNI checking')
  parser.add_argument('-t', '--target', default='Debug',
                      help='the build target (default: %(default)s)')
  parser.add_argument('-r', '--auto-reconnect', action='store_true',
                      help='push binary which will reboot the device on adb'
                      ' disconnections')
  parser.add_argument('--adb-key-files', type=str, nargs='+',
                      help='list of adb keys to push to device')
  args = parser.parse_args()
  constants.SetBuildType(args.target)

  return ProvisionDevices(args)


if __name__ == '__main__':
  sys.exit(main())
