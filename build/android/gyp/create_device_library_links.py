#!/usr/bin/env python
#
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Creates symlinks to native libraries for an APK.

The native libraries should have previously been pushed to the device (in
options.target_dir). This script then creates links in an apk's lib/ folder to
those native libraries.
"""

import json
import optparse
import os
import sys

BUILD_ANDROID_DIR = os.path.join(os.path.dirname(__file__), '..')
sys.path.append(BUILD_ANDROID_DIR)

from pylib import android_commands
from pylib import build_utils
from pylib.utils import apk_helper


def CreateLinks(options):
  libraries = build_utils.ReadJson(options.libraries_json)
  apk_package = apk_helper.GetPackageName(options.apk)

  # There is a large (~100ms) overhead for each call to adb.RunShellCommand. To
  # avoid this overhead, craft a single command that creates all the links.
  link = '/data/data/' + apk_package + '/lib/$f'
  target = options.target_dir + '/$f'
  names = ' '.join(libraries)
  cmd = (
      'for f in ' + names + '; do \n' +
        'rm ' + link + ' > /dev/null 2>&1 \n' +
        'ln -s ' + target + ' ' + link + '\n' +
      'done'
      )

  adb = android_commands.AndroidCommands()
  result = adb.RunShellCommand(cmd)

  if result:
    raise Exception(
        'Unexpected output creating links on device.\n' +
        '\n'.join(result))


def main(argv):
  parser = optparse.OptionParser()
  parser.add_option('--apk', help='Path to the apk.')
  parser.add_option('--libraries-json',
      help='Path to the json list of native libraries.')
  parser.add_option('--target-dir',
      help='Device directory that contains the target libraries for symlinks.')
  parser.add_option('--stamp', help='Path to touch on success.')
  options, _ = parser.parse_args()

  required_options = ['apk', 'libraries_json', 'target_dir']
  build_utils.CheckOptions(options, parser, required=required_options)

  CreateLinks(options)

  if options.stamp:
    build_utils.Touch(options.stamp)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
