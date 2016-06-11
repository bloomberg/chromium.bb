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
import re
import sys

sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))
import devil_chromium
from devil.android import device_blacklist
from devil.android import device_list
from devil.android import device_utils
from devil.android.tools import device_recovery
from devil.android.tools import device_status
from devil.constants import exit_codes
from devil.utils import lsusb
from devil.utils import run_tests_helper
from pylib.constants import host_paths

_RE_DEVICE_ID = re.compile(r'Device ID = (\d+)')


def main():
  parser = argparse.ArgumentParser()
  device_status.AddArguments(parser)
  parser.add_argument('--out-dir',
                      help='DEPRECATED. '
                           'Directory where the device path is stored',
                      default=os.path.join(host_paths.DIR_SOURCE_ROOT, 'out'))
  parser.add_argument('--restart-usb', action='store_true',
                      help='DEPRECATED. '
                           'This script now always tries to reset USB.')
  args = parser.parse_args()

  run_tests_helper.SetLogLevel(args.verbose)

  devil_chromium.Initialize(adb_path=args.adb_path)

  blacklist = (device_blacklist.Blacklist(args.blacklist_file)
               if args.blacklist_file
               else None)

  last_devices_path = os.path.join(
      args.out_dir, device_list.LAST_DEVICES_FILENAME)
  args.known_devices_files.append(last_devices_path)

  expected_devices = set()
  try:
    for path in args.known_devices_files:
      if os.path.exists(path):
        expected_devices.update(device_list.GetPersistentDeviceList(path))
  except IOError:
    logging.warning('Problem reading %s, skipping.', path)

  logging.info('Expected devices:')
  for device in expected_devices:
    logging.info('  %s', device)

  usb_devices = set(lsusb.get_android_devices())
  devices = [device_utils.DeviceUtils(s)
             for s in expected_devices.union(usb_devices)]

  device_recovery.RecoverDevices(devices, blacklist)
  statuses = device_status.DeviceStatus(devices, blacklist)

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

  # Update the last devices file(s).
  for path in args.known_devices_files:
    device_list.WritePersistentDeviceList(
        path, [status['serial'] for status in statuses])

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
          elif status.get('usb_status', False):
            f.write('{serial} {adb_status}\n'.format(
                serial=status['serial'],
                adb_status=status['adb_status']
            ))
          else:
            f.write('{serial} offline\n'.format(
                serial=status['serial']
            ))
        except Exception: # pylint: disable=broad-except
          pass

  # Dump the device statuses to JSON.
  if args.json_output:
    with open(args.json_output, 'wb') as f:
      f.write(json.dumps(statuses, indent=4))

  live_devices = [
      status['serial'] for status in statuses
      if (status['adb_status'] == 'device'
          and not device_status.IsBlacklisted(status['serial'], blacklist))]

  # If all devices failed, or if there are no devices, it's an infra error.
  return 0 if live_devices else exit_codes.INFRA


if __name__ == '__main__':
  sys.exit(main())
