#!/usr/bin/env python
#
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A class to keep track of devices across builds and report state."""

import argparse
import json
import logging
import os
import psutil
import re
import signal
import sys

sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))
from devil.android import battery_utils
from devil.android import device_blacklist
from devil.android import device_errors
from devil.android import device_list
from devil.android import device_utils
from devil.android.sdk import adb_wrapper
from devil.utils import lsusb
from devil.utils import reset_usb
from devil.utils import run_tests_helper
from pylib import constants

_RE_DEVICE_ID = re.compile(r'Device ID = (\d+)')


def KillAllAdb():
  def GetAllAdb():
    for p in psutil.process_iter():
      try:
        if 'adb' in p.name:
          yield p
      except (psutil.NoSuchProcess, psutil.AccessDenied):
        pass

  for sig in [signal.SIGTERM, signal.SIGQUIT, signal.SIGKILL]:
    for p in GetAllAdb():
      try:
        logging.info('kill %d %d (%s [%s])', sig, p.pid, p.name,
                     ' '.join(p.cmdline))
        p.send_signal(sig)
      except (psutil.NoSuchProcess, psutil.AccessDenied):
        pass
  for p in GetAllAdb():
    try:
      logging.error('Unable to kill %d (%s [%s])', p.pid, p.name,
                    ' '.join(p.cmdline))
    except (psutil.NoSuchProcess, psutil.AccessDenied):
      pass


def _IsBlacklisted(serial, blacklist):
  return blacklist and serial in blacklist.Read()


def _BatteryStatus(device, blacklist):
  battery_info = {}
  try:
    battery = battery_utils.BatteryUtils(device)
    battery_info = battery.GetBatteryInfo(timeout=5)
    battery_level = int(battery_info.get('level', 100))

    if battery_level < 15:
      logging.error('Critically low battery level (%d)', battery_level)
      battery = battery_utils.BatteryUtils(device)
      if not battery.GetCharging():
        battery.SetCharging(True)
      if blacklist:
        blacklist.Extend([device.adb.GetDeviceSerial()])

  except device_errors.CommandFailedError:
    logging.exception('Failed to get battery information for %s',
                      str(device))

  return battery_info


def _IMEISlice(device):
  imei_slice = ''
  try:
    for l in device.RunShellCommand(['dumpsys', 'iphonesubinfo'],
                                    check_return=True, timeout=5):
      m = _RE_DEVICE_ID.match(l)
      if m:
        imei_slice = m.group(1)[-6:]
  except device_errors.CommandFailedError:
    logging.exception('Failed to get IMEI slice for %s', str(device))

  return imei_slice


def DeviceStatus(devices, blacklist):
  """Generates status information for the given devices.

  Args:
    devices: The devices to generate status for.
    blacklist: The current device blacklist.
  Returns:
    A dict of the following form:
    {
      '<serial>': {
        'serial': '<serial>',
        'adb_status': str,
        'usb_status': bool,
        'blacklisted': bool,
        # only if the device is connected and not blacklisted
        'type': ro.build.product,
        'build': ro.build.id,
        'build_detail': ro.build.fingerprint,
        'battery': {
          ...
        },
        'imei_slice': str,
        'wifi_ip': str,
      },
      ...
    }
  """
  adb_devices = {
    a[0].GetDeviceSerial(): a
    for a in adb_wrapper.AdbWrapper.Devices(desired_state=None, long_list=True)
  }
  usb_devices = set(lsusb.get_android_devices())

  def blacklisting_device_status(device):
    serial = device.adb.GetDeviceSerial()
    adb_status = (
        adb_devices[serial][1] if serial in adb_devices
        else 'unknown')
    usb_status = bool(serial in usb_devices)

    device_status = {
      'serial': serial,
      'adb_status': adb_status,
      'usb_status': usb_status,
    }

    if adb_status == 'device':
      if not _IsBlacklisted(serial, blacklist):
        try:
          build_product = device.build_product
          build_id = device.build_id
          build_fingerprint = device.GetProp('ro.build.fingerprint', cache=True)
          wifi_ip = device.GetProp('dhcp.wlan0.ipaddress')
          battery_info = _BatteryStatus(device, blacklist)
          imei_slice = _IMEISlice(device)

          if (device.product_name == 'mantaray' and
              battery_info.get('AC powered', None) != 'true'):
            logging.error('Mantaray device not connected to AC power.')

          device_status.update({
            'ro.build.product': build_product,
            'ro.build.id': build_id,
            'ro.build.fingerprint': build_fingerprint,
            'battery': battery_info,
            'imei_slice': imei_slice,
            'wifi_ip': wifi_ip,

            # TODO(jbudorick): Remove these once no clients depend on them.
            'type': build_product,
            'build': build_id,
            'build_detail': build_fingerprint,
          })

        except device_errors.CommandFailedError:
          logging.exception('Failure while getting device status for %s.',
                            str(device))
          if blacklist:
            blacklist.Extend([serial])

        except device_errors.CommandTimeoutError:
          logging.exception('Timeout while getting device status for %s.',
                            str(device))
          if blacklist:
            blacklist.Extend([serial])

    elif blacklist:
      blacklist.Extend([serial])

    device_status['blacklisted'] = _IsBlacklisted(serial, blacklist)

    return device_status

  parallel_devices = device_utils.DeviceUtils.parallel(devices)
  statuses = parallel_devices.pMap(blacklisting_device_status).pGet(None)
  return statuses


def RecoverDevices(devices, blacklist):
  """Attempts to recover any inoperable devices in the provided list.

  Args:
    devices: The list of devices to attempt to recover.
    blacklist: The current device blacklist, which will be used then
      reset.
  Returns:
    Nothing.
  """

  statuses = DeviceStatus(devices, blacklist)

  should_restart_usb = set(
      status['serial'] for status in statuses
      if (not status['usb_status']
          or status['adb_status'] == 'unknown'))
  should_restart_adb = should_restart_usb.union(set(
      status['serial'] for status in statuses
      if status['adb_status'] in ('offline', 'unauthorized')))
  should_reboot_device = should_restart_adb.union(set(
      status['serial'] for status in statuses
      if status['blacklisted']))

  logging.debug('Should restart USB for:')
  for d in should_restart_usb:
    logging.debug('  %s', d)
  logging.debug('Should restart ADB for:')
  for d in should_restart_adb:
    logging.debug('  %s', d)
  logging.debug('Should reboot:')
  for d in should_reboot_device:
    logging.debug('  %s', d)

  if blacklist:
    blacklist.Reset()

  if should_restart_adb:
    KillAllAdb()
  for serial in should_restart_usb:
    try:
      reset_usb.reset_android_usb(serial)
    except (IOError, device_errors.DeviceUnreachableError):
      logging.exception('Unable to reset USB for %s.', serial)
      if blacklist:
        blacklist.Extend([serial])

  def blacklisting_recovery(device):
    if _IsBlacklisted(device.adb.GetDeviceSerial(), blacklist):
      logging.debug('%s is blacklisted, skipping recovery.', str(device))
      return

    if str(device) in should_reboot_device:
      try:
        device.WaitUntilFullyBooted(retries=0)
        return
      except (device_errors.CommandTimeoutError,
              device_errors.CommandFailedError):
        logging.exception('Failure while waiting for %s. '
                          'Attempting to recover.', str(device))

      try:
        try:
          device.Reboot(block=False, timeout=5, retries=0)
        except device_errors.CommandTimeoutError:
          logging.warning('Timed out while attempting to reboot %s normally.'
                          'Attempting alternative reboot.', str(device))
          # The device drops offline before we can grab the exit code, so
          # we don't check for status.
          device.adb.Root()
          device.adb.Shell('echo b > /proc/sysrq-trigger', expect_status=None,
                           timeout=5, retries=0)
      except device_errors.CommandFailedError:
        logging.exception('Failed to reboot %s.', str(device))
        if blacklist:
          blacklist.Extend([device.adb.GetDeviceSerial()])
      except device_errors.CommandTimeoutError:
        logging.exception('Timed out while rebooting %s.', str(device))
        if blacklist:
          blacklist.Extend([device.adb.GetDeviceSerial()])

      try:
        device.WaitUntilFullyBooted(retries=0)
      except device_errors.CommandFailedError:
        logging.exception('Failure while waiting for %s.', str(device))
        if blacklist:
          blacklist.Extend([device.adb.GetDeviceSerial()])
      except device_errors.CommandTimeoutError:
        logging.exception('Timed out while waiting for %s.', str(device))
        if blacklist:
          blacklist.Extend([device.adb.GetDeviceSerial()])

  device_utils.DeviceUtils.parallel(devices).pMap(blacklisting_recovery)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--out-dir',
                      help='Directory where the device path is stored',
                      default=os.path.join(constants.DIR_SOURCE_ROOT, 'out'))
  parser.add_argument('--restart-usb', action='store_true',
                      help='DEPRECATED. '
                           'This script now always tries to reset USB.')
  parser.add_argument('--json-output',
                      help='Output JSON information into a specified file.')
  parser.add_argument('--blacklist-file', help='Device blacklist JSON file.')
  parser.add_argument('-v', '--verbose', action='count', default=1,
                      help='Log more information.')

  args = parser.parse_args()

  run_tests_helper.SetLogLevel(args.verbose)

  blacklist = (device_blacklist.Blacklist(args.blacklist_file)
               if args.blacklist_file
               else None)

  last_devices_path = os.path.join(
      args.out_dir, device_list.LAST_DEVICES_FILENAME)
  try:
    expected_devices = set(
        device_list.GetPersistentDeviceList(last_devices_path))
  except IOError:
    expected_devices = set()

  usb_devices = set(lsusb.get_android_devices())
  devices = [device_utils.DeviceUtils(s)
             for s in expected_devices.union(usb_devices)]

  RecoverDevices(devices, blacklist)
  statuses = DeviceStatus(devices, blacklist)

  # Log the state of all devices.
  for status in statuses:
    logging.info(status['serial'])
    adb_status = status.get('adb_status')
    blacklisted = status.get('blacklisted')
    logging.info('  USB status: %s',
                 'online' if status.get('usb_status') else 'offline')
    logging.info('  ADB status: %s', adb_status)
    logging.info('  Blacklisted: %s', str(blacklisted))
    if adb_status == 'device' and not blacklisted:
      logging.info('  Device type: %s', status.get('ro.build.product'))
      logging.info('  OS build: %s', status.get('ro.build.id'))
      logging.info('  OS build fingerprint: %s',
                   status.get('ro.build.fingerprint'))
      logging.info('  Battery state:')
      for k, v in status.get('battery', {}).iteritems():
        logging.info('    %s: %s', k, v)
      logging.info('  IMEI slice: %s', status.get('imei_slice'))
      logging.info('  WiFi IP: %s', status.get('wifi_ip'))

  # Update the last devices file.
  device_list.WritePersistentDeviceList(
      last_devices_path,
      [status['serial'] for status in statuses])

  # Write device info to file for buildbot info display.
  if os.path.exists('/home/chrome-bot'):
    with open('/home/chrome-bot/.adb_device_info', 'w') as f:
      for status in statuses:
        try:
          if status['adb_status'] == 'device':
            f.write('{serial} {adb_status} {build_product} {build_id} '
                    '{temperature:.1f}C {level}%\n'.format(
                serial=status['serial'],
                adb_status=status['adb_status'],
                build_product=status['type'],
                build_id=status['build'],
                temperature=float(status['battery']['temperature']) / 10,
                level=status['battery']['level']
            ))
          else:
            f.write('{serial} {adb_status}'.format(
                serial=status['serial'],
                adb_status=status['adb_status']
            ))
        except Exception: # pylint: disable=broad-except
          pass

  # Dump the device statuses to JSON.
  if args.json_output:
    with open(args.json_output, 'wb') as f:
      f.write(json.dumps(statuses, indent=4))

  live_devices = [status['serial'] for status in statuses
                  if (status['adb_status'] == 'device'
                      and not _IsBlacklisted(status['serial'], blacklist))]

  # If all devices failed, or if there are no devices, it's an infra error.
  return 0 if live_devices else constants.INFRA_EXIT_CODE


if __name__ == '__main__':
  sys.exit(main())
