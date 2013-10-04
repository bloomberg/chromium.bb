#!/usr/bin/python
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Binds or unbinds a specified USB video device.

To use, run: sudo ./camera_ctl.py device-identifier bind/unbind

The USB identifier looks like 2-1.4.3:1.0. Available devices are in:
/sys/bus/usb/drivers/uvcvideo/.
"""

import sys


def main():
  """Binds or unbinds the camera device."""

  device_id = sys.argv[1]
  command = sys.argv[2]

  if command != 'bind' and command != 'unbind':
    print 'Incorrect command.'
    sys.exit(-1)

  with open('/sys/bus/usb/drivers/uvcvideo/%s' % command, 'w') as driver:
    driver.write(device_id)
    driver.close()

if __name__ == '__main__':
  main()

