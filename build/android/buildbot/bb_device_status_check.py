#!/usr/bin/env python
#
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A class to keep track of devices across builds and report state."""
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
  device_type = device_adb.old_interface.GetBuildProduct()
  device_build = device_adb.old_interface.GetBuildId()
  device_build_type = device_adb.old_interface.GetBuildType()
  device_product_name = device_adb.old_interface.GetProductName()

  try:
    battery = device_adb.old_interface.GetBatteryInfo()
  except Exception as e:
    battery = None
    logging.error('Unable to obtain battery info for %s, %s', serial, e)

  def _GetData(re_expression, line, lambda_function=lambda x:x):
    if not line:
      return 'Unknown'
    found = re.findall(re_expression, line)
    if found and len(found):
      return lambda_function(found[0])
    return 'Unknown'

  ac_power = _GetData('AC powered: (\w+)', battery)
  battery_level = _GetData('level: (\d+)', battery)
  battery_temp = _GetData('temperature: (\d+)', battery,
                          lambda x: float(x) / 10.0)
  imei_slice = _GetData('Device ID = (\d+)',
                        device_adb.old_interface.GetSubscriberInfo(),
                        lambda x: x[-6:])
  report = ['Device %s (%s)' % (serial, device_type),
            '  Build: %s (%s)' %
              (device_build, device_adb.old_interface.GetBuildFingerprint()),
            '  Battery: %s%%' % battery_level,
            '  Battery temp: %s' % battery_temp,
            '  IMEI slice: %s' % imei_slice,
            '  Wifi IP: %s' % device_adb.old_interface.GetWifiIP(),
            '']

  errors = []
  if battery_level < 15:
    errors += ['Device critically low in battery. Turning off device.']
  if not options.no_provisioning_check:
    setup_wizard_disabled = (
        device_adb.old_interface.GetSetupWizardStatus() == 'DISABLED')
    if not setup_wizard_disabled and device_build_type != 'user':
      errors += ['Setup wizard not disabled. Was it provisioned correctly?']
  if device_product_name == 'mantaray' and ac_power != 'true':
    errors += ['Mantaray device not connected to AC power.']

  # Turn off devices with low battery.
  if battery_level < 15:
    device_adb.old_interface.EnableAdbRoot()
    device_adb.old_interface.Shutdown()
  full_report = '\n'.join(report)
  return device_type, device_build, battery_level, full_report, errors, True


def GetLastDevices(out_dir):
  """Returns a list of devices that have been seen on the bot.

  Args:
    options: out_dir parameter of options argument is used as the base
             directory to load and update the cache file.

  Returns: List of device serial numbers that were on the bot.
  """
  devices_path = os.path.join(out_dir, '.last_devices')
  devices = []
  try:
    with open(devices_path) as f:
      devices = f.read().splitlines()
  except IOError:
    # Ignore error, file might not exist
    pass
  return devices


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

  def WriteDeviceList(file_name, device_list):
    path = os.path.join(out_dir, file_name)
    if not os.path.exists(out_dir):
      os.makedirs(out_dir)
    with open(path, 'w') as f:
      # Write devices currently visible plus devices previously seen.
      f.write('\n'.join(set(device_list)))

  last_devices_path = os.path.join(out_dir, '.last_devices')
  last_devices = GetLastDevices(out_dir)
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
            'adb devices(GetAttachedDevices): %s' %
                android_commands.GetAttachedDevices()]
  else:
    new_devs = set(adb_online_devs) - set(last_devices)
    if new_devs and os.path.exists(last_devices_path):
      bb_annotations.PrintWarning()
      bb_annotations.PrintSummaryText(
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
      except psutil.error.NoSuchProcess:
        pass

  for sig in [signal.SIGTERM, signal.SIGQUIT, signal.SIGKILL]:
    for p in GetAllAdb():
      try:
        print 'kill %d %d (%s [%s])' % (sig, p.pid, p.name,
            ' '.join(p.cmdline))
        p.send_signal(sig)
      except psutil.error.NoSuchProcess:
        pass
  for p in GetAllAdb():
    try:
      print 'Unable to kill %d (%s [%s])' % (p.pid, p.name, ' '.join(p.cmdline))
    except psutil.error.NoSuchProcess:
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
  options, args = parser.parse_args()
  if args:
    parser.error('Unknown options %s' % args)

  # Remove the last build's "bad devices" before checking device statuses.
  device_blacklist.ResetBlacklist()

  if options.restart_usb:
    expected_devices = GetLastDevices(os.path.abspath(options.out_dir))
    devices = android_commands.GetAttachedDevices()
    # Only restart usb if devices are missing.
    if set(expected_devices) != set(devices):
      KillAllAdb()
      retries = 5
      usb_restarted = True
      if not RestartUsb():
        usb_restarted = False
        bb_annotations.PrintWarning()
        print 'USB reset stage failed, wait for any device to come back.'
      while retries:
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

  devices = android_commands.GetAttachedDevices()
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
    SendDeviceStatusAlert(msg)

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

  if False in fail_step_lst:
    # TODO(navabi): Build fails on device status check step if there exists any
    # devices with critically low battery. Remove those devices from testing,
    # allowing build to continue with good devices.
    return 1

  if not devices:
    return 1


if __name__ == '__main__':
  sys.exit(main())
