#!/usr/bin/env python
#
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Install *_incremental.apk targets as well as their dependent files."""

import argparse
import glob
import logging
import os
import posixpath
import sys

from pylib import constants
from pylib.device import device_errors
from pylib.device import device_utils
from pylib.utils import apk_helper


def main():
  logging.basicConfig(level=logging.INFO,
                      format='%(asctime)s %(message)s')
  parser = argparse.ArgumentParser()

  parser.add_argument('apk_path',
                      help='The path to the APK to install.')
  parser.add_argument('--split',
                      action='append',
                      dest='splits',
                      help='A glob matching the apk splits. '
                           'Can be specified multiple times.')
  parser.add_argument('--lib-dir',
                      help='Path to native libraries directory.')
  parser.add_argument('-d', '--device', dest='device',
                      help='Target device for apk to install on.')
  parser.add_argument('--uninstall',
                      action='store_true',
                      default=False,
                      help='Remove the app and all side-loaded files.')

  args = parser.parse_args()

  constants.SetBuildType('Debug')

  if args.device:
    # Retries are annoying when commands fail for legitimate reasons. Might want
    # to enable them if this is ever used on bots though.
    device = device_utils.DeviceUtils(args.device, default_retries=0)
  else:
    devices = device_utils.DeviceUtils.HealthyDevices(default_retries=0)
    if not devices:
      raise device_errors.NoDevicesError()
    elif len(devices) > 1:
      raise Exception('More than one device available.\n'
                      'Use --device=SERIAL to select a device.')
    device = devices[0]

  apk_package = apk_helper.ApkHelper(args.apk_path).GetPackageName()
  device_incremental_dir = '/data/local/tmp/incremental-app-%s' % apk_package

  if args.uninstall:
    logging.info('Uninstalling .apk')
    device.Uninstall(apk_package)
    logging.info('Removing side-loaded files')
    device.RunShellCommand(['rm', '-rf', device_incremental_dir],
                           check_return=True)
    return

  # Install .apk(s) if any of them have changed.
  logging.info('Installing .apk')
  if args.splits:
    splits = []
    for split_glob in args.splits:
      splits.extend((f for f in glob.glob(split_glob)))
    device.InstallSplitApk(args.apk_path, splits, reinstall=True)
  else:
    device.Install(args.apk_path, reinstall=True)

  # Push .so files to the device (if they have changed).
  if args.lib_dir:
    logging.info('Pushing libraries')
    device_lib_dir = posixpath.join(device_incremental_dir, 'lib')
    device.PushChangedFiles([(args.lib_dir, device_lib_dir)],
                            delete_device_stale=True)


if __name__ == '__main__':
  sys.exit(main())

