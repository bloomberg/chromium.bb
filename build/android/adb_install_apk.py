#!/usr/bin/env python
#
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility script to install APKs from the command line quickly."""

import optparse
import os
import sys

from pylib import android_commands
from pylib import constants
from pylib.device import device_utils
from pylib.utils import apk_helper


def AddInstallAPKOption(option_parser):
  """Adds apk option used to install the APK to the OptionParser."""
  option_parser.add_option('--apk',
                           help=('The name of the apk containing the '
                                 ' application (with the .apk extension).'))
  option_parser.add_option('--apk_package',
                           help=('The package name used by the apk containing '
                                 'the application.'))
  option_parser.add_option('--keep_data',
                           action='store_true',
                           default=False,
                           help=('Keep the package data when installing '
                                 'the application.'))
  option_parser.add_option('--debug', action='store_const', const='Debug',
                           dest='build_type',
                           default=os.environ.get('BUILDTYPE', 'Debug'),
                           help='If set, run test suites under out/Debug. '
                           'Default is env var BUILDTYPE or Debug')
  option_parser.add_option('--release', action='store_const', const='Release',
                           dest='build_type',
                           help='If set, run test suites under out/Release. '
                           'Default is env var BUILDTYPE or Debug.')


def ValidateInstallAPKOption(option_parser, options):
  """Validates the apk option and potentially qualifies the path."""
  if not options.apk:
    option_parser.error('--apk is mandatory.')
  if not os.path.exists(options.apk):
    options.apk = os.path.join(constants.GetOutDirectory(), 'apks',
                               options.apk)


def main(argv):
  parser = optparse.OptionParser()
  AddInstallAPKOption(parser)
  options, args = parser.parse_args(argv)
  constants.SetBuildType(options.build_type)
  ValidateInstallAPKOption(parser, options)
  if len(args) > 1:
    raise Exception('Error: Unknown argument:', args[1:])

  devices = android_commands.GetAttachedDevices()
  if not devices:
    raise Exception('Error: no connected devices')

  if not options.apk_package:
    options.apk_package = apk_helper.GetPackageName(options.apk)

  device_utils.DeviceUtils.parallel(devices).old_interface.ManagedInstall(
      options.apk, options.keep_data, options.apk_package).pFinish(None)


if __name__ == '__main__':
  sys.exit(main(sys.argv))

