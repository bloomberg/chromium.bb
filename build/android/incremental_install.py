#!/usr/bin/env python
#
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Install *_incremental.apk targets as well as their dependent files."""

import argparse
import glob
import logging
import posixpath
import sys
import time

from devil.android import apk_helper
from devil.android import device_utils
from devil.android import device_errors
from devil.utils import reraiser_thread
from pylib import constants
from pylib.utils import run_tests_helper


def main():
  start_time = time.time()
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
  parser.add_argument('--no-threading',
                      action='store_true',
                      default=False,
                      help='Do not install and push concurrently')
  parser.add_argument('-v',
                      '--verbose',
                      dest='verbose_count',
                      default=0,
                      action='count',
                      help='Verbose level (multiple times for more)')

  args = parser.parse_args()

  logging.basicConfig(format='%(asctime)s (%(thread)d) %(message)s')
  run_tests_helper.SetLogLevel(args.verbose_count)
  constants.SetBuildType('Debug')

  if args.device:
    # Retries are annoying when commands fail for legitimate reasons. Might want
    # to enable them if this is ever used on bots though.
    device = device_utils.DeviceUtils(args.device, default_retries=0)
  else:
    devices = device_utils.DeviceUtils.HealthyDevices(default_retries=0)
    if not devices:
      raise device_errors.NoDevicesError()
    elif len(devices) == 1:
      device = devices[0]
    else:
      all_devices = device_utils.DeviceUtils.parallel(devices)
      msg = ('More than one device available.\n'
             'Use --device=SERIAL to select a device.\n'
             'Available devices:\n')
      descriptions = all_devices.pMap(lambda d: d.build_description).pGet(None)
      for d, desc in zip(devices, descriptions):
        msg += '  %s (%s)\n' % (d, desc)
      raise Exception(msg)

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
  def do_install():
    if args.splits:
      splits = []
      for split_glob in args.splits:
        splits.extend((f for f in glob.glob(split_glob)))
      device.InstallSplitApk(args.apk_path, splits, reinstall=True,
                             allow_cached_props=True)
    else:
      device.Install(args.apk_path, reinstall=True)
    logging.info('Finished installing .apk')

  # Push .so files to the device (if they have changed).
  def do_push_libs():
    if args.lib_dir:
      device_lib_dir = posixpath.join(device_incremental_dir, 'lib')
      device.PushChangedFiles([(args.lib_dir, device_lib_dir)],
                              delete_device_stale=True)
      logging.info('Finished pushing native libs')

  # Concurrency here speeds things up quite a bit, but DeviceUtils hasn't
  # been designed for multi-threading. Enabling only because this is a
  # developer-only tool.
  if args.no_threading:
    do_install()
    do_push_libs()
  else:
    reraiser_thread.RunAsync((do_install, do_push_libs))
  logging.info('Took %s seconds', round(time.time() - start_time, 1))


if __name__ == '__main__':
  sys.exit(main())

