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
import shutil
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), os.pardir))
from devil.android import apk_helper
from devil.android import device_utils
from devil.android import device_errors
from devil.android.sdk import version_codes
from devil.utils import reraiser_thread
from pylib import constants
from pylib.utils import run_tests_helper
from pylib.utils import time_profile

sys.path.append(os.path.join(os.path.dirname(__file__), os.pardir, 'gyp'))
from util import build_utils


def _TransformDexPaths(paths):
  """Given paths like ["/a/b/c", "/a/c/d"], returns ["b.c", "c.d"]."""
  prefix_len = len(os.path.commonprefix(paths))
  return [p[prefix_len:].replace(os.sep, '.') for p in paths]


def main():
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
  parser.add_argument('--dex-files',
                      help='List of dex files to push.',
                      action='append',
                      default=[])
  parser.add_argument('-d', '--device', dest='device',
                      help='Target device for apk to install on.')
  parser.add_argument('--uninstall',
                      action='store_true',
                      default=False,
                      help='Remove the app and all side-loaded files.')
  parser.add_argument('--output-directory',
                      help='Path to the root build directory.')
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

  run_tests_helper.SetLogLevel(args.verbose_count)
  constants.SetBuildType('Debug')
  if args.output_directory:
    constants.SetOutputDirectory(args.output_directory)

  main_timer = time_profile.TimeProfile()
  install_timer = time_profile.TimeProfile()
  push_native_timer = time_profile.TimeProfile()
  push_dex_timer = time_profile.TimeProfile()

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

  apk_help = apk_helper.ApkHelper(args.apk_path)
  apk_package = apk_help.GetPackageName()
  device_incremental_dir = '/data/local/tmp/incremental-app-%s' % apk_package

  if args.uninstall:
    device.Uninstall(apk_package)
    device.RunShellCommand(['rm', '-rf', device_incremental_dir],
                           check_return=True)
    logging.info('Uninstall took %s seconds.', main_timer.GetDelta())
    return

  if device.build_version_sdk >= version_codes.MARSHMALLOW:
    if apk_help.HasIsolatedProcesses():
      raise Exception('Cannot use perform incremental installs on Android M+ '
                      'without first disabling isolated processes. Use GN arg: '
                      'disable_incremental_isolated_processes=true to do so.')

  # Install .apk(s) if any of them have changed.
  def do_install():
    install_timer.Start()
    if args.splits:
      splits = []
      for split_glob in args.splits:
        splits.extend((f for f in glob.glob(split_glob)))
      device.InstallSplitApk(args.apk_path, splits, reinstall=True,
                             allow_cached_props=True)
    else:
      device.Install(args.apk_path, reinstall=True)
    install_timer.Stop(log=False)

  # Push .so and .dex files to the device (if they have changed).
  def do_push_files():
    if args.lib_dir:
      push_native_timer.Start()
      device_lib_dir = posixpath.join(device_incremental_dir, 'lib')
      device.PushChangedFiles([(args.lib_dir, device_lib_dir)],
                              delete_device_stale=True)
      push_native_timer.Stop(log=False)

    if args.dex_files:
      push_dex_timer.Start()
      # Put all .dex files to be pushed into a temporary directory so that we
      # can use delete_device_stale=True.
      with build_utils.TempDir() as temp_dir:
        device_dex_dir = posixpath.join(device_incremental_dir, 'dex')
        # Ensure no two files have the same name.
        transformed_names = _TransformDexPaths(args.dex_files)
        for src_path, dest_name in zip(args.dex_files, transformed_names):
          shutil.copyfile(src_path, os.path.join(temp_dir, dest_name))
        device.PushChangedFiles([(temp_dir, device_dex_dir)],
                                delete_device_stale=True)
      push_dex_timer.Stop(log=False)

  # Create 2 lock files:
  # * install.lock tells the app to pause on start-up (until we release it).
  # * firstrun.lock is used by the app to pause all secondary processes until
  #   the primary process finishes loading the .dex / .so files.
  def create_lock_files():
    # Creates or zeros out lock files.
    cmd = ('D="%s";'
           'mkdir -p $D &&'
           'echo -n >$D/install.lock 2>$D/firstrun.lock')
    device.RunShellCommand(cmd % device_incremental_dir, check_return=True)

  # The firstrun.lock is released by the app itself.
  def release_installer_lock():
    device.RunShellCommand('echo > %s/install.lock' % device_incremental_dir,
                           check_return=True)

  create_lock_files()
  # Concurrency here speeds things up quite a bit, but DeviceUtils hasn't
  # been designed for multi-threading. Enabling only because this is a
  # developer-only tool.
  if args.no_threading:
    do_install()
    do_push_files()
  else:
    reraiser_thread.RunAsync((do_install, do_push_files))
  release_installer_lock()
  logging.info('Took %s seconds (install=%s, libs=%s, dex=%s)',
               main_timer.GetDelta(), install_timer.GetDelta(),
               push_native_timer.GetDelta(), push_dex_timer.GetDelta())


if __name__ == '__main__':
  sys.exit(main())

