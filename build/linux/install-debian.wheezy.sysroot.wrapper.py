#!/usr/bin/env python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This wrapper script runs the Debian sysroot installation scripts, if they
# exist.
#
# The script is a no-op except for linux users who have the following in their
# GYP_DEFINES:
#
# * branding=Chrome
# * buildtype=Official
#
# and not:
#
# * chromeos=1

import os.path
import subprocess
import sys

def main():
  if sys.platform != 'linux2':
    return 0

  SRC_DIR = os.path.dirname(os.path.dirname(os.path.dirname(
      os.path.realpath(__file__))))
  SCRIPT_FILE = os.path.join(SRC_DIR,
                             'chrome',
                             'installer',
                             'linux',
                             'internal',
                             'sysroot_scripts',
                             'install-debian.wheezy.sysroot.py')
  if os.path.exists(SCRIPT_FILE):
    ret = subprocess.call([SCRIPT_FILE, '--linux-only', '--arch=amd64'])
    if ret != 0:
      return ret
    ret  = subprocess.call([SCRIPT_FILE, '--linux-only', '--arch=i386'])
    if ret != 0:
      return ret
  return 0

if __name__ == '__main__':
  sys.exit(main())
