#!/usr/bin/env python

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
from optparse import OptionParser
from com.android.monkeyrunner import MonkeyRunner, MonkeyDevice

def main(argv):
  # Parse options.
  parser = OptionParser()
  parser.add_option("--serial", dest="serial",
                    help="connect to device with specified SERIAL",
                    metavar="SERIAL")
  parser.add_option("--file", dest="filename",
                    help="write screenshot to FILE",
                    metavar="FILE", default="Screenshot.png")
  parser.add_option("--timeout", dest="timeout",
                    help="TIMEOUT in seconds for connecting to a device",
                    metavar="TIMEOUT", default=120)
  (options, args) = parser.parse_args(argv)

  # Connect to the current device, returning a MonkeyDevice object.
  # Monkeyrunner fails with a NullPointerException if options.serial is None.
  if options.serial:
    device = MonkeyRunner.waitForConnection(options.timeout, options.serial)
  else:
    device = MonkeyRunner.waitForConnection(options.timeout)

  if not device:
    return 1

  # Grab screenshot and write to disk.
  result = device.takeSnapshot()
  result.writeToFile(options.filename, 'png')
  return 0

if __name__ == '__main__':
  sys.exit(main(sys.argv))
