#!/usr/bin/env python
#
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Provisions Android devices with settings required for bots.

Usage:
  ./provision_devices.py [-d <device serial number>]
"""

import logging
import optparse
import os
import re
import subprocess
import sys
import time

from pylib import android_commands
from pylib import constants
from pylib import device_settings
from pylib.cmd_helper import GetCmdOutput
from pylib.device import device_utils

def KillHostHeartbeat():
  ps = subprocess.Popen(['ps', 'aux'], stdout = subprocess.PIPE)
  stdout, _ = ps.communicate()
  matches = re.findall('\\n.*host_heartbeat.*', stdout)
  for match in matches:
    print 'An instance of host heart beart running... will kill'
    pid = re.findall('(\d+)', match)[1]
    subprocess.call(['kill', str(pid)])


def LaunchHostHeartbeat():
  # Kill if existing host_heartbeat
  KillHostHeartbeat()
  # Launch a new host_heartbeat
  print 'Spawning host heartbeat...'
  subprocess.Popen([os.path.join(constants.DIR_SOURCE_ROOT,
                                 'build/android/host_heartbeat.py')])


def PushAndLaunchAdbReboot(devices, target):
  """Pushes and launches the adb_reboot binary on the device.

  Arguments:
    devices: The list of serial numbers of the device to which the
             adb_reboot binary should be pushed.
    target : The build target (example, Debug or Release) which helps in
             locating the adb_reboot binary.
  """
  for device_serial in devices:
    print 'Will push and launch adb_reboot on %s' % device_serial
    device = device_utils.DeviceUtils(device_serial)
    # Kill if adb_reboot is already running.
    device.old_interface.KillAllBlocking('adb_reboot', 2)
    # Push adb_reboot
    print '  Pushing adb_reboot ...'
    adb_reboot = os.path.join(constants.DIR_SOURCE_ROOT,
                              'out/%s/adb_reboot' % target)
    device.old_interface.PushIfNeeded(adb_reboot, '/data/local/tmp/')
    # Launch adb_reboot
    print '  Launching adb_reboot ...'
    p = subprocess.Popen(['adb', '-s', device, 'shell'], stdin=subprocess.PIPE)
    p.communicate('/data/local/tmp/adb_reboot; exit\n')
  LaunchHostHeartbeat()


def _ConfigureLocalProperties(device):
  """Set standard readonly testing device properties prior to reboot."""
  local_props = [
      'ro.monkey=1',
      'ro.test_harness=1',
      'ro.audio.silent=1',
      'ro.setupwizard.mode=DISABLED',
      ]
  device.old_interface.SetProtectedFileContents(
      constants.DEVICE_LOCAL_PROPERTIES_PATH,
      '\n'.join(local_props))
  # Android will not respect the local props file if it is world writable.
  device.old_interface.RunShellCommandWithSU(
      'chmod 644 %s' % constants.DEVICE_LOCAL_PROPERTIES_PATH)

  # LOCAL_PROPERTIES_PATH = '/data/local.prop'


def WipeDeviceData(device):
  """Wipes data from device, keeping only the adb_keys for authorization.

  After wiping data on a device that has been authorized, adb can still
  communicate with the device, but after reboot the device will need to be
  re-authorized because the adb keys file is stored in /data/misc/adb/.
  Thus, before reboot the adb_keys file is rewritten so the device does not need
  to be re-authorized.

  Arguments:
    device: the device to wipe
  """
  device_authorized = device.old_interface.FileExistsOnDevice(
      constants.ADB_KEYS_FILE)
  if device_authorized:
    adb_keys = device.old_interface.RunShellCommandWithSU(
      'cat %s' % constants.ADB_KEYS_FILE)[0]
  device.old_interface.RunShellCommandWithSU('wipe data')
  if device_authorized:
    path_list = constants.ADB_KEYS_FILE.split('/')
    dir_path = '/'.join(path_list[:len(path_list)-1])
    device.old_interface.RunShellCommandWithSU('mkdir -p %s' % dir_path)
    adb_keys = device.old_interface.RunShellCommand(
      'echo %s > %s' % (adb_keys, constants.ADB_KEYS_FILE))
  device.Reboot()


def ProvisionDevices(options):
  if options.device is not None:
    devices = [options.device]
  else:
    devices = android_commands.GetAttachedDevices()
  for device_serial in devices:
    device = device_utils.DeviceUtils(device_serial)
    install_output = GetCmdOutput(
      ['%s/build/android/adb_install_apk.py' % constants.DIR_SOURCE_ROOT,
       '--apk',
       '%s/build/android/CheckInstallApk-debug.apk' % constants.DIR_SOURCE_ROOT
       ])
    failure_string = 'Failure [INSTALL_FAILED_INSUFFICIENT_STORAGE]'
    if failure_string in install_output:
      WipeDeviceData(device)
    _ConfigureLocalProperties(device)
    device_settings.ConfigureContentSettingsDict(
        device, device_settings.DETERMINISTIC_DEVICE_SETTINGS)
    # TODO(tonyg): We eventually want network on. However, currently radios
    # can cause perfbots to drain faster than they charge.
    if 'perf' in os.environ.get('BUILDBOT_BUILDERNAME', '').lower():
      device_settings.ConfigureContentSettingsDict(
          device, device_settings.NETWORK_DISABLED_SETTINGS)
    device.old_interface.RunShellCommandWithSU('date -u %f' % time.time())
  if options.auto_reconnect:
    PushAndLaunchAdbReboot(devices, options.target)


def main(argv):
  logging.basicConfig(level=logging.INFO)

  parser = optparse.OptionParser()
  parser.add_option('-d', '--device',
                    help='The serial number of the device to be provisioned')
  parser.add_option('-t', '--target', default='Debug', help='The build target')
  parser.add_option(
      '-r', '--auto-reconnect', action='store_true',
      help='Push binary which will reboot the device on adb disconnections.')
  options, args = parser.parse_args(argv[1:])
  constants.SetBuildType(options.target)

  if args:
    print >> sys.stderr, 'Unused args %s' % args
    return 1

  ProvisionDevices(options)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
