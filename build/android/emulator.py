#!/usr/bin/env python
#
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script to launch Android emulators.

Assumes system environment ANDROID_NDK_ROOT has been set.
"""

import optparse
import sys

from pylib.utils import emulator


def main(argv):
  option_parser = optparse.OptionParser()
  option_parser.add_option('-n', '--num', dest='emulator_count',
                           help='Number of emulators to launch.',
                           type='int',
                           default=1)
  option_parser.add_option('-w', '--wait', dest='wait_for_boot',
                           action='store_true',
                           help='If set, wait for the emulators to boot.')
  options, args = option_parser.parse_args(argv)
  emulator.LaunchEmulators(options.emulator_count, options.wait_for_boot)


if __name__ == '__main__':
  main(sys.argv)
