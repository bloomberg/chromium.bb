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
import datetime
import json
import logging
import os
import posixpath
import re
import subprocess
import sys
import time

from devil.android import battery_utils
from devil.android import device_blacklist
from devil.android import device_errors
from devil.android import device_utils
from devil.android.sdk import version_codes
from devil.utils import run_tests_helper
from devil.utils import timeout_retry
from pylib import constants
from pylib import device_settings

_SYSTEM_WEBVIEW_PATHS = ['/system/app/webview', '/system/app/WebViewGoogle']
_CHROME_PACKAGE_REGEX = re.compile('.*chrom.*')
_TOMBSTONE_REGEX = re.compile('tombstone.*')


class _DEFAULT_TIMEOUTS(object):
  # L can take a while to reboot after a wipe.
  LOLLIPOP = 600
  PRE_LOLLIPOP = 180

  HELP_TEXT = '{}s on L, {}s on pre-L'.format(LOLLIPOP, PRE_LOLLIPOP)


class _PHASES(object):
  WIPE = 'wipe'
  PROPERTIES = 'properties'
  FINISH = 'finish'

  ALL = [WIPE, PROPERTIES, FINISH]


def ProvisionDevices(args):
  if args.blacklist_file:
    blacklist = device_blacklist.Blacklist(args.blacklist_file)
  else:
    # TODO(jbudorick): Remove once the bots have switched over.
    blacklist = device_blacklist.Blacklist(device_blacklist.BLACKLIST_JSON)

  devices = device_utils.DeviceUtils.HealthyDevices(blacklist)
  if args.device:
    devices = [d for d in devices if d == args.device]
    if not devices:
      raise device_errors.DeviceUnreachableError(args.device)

  parallel_devices = device_utils.DeviceUtils.parallel(devices)
  parallel_devices.pMap(ProvisionDevice, blacklist, args)
  if args.auto_reconnect:
    _LaunchHostHeartbeat()
  blacklisted_devices = blacklist.Read()
  if args.output_device_blacklist:
    with open(args.output_device_blacklist, 'w') as f:
      json.dump(blacklisted_devices, f)
  if all(d in blacklisted_devices for d in devices):
    raise device_errors.NoDevicesError
  return 0


def ProvisionDevice(device, blacklist, options):
  if options.reboot_timeout:
    reboot_timeout = options.reboot_timeout
  elif (device.build_version_sdk >= version_codes.LOLLIPOP):
    reboot_timeout = _DEFAULT_TIMEOUTS.LOLLIPOP
  else:
    reboot_timeout = _DEFAULT_TIMEOUTS.PRE_LOLLIPOP

  def should_run_phase(phase_name):
    return not options.phases or phase_name in options.phases

  def run_phase(phase_func, reboot=True):
    try:
      device.WaitUntilFullyBooted(timeout=reboot_timeout, retries=0)
    except device_errors.CommandTimeoutError:
      logging.error('Device did not finish booting. Will try to reboot.')
      device.Reboot(timeout=reboot_timeout)
    phase_func(device, options)
    if reboot:
      device.Reboot(False, retries=0)
      device.adb.WaitForDevice()

  try:
    if should_run_phase(_PHASES.WIPE):
      if options.chrome_specific_wipe:
        run_phase(WipeChromeData)
      else:
        run_phase(WipeDevice)

    if should_run_phase(_PHASES.PROPERTIES):
      run_phase(SetProperties)

    if should_run_phase(_PHASES.FINISH):
      run_phase(FinishProvisioning, reboot=False)

  except device_errors.CommandTimeoutError:
    logging.exception('Timed out waiting for device %s. Adding to blacklist.',
                      str(device))
    blacklist.Extend([str(device)])

  except device_errors.CommandFailedError:
    logging.exception('Failed to provision device %s. Adding to blacklist.',
                      str(device))
    blacklist.Extend([str(device)])


def WipeChromeData(device, options):
  """Wipes chrome specific data from device
  Chrome specific data is:
  (1) any dir under /data/data/ whose name matches *chrom*, except
      com.android.chrome, which is the chrome stable package
  (2) any dir under /data/app/ and /data/app-lib/ whose name matches *chrom*
  (3) any files under /data/tombstones/ whose name matches "tombstone*"
  (4) /data/local.prop if there is any
  (5) /data/local/chrome-command-line if there is any
  (6) dir /data/local/.config/ if there is any (this is telemetry related)
  (7) dir /data/local/tmp/

  Arguments:
    device: the device to wipe
  """
  if options.skip_wipe:
    return

  try:
    device.EnableRoot()
    _WipeUnderDirIfMatch(device, '/data/data/', _CHROME_PACKAGE_REGEX,
                         constants.PACKAGE_INFO['chrome_stable'].package)
    _WipeUnderDirIfMatch(device, '/data/app/', _CHROME_PACKAGE_REGEX)
    _WipeUnderDirIfMatch(device, '/data/app-lib/', _CHROME_PACKAGE_REGEX)
    _WipeUnderDirIfMatch(device, '/data/tombstones/', _TOMBSTONE_REGEX)

    _WipeFileOrDir(device, '/data/local.prop')
    _WipeFileOrDir(device, '/data/local/chrome-command-line')
    _WipeFileOrDir(device, '/data/local/.config/')
    _WipeFileOrDir(device, '/data/local/tmp/')

    device.RunShellCommand('rm -rf %s/*' % device.GetExternalStoragePath(),
                           check_return=True)
  except device_errors.CommandFailedError:
    logging.exception('Possible failure while wiping the device. '
                      'Attempting to continue.')


def WipeDevice(device, options):
  """Wipes data from device, keeping only the adb_keys for authorization.

  After wiping data on a device that has been authorized, adb can still
  communicate with the device, but after reboot the device will need to be
  re-authorized because the adb keys file is stored in /data/misc/adb/.
  Thus, adb_keys file is rewritten so the device does not need to be
  re-authorized.

  Arguments:
    device: the device to wipe
  """
  if options.skip_wipe:
    return

  try:
    device.EnableRoot()
    device_authorized = device.FileExists(constants.ADB_KEYS_FILE)
    if device_authorized:
      adb_keys = device.ReadFile(constants.ADB_KEYS_FILE,
                                 as_root=True).splitlines()
    device.RunShellCommand(['wipe', 'data'],
                           as_root=True, check_return=True)
    device.adb.WaitForDevice()

    if device_authorized:
      adb_keys_set = set(adb_keys)
      for adb_key_file in options.adb_key_files or []:
        try:
          with open(adb_key_file, 'r') as f:
            adb_public_keys = f.readlines()
          adb_keys_set.update(adb_public_keys)
        except IOError:
          logging.warning('Unable to find adb keys file %s.' % adb_key_file)
      _WriteAdbKeysFile(device, '\n'.join(adb_keys_set))
  except device_errors.CommandFailedError:
    logging.exception('Possible failure while wiping the device. '
                      'Attempting to continue.')


def _WriteAdbKeysFile(device, adb_keys_string):
  dir_path = posixpath.dirname(constants.ADB_KEYS_FILE)
  device.RunShellCommand(['mkdir', '-p', dir_path],
                         as_root=True, check_return=True)
  device.RunShellCommand(['restorecon', dir_path],
                         as_root=True, check_return=True)
  device.WriteFile(constants.ADB_KEYS_FILE, adb_keys_string, as_root=True)
  device.RunShellCommand(['restorecon', constants.ADB_KEYS_FILE],
                         as_root=True, check_return=True)


def SetProperties(device, options):
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

  if options.disable_mock_location:
    device_settings.ConfigureContentSettings(
        device, device_settings.DISABLE_MOCK_LOCATION_SETTINGS)
  else:
    device_settings.ConfigureContentSettings(
        device, device_settings.ENABLE_MOCK_LOCATION_SETTINGS)

  device_settings.SetLockScreenSettings(device)
  if options.disable_network:
    device_settings.ConfigureContentSettings(
        device, device_settings.NETWORK_DISABLED_SETTINGS)

  if options.disable_system_chrome:
    # The system chrome version on the device interferes with some tests.
    device.RunShellCommand(['pm', 'disable', 'com.android.chrome'],
                           check_return=True)

  if options.remove_system_webview:
    if device.HasRoot():
      # This is required, e.g., to replace the system webview on a device.
      device.adb.Remount()
      device.RunShellCommand(['stop'], check_return=True)
      device.RunShellCommand(['rm', '-rf'] + _SYSTEM_WEBVIEW_PATHS,
                             check_return=True)
      device.RunShellCommand(['start'], check_return=True)
    else:
      logging.warning('Cannot remove system webview from a non-rooted device')


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
    local_props.append(
        '%s=all' % device_utils.DeviceUtils.JAVA_ASSERT_PROPERTY)
    local_props.append('debug.checkjni=1')
  try:
    device.WriteFile(
        device.LOCAL_PROPERTIES_PATH,
        '\n'.join(local_props), as_root=True)
    # Android will not respect the local props file if it is world writable.
    device.RunShellCommand(
        ['chmod', '644', device.LOCAL_PROPERTIES_PATH],
        as_root=True, check_return=True)
  except device_errors.CommandFailedError:
    logging.exception('Failed to configure local properties.')


def FinishProvisioning(device, options):
  if options.min_battery_level is not None:
    try:
      battery = battery_utils.BatteryUtils(device)
      battery.ChargeDeviceToLevel(options.min_battery_level)
    except device_errors.CommandFailedError:
      logging.exception('Unable to charge device to specified level.')

  if options.max_battery_temp is not None:
    try:
      battery = battery_utils.BatteryUtils(device)
      battery.LetBatteryCoolToTemperature(options.max_battery_temp)
    except device_errors.CommandFailedError:
      logging.exception('Unable to let battery cool to specified temperature.')

  def _set_and_verify_date():
    if (device.build_version_sdk >= version_codes.MARSHMALLOW):
      date_format = '%m%d%H%M%Y.%S'
      set_date_command = ['date']
    else:
      date_format = '%Y%m%d.%H%M%S'
      set_date_command = ['date', '-s']
    strgmtime = time.strftime(date_format, time.gmtime())
    set_date_command.append(strgmtime)
    device.RunShellCommand(set_date_command, as_root=True, check_return=True)

    device_time = device.RunShellCommand(
        ['date', '+"%Y%m%d.%H%M%S"'], as_root=True,
        single_line=True).replace('"', '')
    device_time = datetime.datetime.strptime(device_time, "%Y%m%d.%H%M%S")
    correct_time = datetime.datetime.strptime(strgmtime, date_format)
    tdelta = (correct_time - device_time).seconds
    if tdelta <= 1:
      logging.info('Date/time successfully set on %s', device)
      return True
    else:
      return False

  # Sometimes the date is not set correctly on the devices. Retry on failure.
  if not timeout_retry.WaitFor(_set_and_verify_date, wait_period=1,
                               max_tries=2):
    raise device_errors.CommandFailedError(
        'Failed to set date & time.', device_serial=str(device))

  props = device.RunShellCommand('getprop', check_return=True)
  for prop in props:
    logging.info('  %s' % prop)
  if options.auto_reconnect:
    _PushAndLaunchAdbReboot(device, options.target)


def _WipeUnderDirIfMatch(device, path, pattern, app_to_keep=None):
  ls_result = device.Ls(path)
  for (content, _) in ls_result:
    if pattern.match(content):
      if content != app_to_keep:
        _WipeFileOrDir(device, path + content)


def _WipeFileOrDir(device, path):
  if device.PathExists(path):
    device.RunShellCommand(['rm', '-rf', path], check_return=True)


def _PushAndLaunchAdbReboot(device, target):
  """Pushes and launches the adb_reboot binary on the device.

  Arguments:
    device: The DeviceUtils instance for the device to which the adb_reboot
            binary should be pushed.
    target: The build target (example, Debug or Release) which helps in
            locating the adb_reboot binary.
  """
  logging.info('Will push and launch adb_reboot on %s' % str(device))
  # Kill if adb_reboot is already running.
  device.KillAll('adb_reboot', blocking=True, timeout=2, quiet=True)
  # Push adb_reboot
  logging.info('  Pushing adb_reboot ...')
  adb_reboot = os.path.join(constants.DIR_SOURCE_ROOT,
                            'out/%s/adb_reboot' % target)
  device.PushChangedFiles([(adb_reboot, '/data/local/tmp/')])
  # Launch adb_reboot
  logging.info('  Launching adb_reboot ...')
  device.RunShellCommand(
      ['/data/local/tmp/adb_reboot'],
      check_return=True)


def _LaunchHostHeartbeat():
  # Kill if existing host_heartbeat
  KillHostHeartbeat()
  # Launch a new host_heartbeat
  logging.info('Spawning host heartbeat...')
  subprocess.Popen([os.path.join(constants.DIR_SOURCE_ROOT,
                                 'build/android/host_heartbeat.py')])


def KillHostHeartbeat():
  ps = subprocess.Popen(['ps', 'aux'], stdout=subprocess.PIPE)
  stdout, _ = ps.communicate()
  matches = re.findall('\\n.*host_heartbeat.*', stdout)
  for match in matches:
    logging.info('An instance of host heart beart running... will kill')
    pid = re.findall(r'(\S+)', match)[1]
    subprocess.call(['kill', str(pid)])


def main():
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
  parser.add_argument('--blacklist-file', help='Device blacklist JSON file.')
  parser.add_argument('--phase', action='append', choices=_PHASES.ALL,
                      dest='phases',
                      help='Phases of provisioning to run. '
                           '(If omitted, all phases will be run.)')
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
  parser.add_argument('--disable-mock-location', action='store_true',
                      default=False, help='Set ALLOW_MOCK_LOCATION to false')
  parser.add_argument('--disable-network', action='store_true',
                      help='disable network access on devices')
  parser.add_argument('--disable-java-debug', action='store_false',
                      dest='enable_java_debug', default=True,
                      help='disable Java property asserts and JNI checking')
  parser.add_argument('--disable-system-chrome', action='store_true',
                      help='Disable the system chrome from devices.')
  parser.add_argument('--remove-system-webview', action='store_true',
                      help='Remove the system webview from devices.')
  parser.add_argument('-t', '--target', default='Debug',
                      help='the build target (default: %(default)s)')
  parser.add_argument('-r', '--auto-reconnect', action='store_true',
                      help='push binary which will reboot the device on adb'
                      ' disconnections')
  parser.add_argument('--adb-key-files', type=str, nargs='+',
                      help='list of adb keys to push to device')
  parser.add_argument('-v', '--verbose', action='count', default=1,
                      help='Log more information.')
  parser.add_argument('--max-battery-temp', type=int, metavar='NUM',
                      help='Wait for the battery to have this temp or lower.')
  parser.add_argument('--output-device-blacklist',
                      help='Json file to output the device blacklist.')
  parser.add_argument('--chrome-specific-wipe', action='store_true',
                      help='only wipe chrome specific data during provisioning')
  args = parser.parse_args()
  constants.SetBuildType(args.target)

  run_tests_helper.SetLogLevel(args.verbose)

  return ProvisionDevices(args)


if __name__ == '__main__':
  sys.exit(main())
