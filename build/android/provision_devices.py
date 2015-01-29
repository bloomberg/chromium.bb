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
  device.old_interface.GetAndroidToolStatusAndOutput(
      '/data/local/tmp/adb_reboot')


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


def WipeDeviceData(device):
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
    adb_keys = device.ReadFile(constants.ADB_KEYS_FILE, as_root=True)
  device.RunShellCommand('wipe data', as_root=True)
  if device_authorized:
    path_list = constants.ADB_KEYS_FILE.split('/')
    dir_path = '/'.join(path_list[:len(path_list)-1])
    device.RunShellCommand('mkdir -p %s' % dir_path, as_root=True)
    device.RunShellCommand('restorecon %s' % dir_path, as_root=True)
    device.WriteFile(constants.ADB_KEYS_FILE, adb_keys, as_root=True)
    device.RunShellCommand('restorecon %s' % constants.ADB_KEYS_FILE,
                           as_root=True)


def WipeDeviceIfPossible(device, timeout):
  try:
    device.EnableRoot()
    WipeDeviceData(device)
    device.Reboot(True, timeout=timeout, retries=0)
  except (errors.DeviceUnresponsiveError, device_errors.CommandFailedError):
    pass


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
      WipeDeviceIfPossible(device, reboot_timeout)
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
    if options.wait_for_battery:
      try:
        battery_info = device.old_interface.GetBatteryInfo()
      except Exception as e:
        battery_info = {}
        logging.error('Unable to obtain battery info for %s, %s',
                      str(device), e)

      while int(battery_info.get('level', 100)) < options.min_battery_level:
        if not device.old_interface.IsDeviceCharging():
          if device.old_interface.CanControlUsbCharging():
            device.old_interface.EnableUsbCharging()
          else:
            logging.error('Device is not charging')
            break
        logging.info('Waiting for device to charge. Current level=%s',
                     battery_info.get('level', 0))
        time.sleep(60)
        battery_info = device.old_interface.GetBatteryInfo()
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

  # TODO(perezju): This script used to rely on the builder name to determine
  # the desired device configuration for perf bots. To safely phase this out,
  # we now:
  # - expose these configuration settings as command line options
  # - set default values for these options based on the builder name, thus
  #   matching the previous behaviour of the script on all bots.
  # - explicitly adding these options on the perf bots will also maintain the
  #   script behaviour, namely:
  #     --wait-for-battery --disable-network --disable-java-debug
  # - after all perf-bot recipes are updated, we can remove the following
  #   builder-name-sniffing code and replace |is_perf| with |False|.
  is_perf = 'perf' in os.environ.get('BUILDBOT_BUILDERNAME', '').lower()

  # Recommended options on perf bots:
  # --disable-network
  #     TODO(tonyg): We eventually want network on. However, currently radios
  #     can cause perfbots to drain faster than they charge.
  # --wait-for-battery
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
  parser.add_argument('--wait-for-battery', action='store_true',
                      default=is_perf,
                      help='wait for the battery on the devices to charge')
  parser.add_argument('--min-battery-level', default=95, type=int,
                      metavar='NUM',
                      help='when waiting for battery, minimum battery level'
                      ' required to continue (default: %(default)s)')
  parser.add_argument('--disable-location', action='store_true',
                      help='disable Google location services on devices')
  parser.add_argument('--disable-network', action='store_true',
                      default=is_perf,
                      help='disable network access on devices')
  parser.add_argument('--disable-java-debug', action='store_false',
                      dest='enable_java_debug', default=not is_perf,
                      help='disable Java property asserts and JNI checking')
  parser.add_argument('-t', '--target', default='Debug',
                      help='the build target (default: %(default)s)')
  parser.add_argument('-r', '--auto-reconnect', action='store_true',
                      help='push binary which will reboot the device on adb'
                      ' disconnections')
  args = parser.parse_args()
  constants.SetBuildType(args.target)

  return ProvisionDevices(args)


if __name__ == '__main__':
  sys.exit(main())
