#!/usr/bin/env python
#
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A class to keep track of devices across builds and report state."""
import logging
import optparse
import os
import smtplib
import sys
import re

from pylib import android_commands
from pylib import buildbot_report
from pylib import constants
from pylib.cmd_helper import GetCmdOutput


def DeviceInfo(serial):
  """Gathers info on a device via various adb calls.

  Args:
    serial: The serial of the attached device to construct info about.

  Returns:
    Tuple of device type, build id, report as a string, error messages, and
    boolean indicating whether or not device can be used for testing.
  """

  def AdbShellCmd(cmd):
    return GetCmdOutput('adb -s %s shell %s' % (serial, cmd),
                        shell=True).strip()

  device_adb = android_commands.AndroidCommands(serial)

  # TODO(navabi): Replace AdbShellCmd with device_adb.
  device_type = AdbShellCmd('getprop ro.build.product')
  device_build = AdbShellCmd('getprop ro.build.id')
  device_build_type = AdbShellCmd('getprop ro.build.type')
  device_product_name = AdbShellCmd('getprop ro.product.name')

  setup_wizard_disabled = AdbShellCmd(
      'getprop ro.setupwizard.mode') == 'DISABLED'
  battery = AdbShellCmd('dumpsys battery')
  install_output = GetCmdOutput(
    ['%s/build/android/adb_install_apk.py' % constants.DIR_SOURCE_ROOT, '--apk',
     '%s/build/android/CheckInstallApk-debug.apk' % constants.DIR_SOURCE_ROOT])
  install_speed_found = re.findall('(\d+) KB/s', install_output)
  if install_speed_found:
    install_speed = int(install_speed_found[0])
  else:
    install_speed = 'Unknown'
  if 'Error' in battery:
    ac_power = 'Unknown'
    battery_level = 'Unknown'
    battery_temp = 'Unknown'
  else:
    ac_power = re.findall('AC powered: (\w+)', battery)[0]
    battery_level = int(re.findall('level: (\d+)', battery)[0])
    battery_temp = float(re.findall('temperature: (\d+)', battery)[0]) / 10
  report = ['Device %s (%s)' % (serial, device_type),
            '  Build: %s (%s)' % (device_build,
                                  AdbShellCmd('getprop ro.build.fingerprint')),
            '  Battery: %s%%' % battery_level,
            '  Battery temp: %s' % battery_temp,
            '  IMEI slice: %s' % AdbShellCmd('dumpsys iphonesubinfo '
                                             '| grep Device'
                                             "| awk '{print $4}'")[-6:],
            '  Wifi IP: %s' % AdbShellCmd('getprop dhcp.wlan0.ipaddress'),
            '  Install Speed: %s KB/s' % install_speed,
            '']

  errors = []
  if battery_level < 15:
    errors += ['Device critically low in battery. Turning off device.']
  if not setup_wizard_disabled and device_build_type != 'user':
    errors += ['Setup wizard not disabled. Was it provisioned correctly?']
  if device_product_name == 'mantaray' and ac_power != 'true':
    errors += ['Mantaray device not connected to AC power.']
  # TODO(navabi): Insert warning once we have a better handle of what install
  # speeds to expect. The following lines were causing too many alerts.
  # if install_speed < 500:
  #   errors += ['Device install speed too low. Do not use for testing.']

  # Causing the device status check step fail for slow install speed or low
  # battery currently is too disruptive to the bots (especially try bots).
  # Turn off devices with low battery and the step does not fail.
  if battery_level < 15:
    device_adb.EnableAdbRoot()
    AdbShellCmd('reboot -p')
  return device_type, device_build, '\n'.join(report), errors, True


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

  def ReadDeviceList(file_name):
    devices_path = os.path.join(out_dir, file_name)
    devices = []
    try:
      with open(devices_path) as f:
        devices = f.read().splitlines()
    except IOError:
      # Ignore error, file might not exist
      pass
    return devices

  def WriteDeviceList(file_name, device_list):
    path = os.path.join(out_dir, file_name)
    if not os.path.exists(out_dir):
      os.makedirs(out_dir)
    with open(path, 'w') as f:
      # Write devices currently visible plus devices previously seen.
      f.write('\n'.join(set(device_list)))

  last_devices_path = os.path.join(out_dir, '.last_devices')
  last_devices = ReadDeviceList('.last_devices')
  missing_devs = list(set(last_devices) - set(adb_online_devs))

  all_known_devices = list(set(adb_online_devs) | set(last_devices))
  WriteDeviceList('.last_devices', all_known_devices)
  WriteDeviceList('.last_missing', missing_devs)

  if not all_known_devices:
    # This can happen if for some reason the .last_devices file is not
    # present or if it was empty.
    return ['No online devices. Have any devices been plugged in?']
  if missing_devs:
    devices_missing_msg = '%d devices not detected.' % len(missing_devs)
    buildbot_report.PrintSummaryText(devices_missing_msg)

    # TODO(navabi): Debug by printing both output from GetCmdOutput and
    # GetAttachedDevices to compare results.
    return ['Current online devices: %s' % adb_online_devs,
            '%s are no longer visible. Were they removed?\n' % missing_devs,
            'SHERIFF: See go/chrome_device_monitor',
            'Cache file: %s\n\n' % last_devices_path,
            'adb devices: %s' % GetCmdOutput(['adb', 'devices']),
            'adb devices(GetAttachedDevices): %s' %
            android_commands.GetAttachedDevices()]
  else:
    new_devs = set(adb_online_devs) - set(last_devices)
    if new_devs and os.path.exists(last_devices_path):
      buildbot_report.PrintWarning()
      buildbot_report.PrintSummaryText(
          '%d new devices detected' % len(new_devs))
      print ('New devices detected %s. And now back to your '
             'regularly scheduled program.' % list(new_devs))


def SendDeviceStatusAlert(msg):
  from_address = 'buildbot@chromium.org'
  to_address = 'chromium-android-device-alerts@google.com'
  bot_name = os.environ.get('BUILDBOT_BUILDERNAME')
  slave_name = os.environ.get('BUILDBOT_SLAVENAME')
  subject = 'Device status check errors on %s, %s.' % (slave_name, bot_name)
  msg_body = '\r\n'.join(['From: %s' % from_address, 'To: %s' % to_address,
                          'Subject: %s' % subject, '', msg])
  try:
    server = smtplib.SMTP('localhost')
    server.sendmail(from_address, [to_address], msg_body)
    server.quit()
  except Exception as e:
    print 'Failed to send alert email. Error: %s' % e


def main():
  parser = optparse.OptionParser()
  parser.add_option('', '--out-dir',
                    help='Directory where the device path is stored',
                    default=os.path.join(os.path.dirname(__file__), '..',
                                         '..', 'out'))

  options, args = parser.parse_args()
  if args:
    parser.error('Unknown options %s' % args)
  devices = android_commands.GetAttachedDevices()
  types, builds, reports, errors = [], [], [], []
  fail_step_lst = []
  if devices:
    types, builds, reports, errors, fail_step_lst = zip(*[DeviceInfo(dev)
                                                          for dev in devices])

  err_msg = CheckForMissingDevices(options, devices) or []

  unique_types = list(set(types))
  unique_builds = list(set(builds))

  buildbot_report.PrintMsg('Online devices: %d. Device types %s, builds %s'
                           % (len(devices), unique_types, unique_builds))
  print '\n'.join(reports)

  for serial, dev_errors in zip(devices, errors):
    if dev_errors:
      err_msg += ['%s errors:' % serial]
      err_msg += ['    %s' % error for error in dev_errors]

  if err_msg:
    buildbot_report.PrintWarning()
    msg = '\n'.join(err_msg)
    print msg
    SendDeviceStatusAlert(msg)

  if False in fail_step_lst:
    # TODO(navabi): Build fails on device status check step if there exists any
    # devices with critically low battery or install speed. Remove those devices
    # from testing, allowing build to continue with good devices.
    return 1

  if not devices:
    return 1


if __name__ == '__main__':
  sys.exit(main())
