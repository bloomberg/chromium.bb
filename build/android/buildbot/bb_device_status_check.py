#!/usr/bin/env python
#
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A class to keep track of devices across builds and report state."""
import json
import logging
import optparse
import os
import psutil
import re
import signal
import smtplib
import subprocess
import sys
import time
import urllib

import bb_annotations
import bb_utils

sys.path.append(os.path.join(os.path.dirname(__file__),
                             os.pardir, os.pardir, 'util', 'lib',
                             'common'))
import perf_tests_results_helper  # pylint: disable=F0401

sys.path.append(os.path.join(os.path.dirname(__file__), '..'))
from pylib import android_commands
from pylib import constants
from pylib.cmd_helper import GetCmdOutput
from pylib.device import device_blacklist
from pylib.device import device_errors
from pylib.device import device_list
from pylib.device import device_utils

def DeviceInfo(serial, options):
  """Gathers info on a device via various adb calls.

  Args:
    serial: The serial of the attached device to construct info about.

  Returns:
    Tuple of device type, build id, report as a string, error messages, and
    boolean indicating whether or not device can be used for testing.
  """

  device_adb = device_utils.DeviceUtils(serial)
  device_type = device_adb.GetProp('ro.build.product')
  device_build = device_adb.GetProp('ro.build.id')
  device_build_type = device_adb.GetProp('ro.build.type')
  device_product_name = device_adb.GetProp('ro.product.name')

  try:
    battery_info = device_adb.old_interface.GetBatteryInfo()
  except Exception as e:
    battery_info = {}
    logging.error('Unable to obtain battery info for %s, %s', serial, e)

  def _GetData(re_expression, line, lambda_function=lambda x:x):
    if not line:
      return 'Unknown'
    found = re.findall(re_expression, line)
    if found and len(found):
      return lambda_function(found[0])
    return 'Unknown'

  battery_level = int(battery_info.get('level', 100))
  imei_slice = _GetData('Device ID = (\d+)',
                        device_adb.old_interface.GetSubscriberInfo(),
                        lambda x: x[-6:])
  report = ['Device %s (%s)' % (serial, device_type),
            '  Build: %s (%s)' %
              (device_build, device_adb.GetProp('ro.build.fingerprint')),
            '  Current Battery Service state: ',
            '\n'.join(['    %s: %s' % (k, v)
                       for k, v in battery_info.iteritems()]),
            '  IMEI slice: %s' % imei_slice,
            '  Wifi IP: %s' % device_adb.GetProp('dhcp.wlan0.ipaddress'),
            '']

  errors = []
  dev_good = True
  if battery_level < 15:
    errors += ['Device critically low in battery. Turning off device.']
    dev_good = False
  if not options.no_provisioning_check:
    setup_wizard_disabled = (
        device_adb.GetProp('ro.setupwizard.mode') == 'DISABLED')
    if not setup_wizard_disabled and device_build_type != 'user':
      errors += ['Setup wizard not disabled. Was it provisioned correctly?']
  if (device_product_name == 'mantaray' and
      battery_info.get('AC powered', None) != 'true'):
    errors += ['Mantaray device not connected to AC power.']

  # Turn off devices with low battery.
  if battery_level < 15:
    try:
      device_adb.EnableRoot()
    except device_errors.CommandFailedError as e:
      # Attempt shutdown anyway.
      # TODO(jbudorick) Handle this exception appropriately after interface
      #                 conversions are finished.
      logging.error(str(e))
    device_adb.old_interface.Shutdown()
  full_report = '\n'.join(report)
  return device_type, device_build, battery_level, full_report, errors, dev_good


def CheckForMissingDevices(options, adb_online_devs):
  """Uses file of previous online devices to detect broken phones.

  Args:
    options: out_dir parameter of options argument is used as the base
             directory to load and update the cache file.
    adb_online_devs: A list of serial numbers of the currently visible
                     and online attached devices.
  """
  # TODO(navabi): remove this once the bug that causes different number
  # of devices to be detected between calls is fixed.
  logger = logging.getLogger()
  logger.setLevel(logging.INFO)

  out_dir = os.path.abspath(options.out_dir)

  # last_devices denotes all known devices prior to this run
  last_devices_path = os.path.join(out_dir, device_list.LAST_DEVICES_FILENAME)
  last_missing_devices_path = os.path.join(out_dir,
      device_list.LAST_MISSING_DEVICES_FILENAME)
  try:
    last_devices = device_list.GetPersistentDeviceList(last_devices_path)
  except IOError:
    # Ignore error, file might not exist
    last_devices = []

  try:
    last_missing_devices = device_list.GetPersistentDeviceList(
        last_missing_devices_path)
  except IOError:
    last_missing_devices = []

  missing_devs = list(set(last_devices) - set(adb_online_devs))
  new_missing_devs = list(set(missing_devs) - set(last_missing_devices))

  if new_missing_devs and os.environ.get('BUILDBOT_SLAVENAME'):
    logging.info('new_missing_devs %s' % new_missing_devs)
    devices_missing_msg = '%d devices not detected.' % len(missing_devs)
    bb_annotations.PrintSummaryText(devices_missing_msg)

    from_address = 'chrome-bot@chromium.org'
    to_addresses = ['chrome-labs-tech-ticket@google.com']
    subject = 'Devices offline on %s, %s, %s' % (
      os.environ.get('BUILDBOT_SLAVENAME'),
      os.environ.get('BUILDBOT_BUILDERNAME'),
      os.environ.get('BUILDBOT_BUILDNUMBER'))
    msg = ('Please reboot the following devices:\n%s' %
           '\n'.join(map(str,new_missing_devs)))
    SendEmail(from_address, to_addresses, subject, msg)

  all_known_devices = list(set(adb_online_devs) | set(last_devices))
  device_list.WritePersistentDeviceList(last_devices_path, all_known_devices)
  device_list.WritePersistentDeviceList(last_missing_devices_path, missing_devs)

  if not all_known_devices:
    # This can happen if for some reason the .last_devices file is not
    # present or if it was empty.
    return ['No online devices. Have any devices been plugged in?']
  if missing_devs:
    devices_missing_msg = '%d devices not detected.' % len(missing_devs)
    bb_annotations.PrintSummaryText(devices_missing_msg)

    # TODO(navabi): Debug by printing both output from GetCmdOutput and
    # GetAttachedDevices to compare results.
    crbug_link = ('https://code.google.com/p/chromium/issues/entry?summary='
                  '%s&comment=%s&labels=Restrict-View-Google,OS-Android,Infra' %
                  (urllib.quote('Device Offline'),
                   urllib.quote('Buildbot: %s %s\n'
                                'Build: %s\n'
                                '(please don\'t change any labels)' %
                                (os.environ.get('BUILDBOT_BUILDERNAME'),
                                 os.environ.get('BUILDBOT_SLAVENAME'),
                                 os.environ.get('BUILDBOT_BUILDNUMBER')))))
    return ['Current online devices: %s' % adb_online_devs,
            '%s are no longer visible. Were they removed?\n' % missing_devs,
            'SHERIFF:\n',
            '@@@STEP_LINK@Click here to file a bug@%s@@@\n' % crbug_link,
            'Cache file: %s\n\n' % last_devices_path,
            'adb devices: %s' % GetCmdOutput(['adb', 'devices']),
            'adb devices(GetAttachedDevices): %s' % adb_online_devs]
  else:
    new_devs = set(adb_online_devs) - set(last_devices)
    if new_devs and os.path.exists(last_devices_path):
      bb_annotations.PrintWarning()
      bb_annotations.PrintSummaryText(
          '%d new devices detected' % len(new_devs))
      print ('New devices detected %s. And now back to your '
             'regularly scheduled program.' % list(new_devs))


def SendEmail(from_address, to_addresses, subject, msg):
  msg_body = '\r\n'.join(['From: %s' % from_address,
                          'To: %s' % ', '.join(to_addresses),
                          'Subject: %s' % subject, '', msg])
  try:
    server = smtplib.SMTP('localhost')
    server.sendmail(from_address, to_addresses, msg_body)
    server.quit()
  except Exception as e:
    print 'Failed to send alert email. Error: %s' % e


def RestartUsb():
  if not os.path.isfile('/usr/bin/restart_usb'):
    print ('ERROR: Could not restart usb. /usr/bin/restart_usb not installed '
           'on host (see BUG=305769).')
    return False

  lsusb_proc = bb_utils.SpawnCmd(['lsusb'], stdout=subprocess.PIPE)
  lsusb_output, _ = lsusb_proc.communicate()
  if lsusb_proc.returncode:
    print ('Error: Could not get list of USB ports (i.e. lsusb).')
    return lsusb_proc.returncode

  usb_devices = [re.findall('Bus (\d\d\d) Device (\d\d\d)', lsusb_line)[0]
                 for lsusb_line in lsusb_output.strip().split('\n')]

  all_restarted = True
  # Walk USB devices from leaves up (i.e reverse sorted) restarting the
  # connection. If a parent node (e.g. usb hub) is restarted before the
  # devices connected to it, the (bus, dev) for the hub can change, making the
  # output we have wrong. This way we restart the devices before the hub.
  for (bus, dev) in reversed(sorted(usb_devices)):
    # Can not restart root usb connections
    if dev != '001':
      return_code = bb_utils.RunCmd(['/usr/bin/restart_usb', bus, dev])
      if return_code:
        print 'Error restarting USB device /dev/bus/usb/%s/%s' % (bus, dev)
        all_restarted = False
      else:
        print 'Restarted USB device /dev/bus/usb/%s/%s' % (bus, dev)

  return all_restarted


def KillAllAdb():
  def GetAllAdb():
    for p in psutil.process_iter():
      try:
        if 'adb' in p.name:
          yield p
      except (psutil.error.NoSuchProcess, psutil.error.AccessDenied):
        pass

  for sig in [signal.SIGTERM, signal.SIGQUIT, signal.SIGKILL]:
    for p in GetAllAdb():
      try:
        print 'kill %d %d (%s [%s])' % (sig, p.pid, p.name,
            ' '.join(p.cmdline))
        p.send_signal(sig)
      except (psutil.error.NoSuchProcess, psutil.error.AccessDenied):
        pass
  for p in GetAllAdb():
    try:
      print 'Unable to kill %d (%s [%s])' % (p.pid, p.name, ' '.join(p.cmdline))
    except (psutil.error.NoSuchProcess, psutil.error.AccessDenied):
      pass


def main():
  parser = optparse.OptionParser()
  parser.add_option('', '--out-dir',
                    help='Directory where the device path is stored',
                    default=os.path.join(constants.DIR_SOURCE_ROOT, 'out'))
  parser.add_option('--no-provisioning-check', action='store_true',
                    help='Will not check if devices are provisioned properly.')
  parser.add_option('--device-status-dashboard', action='store_true',
                    help='Output device status data for dashboard.')
  parser.add_option('--restart-usb', action='store_true',
                    help='Restart USB ports before running device check.')
  parser.add_option('--json-output',
                    help='Output JSON information into a specified file.')

  options, args = parser.parse_args()
  if args:
    parser.error('Unknown options %s' % args)

  # Remove the last build's "bad devices" before checking device statuses.
  device_blacklist.ResetBlacklist()

  try:
    expected_devices = device_list.GetPersistentDeviceList(
        os.path.join(options.out_dir, device_list.LAST_DEVICES_FILENAME))
  except IOError:
    expected_devices = []
  devices = android_commands.GetAttachedDevices()
  # Only restart usb if devices are missing.
  if set(expected_devices) != set(devices):
    print 'expected_devices: %s, devices: %s' % (expected_devices, devices)
    KillAllAdb()
    retries = 5
    usb_restarted = True
    if options.restart_usb:
      if not RestartUsb():
        usb_restarted = False
        bb_annotations.PrintWarning()
        print 'USB reset stage failed, wait for any device to come back.'
    while retries:
      print 'retry adb devices...'
      time.sleep(1)
      devices = android_commands.GetAttachedDevices()
      if set(expected_devices) == set(devices):
        # All devices are online, keep going.
        break
      if not usb_restarted and devices:
        # The USB wasn't restarted, but there's at least one device online.
        # No point in trying to wait for all devices.
        break
      retries -= 1

  # TODO(navabi): Test to make sure this fails and then fix call
  offline_devices = android_commands.GetAttachedDevices(
      hardware=False, emulator=False, offline=True)

  types, builds, batteries, reports, errors = [], [], [], [], []
  fail_step_lst = []
  if devices:
    types, builds, batteries, reports, errors, fail_step_lst = (
        zip(*[DeviceInfo(dev, options) for dev in devices]))

  err_msg = CheckForMissingDevices(options, devices) or []

  unique_types = list(set(types))
  unique_builds = list(set(builds))

  bb_annotations.PrintMsg('Online devices: %d. Device types %s, builds %s'
                           % (len(devices), unique_types, unique_builds))
  print '\n'.join(reports)

  for serial, dev_errors in zip(devices, errors):
    if dev_errors:
      err_msg += ['%s errors:' % serial]
      err_msg += ['    %s' % error for error in dev_errors]

  if err_msg:
    bb_annotations.PrintWarning()
    msg = '\n'.join(err_msg)
    print msg
    from_address = 'buildbot@chromium.org'
    to_addresses = ['chromium-android-device-alerts@google.com']
    bot_name = os.environ.get('BUILDBOT_BUILDERNAME')
    slave_name = os.environ.get('BUILDBOT_SLAVENAME')
    subject = 'Device status check errors on %s, %s.' % (slave_name, bot_name)
    SendEmail(from_address, to_addresses, subject, msg)

  if options.device_status_dashboard:
    perf_tests_results_helper.PrintPerfResult('BotDevices', 'OnlineDevices',
                                              [len(devices)], 'devices')
    perf_tests_results_helper.PrintPerfResult('BotDevices', 'OfflineDevices',
                                              [len(offline_devices)], 'devices',
                                              'unimportant')
    for serial, battery in zip(devices, batteries):
      perf_tests_results_helper.PrintPerfResult('DeviceBattery', serial,
                                                [battery], '%',
                                                'unimportant')

  if options.json_output:
    with open(options.json_output, 'wb') as f:
      f.write(json.dumps({
        'online_devices': devices,
        'offline_devices': offline_devices,
        'expected_devices': expected_devices,
        'unique_types': unique_types,
        'unique_builds': unique_builds,
      }))

  if False in fail_step_lst:
    # TODO(navabi): Build fails on device status check step if there exists any
    # devices with critically low battery. Remove those devices from testing,
    # allowing build to continue with good devices.
    return 2

  if not devices:
    return 1


if __name__ == '__main__':
  sys.exit(main())
