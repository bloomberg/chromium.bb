#!/usr/bin/env python
#
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import fnmatch
import optparse
import os
import sys

from util import md5_check

BUILD_ANDROID_DIR = os.path.join(os.path.dirname(__file__), '..')
sys.path.append(BUILD_ANDROID_DIR)

from pylib import build_utils


def DoDex(options, paths):
  dx_binary = os.path.join(options.android_sdk_root, 'platform-tools', 'dx')
  dex_cmd = [dx_binary, '--dex', '--output', options.dex_path] + paths

  md5_stamp = '%s.md5' % options.dex_path
  md5_checker = md5_check.Md5Checker(
      stamp=md5_stamp, inputs=paths, command=dex_cmd)
  if md5_checker.IsStale():
    build_utils.CheckCallDie(dex_cmd)
  else:
    build_utils.Touch(options.dex_path)
  md5_checker.Write()


def main(argv):
  parser = optparse.OptionParser()
  parser.add_option('--android-sdk-root', help='Android sdk root directory.')
  parser.add_option('--dex-path', help='Dex output path.')
  parser.add_option('--stamp', help='Path to touch on success.')

  # TODO(newt): remove this once http://crbug.com/177552 is fixed in ninja.
  parser.add_option('--ignore', help='Ignored.')

  options, paths = parser.parse_args()

  DoDex(options, paths)

  if options.stamp:
    build_utils.Touch(options.stamp)


if __name__ == '__main__':
  sys.exit(main(sys.argv))

