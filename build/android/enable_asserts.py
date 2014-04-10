#!/usr/bin/env python
#
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Enables dalvik vm asserts in the android device."""

from pylib.device import device_utils
import optparse
import sys


def main(argv):
  option_parser = optparse.OptionParser()
  option_parser.add_option('--enable_asserts', dest='set_asserts',
      action='store_true', default=None,
      help='Sets the dalvik.vm.enableassertions property to "all"')
  option_parser.add_option('--disable_asserts', dest='set_asserts',
      action='store_false', default=None,
      help='Removes the dalvik.vm.enableassertions property')
  options, _ = option_parser.parse_args(argv)

  device = device_utils.DeviceUtils()
  if options.set_asserts != None:
    if device.old_interface.SetJavaAssertsEnabled(options.set_asserts):
      device.old_interface.Reboot(full_reboot=False)


if __name__ == '__main__':
  main(sys.argv)
