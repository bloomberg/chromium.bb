#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Prints the lowest locally available SDK version greater than or equal to a
given minimum sdk version to standard output.

If --developer_dir is passed, then the script will use the Xcode toolchain
located at DEVELOPER_DIR.

If --print_sdk_path is passed, then the script will also print the SDK path.
If --print_bin_path is passed, then the script will also print the path to the
toolchain bin dir.

Usage:
  python find_sdk.py [--developer_dir DEVELOPER_DIR] [--print_sdk_path] \
  [--print_bin_path] 10.6  # Ignores SDKs < 10.6

Sample Output:
/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.14.sdk
/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/
10.14
"""

from __future__ import print_function

import os
import re
import subprocess
import sys

from optparse import OptionParser


class SdkError(Exception):
  def __init__(self, value):
    self.value = value
  def __str__(self):
    return repr(self.value)


def parse_version(version_str):
  """'10.6' => [10, 6]"""
  return map(int, re.findall(r'(\d+)', version_str))


def main():
  parser = OptionParser()
  parser.add_option("--verify",
                    action="store_true", dest="verify", default=False,
                    help="return the sdk argument and warn if it doesn't exist")
  parser.add_option("--sdk_path",
                    action="store", type="string", dest="sdk_path", default="",
                    help="user-specified SDK path; bypasses verification")
  parser.add_option("--print_sdk_path",
                    action="store_true", dest="print_sdk_path", default=False,
                    help="Additionally print the path the SDK (appears first).")
  parser.add_option("--print_bin_path",
                    action="store_true", dest="print_bin_path", default=False,
                    help="Additionally print the path the toolchain bin dir.")
  parser.add_option("--developer_dir", help='Path to Xcode.')
  options, args = parser.parse_args()
  if len(args) != 1:
    parser.error('Please specify a minimum SDK version')
  min_sdk_version = args[0]


  # If we are passed the developer dir, then we don't need to call xcode-select.
  # This is important to avoid since we want to minimize dependencies on the
  # xcode toolchain.
  if options.developer_dir:
    dev_dir = os.path.join(options.developer_dir, 'Contents/Developer')
  else:
    job = subprocess.Popen(['xcode-select', '-print-path'],
                           stdout=subprocess.PIPE,
                           stderr=subprocess.STDOUT)
    out, err = job.communicate()
    if job.returncode != 0:
      print(out, file=sys.stderr)
      print(err, file=sys.stderr)
      raise Exception('Error %d running xcode-select' % job.returncode)
    dev_dir = out.rstrip()
  sdk_dir = os.path.join(
      dev_dir, 'Platforms/MacOSX.platform/Developer/SDKs')

  if not os.path.isdir(sdk_dir):
    raise SdkError('Install Xcode, launch it, accept the license ' +
      'agreement, and run `sudo xcode-select -s /path/to/Xcode.app` ' +
      'to continue.')
  sdks = [re.findall('^MacOSX(10\.\d+)\.sdk$', s) for s in os.listdir(sdk_dir)]
  sdks = [s[0] for s in sdks if s]  # [['10.5'], ['10.6']] => ['10.5', '10.6']
  sdks = [s for s in sdks  # ['10.5', '10.6'] => ['10.6']
          if parse_version(s) >= parse_version(min_sdk_version)]
  if not sdks:
    raise Exception('No %s+ SDK found' % min_sdk_version)
  best_sdk = sorted(sdks, key=parse_version)[0]

  if options.verify and best_sdk != min_sdk_version and not options.sdk_path:
    print('', file=sys.stderr)
    print('                                           vvvvvvv', file=sys.stderr)
    print('', file=sys.stderr)
    print('This build requires the %s SDK, but it was not found on your system.' \
        % min_sdk_version, file=sys.stderr)
    print(
        'Either install it, or explicitly set mac_sdk in your GYP_DEFINES.',
        file=sys.stderr)
    print('', file=sys.stderr)
    print('                                           ^^^^^^^', file=sys.stderr)
    print('', file=sys.stderr)
    sys.exit(1)

  if options.print_sdk_path:
    sdk_name = 'MacOSX' + best_sdk + '.sdk'
    print(os.path.join(sdk_dir, sdk_name))

  if options.print_bin_path:
    bin_path = 'Toolchains/XcodeDefault.xctoolchain/usr/bin/'
    print(os.path.join(dev_dir, bin_path))

  return best_sdk


if __name__ == '__main__':
  if sys.platform != 'darwin':
    raise Exception("This script only runs on Mac")
  print(main())
  sys.exit(0)
