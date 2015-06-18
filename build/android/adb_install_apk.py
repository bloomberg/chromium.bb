#!/usr/bin/env python
#
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility script to install APKs from the command line quickly."""

import argparse
import logging
import os
import sys

from pylib import constants
from pylib.device import device_blacklist
from pylib.device import device_errors
from pylib.device import device_utils
from pylib.utils import run_tests_helper


def main():
  parser = argparse.ArgumentParser()

  apk_group = parser.add_mutually_exclusive_group(required=True)
  apk_group.add_argument('--apk', dest='apk_name',
                         help='DEPRECATED The name of the apk containing the'
                              ' application (with the .apk extension).')
  apk_group.add_argument('apk_path', nargs='?',
                         help='The path to the APK to install.')

  # TODO(jbudorick): Remove once no clients pass --apk_package
  parser.add_argument('--apk_package', help='DEPRECATED unused')
  parser.add_argument('--keep_data',
                      action='store_true',
                      default=False,
                      help='Keep the package data when installing '
                           'the application.')
  parser.add_argument('--debug', action='store_const', const='Debug',
                      dest='build_type',
                      default=os.environ.get('BUILDTYPE', 'Debug'),
                      help='If set, run test suites under out/Debug. '
                           'Default is env var BUILDTYPE or Debug')
  parser.add_argument('--release', action='store_const', const='Release',
                      dest='build_type',
                      help='If set, run test suites under out/Release. '
                           'Default is env var BUILDTYPE or Debug.')
  parser.add_argument('-d', '--device', dest='device',
                      help='Target device for apk to install on.')
  parser.add_argument('-v', '--verbose', action='count',
                      help='Enable verbose logging.')

  args = parser.parse_args()

  run_tests_helper.SetLogLevel(args.verbose)
  constants.SetBuildType(args.build_type)

  apk = args.apk_path or args.apk_name
  if not apk.endswith('.apk'):
    apk += '.apk'
  if not os.path.exists(apk):
    apk = os.path.join(constants.GetOutDirectory(), 'apks', apk)
    if not os.path.exists(apk):
      parser.error('%s not found.' % apk)

  devices = device_utils.DeviceUtils.HealthyDevices()

  if args.device:
    devices = [d for d in devices if d == args.device]
    if not devices:
      raise device_errors.DeviceUnreachableError(args.device)
  elif not devices:
    raise device_errors.NoDevicesError()

  def blacklisting_install(device):
    try:
      device.Install(apk, reinstall=args.keep_data)
    except device_errors.CommandFailedError:
      logging.exception('Failed to install %s', args.apk)
      device_blacklist.ExtendBlacklist([str(device)])
      logging.warning('Blacklisting %s', str(device))
    except device_errors.CommandTimeoutError:
      logging.exception('Timed out while installing %s', args.apk)
      device_blacklist.ExtendBlacklist([str(device)])
      logging.warning('Blacklisting %s', str(device))

  device_utils.DeviceUtils.parallel(devices).pMap(blacklisting_install)


if __name__ == '__main__':
  sys.exit(main())

