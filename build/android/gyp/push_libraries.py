#!/usr/bin/env python
#
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Pushes native libraries to a device.

"""

import json
import optparse
import os
import sys

BUILD_ANDROID_DIR = os.path.join(os.path.dirname(__file__), '..')
sys.path.append(BUILD_ANDROID_DIR)

from pylib import android_commands
from pylib import build_utils


def DoPush(options):
  libraries = build_utils.ReadJson(options.libraries_json)

  adb = android_commands.AndroidCommands()
  adb.RunShellCommand('mkdir ' + options.device_dir)
  for lib in libraries:
    device_path = os.path.join(options.device_dir, lib)
    host_path = os.path.join(options.libraries_dir, lib)
    adb.PushIfNeeded(host_path, device_path)


def main(argv):
  parser = optparse.OptionParser()
  parser.add_option('--libraries-dir',
      help='Directory that contains stripped libraries.')
  parser.add_option('--device-dir',
      help='Device directory to push the libraries to.')
  parser.add_option('--libraries-json',
      help='Path to the json list of native libraries.')
  parser.add_option('--stamp', help='Path to touch on success.')
  options, _ = parser.parse_args()

  required_options = ['libraries_dir', 'device_dir', 'libraries_json']
  build_utils.CheckOptions(options, parser, required=required_options)

  DoPush(options)

  if options.stamp:
    build_utils.Touch(options.stamp)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
