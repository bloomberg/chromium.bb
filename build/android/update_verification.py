#!/usr/bin/env python
#
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs semi-automated update testing on a non-rooted device."""
import logging
import optparse
import os
import shutil
import sys
import time

from pylib import android_commands
from pylib.device import device_utils

def _SaveAppData(device, package_name, from_apk=None, data_dir=None):
  def _BackupAppData(data_dir=None):
    device.old_interface.Adb().SendCommand('backup %s' % package_name)
    backup_file = os.path.join(os.getcwd(), 'backup.ab')
    assert os.path.exists(backup_file), 'Backup failed.'
    if data_dir:
      if not os.path.isdir(data_dir):
        os.makedirs(data_dir)
      shutil.move(backup_file, data_dir)
      backup_file = os.path.join(data_dir, 'backup.ab')
    print 'Application data saved to %s' % backup_file

  if from_apk:
    logging.info('Installing %s...', from_apk)
    # TODO(jbudorick) Switch to AdbWrapper.Install on the impl switch.
    output = device.old_interface.Install(from_apk, reinstall=True)
    if 'Success' not in output:
      raise Exception('Unable to install %s. output: %s' % (from_apk, output))

  raw_input('Set the application state. Once ready, press enter and '
            'select "Backup my data" on the device.')
  _BackupAppData(data_dir)


def _VerifyAppUpdate(device, to_apk, app_data, from_apk=None):
  def _RestoreAppData():
    assert os.path.exists(app_data), 'Backup file does not exist!'
    device.old_interface.Adb().SendCommand('restore %s' % app_data)
    # It seems restore command is not synchronous.
    time.sleep(15)

  if from_apk:
    logging.info('Installing %s...', from_apk)
    # TODO(jbudorick) Switch to AdbWrapper.Install on the impl switch.
    output = device.old_interface.Install(from_apk, reinstall=True)
    if 'Success' not in output:
      raise Exception('Unable to install %s. output: %s' % (from_apk, output))

  logging.info('Restoring the application data...')
  raw_input('Press enter and select "Restore my data" on the device.')
  _RestoreAppData()

  logging.info('Verifying that %s cannot be installed side-by-side...',
               to_apk)
  # TODO(jbudorick) Switch to AdbWrapper.Install on the impl switch.
  output = device.old_interface.Install(to_apk)
  if 'INSTALL_FAILED_ALREADY_EXISTS' not in output:
    if 'Success' in output:
      raise Exception('Package name has changed! output: %s' % output)
    else:
      raise Exception(output)

  logging.info('Verifying that %s can be overinstalled...', to_apk)
  # TODO(jbudorick) Switch to AdbWrapper.Install on the impl switch.
  output = device.old_interface.Install(to_apk, reinstall=True)
  if 'Success' not in output:
    raise Exception('Unable to install %s.\n output: %s' % (to_apk, output))
  logging.info('Successfully updated to the new apk. Please verify that the '
               'the application data is preserved.')


def main():
  logger = logging.getLogger()
  logger.setLevel(logging.DEBUG)
  desc = (
      'Performs semi-automated application update verification testing. '
      'When given --save, it takes a snapshot of the application data '
      'on the device. (A dialog on the device will prompt the user to grant '
      'permission to backup the data.) Otherwise, it performs the update '
      'testing as follows: '
      '1. Installs the |from-apk| (optional). '
      '2. Restores the previously stored snapshot of application data '
      'given by |app-data| '
      '(A dialog on the device will prompt the user to grant permission to '
      'restore the data.) '
      '3. Verifies that |to-apk| cannot be installed side-by-side. '
      '4. Verifies that |to-apk| can replace |from-apk|.')
  parser = optparse.OptionParser(description=desc)
  parser.add_option('--package-name', help='Package name for the application.')
  parser.add_option('--save', action='store_true',
                    help=('Save a snapshot of application data. '
                          'This will be saved as backup.db in the '
                          'current directory if |app-data| directory '
                          'is not specifid.'))
  parser.add_option('--from-apk',
                    help=('APK to update from. This is optional if you already '
                          'have the app installed.'))
  parser.add_option('--to-apk', help='APK to update to.')
  parser.add_option('--app-data',
                    help=('Path to the application data to be restored or the '
                          'directory where the data should be saved.'))
  (options, args) = parser.parse_args()

  if args:
    parser.print_help(sys.stderr)
    parser.error('Unknown arguments: %s.' % args)

  devices = android_commands.GetAttachedDevices()
  if len(devices) != 1:
    parser.error('Exactly 1 device must be attached.')
  device = device_utils.DeviceUtils(devices[0])

  if options.from_apk:
    assert os.path.isfile(options.from_apk)

  if options.save:
    if not options.package_name:
      parser.print_help(sys.stderr)
      parser.error('Missing --package-name.')
    _SaveAppData(device, options.package_name, from_apk=options.from_apk,
                 data_dir=options.app_data)
  else:
    if not options.to_apk or not options.app_data:
      parser.print_help(sys.stderr)
      parser.error('Missing --to-apk or --app-data.')
    assert os.path.isfile(options.to_apk)
    assert os.path.isfile(options.app_data)
    _VerifyAppUpdate(device, options.to_apk, options.app_data,
                     from_apk=options.from_apk)


if __name__ == '__main__':
  main()
