#!/usr/bin/python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
cr_cronet.py - cr - like helper tool for cronet developers
"""

import argparse
import os
import sys

def run(command):
  print command
  return os.system(command)

def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('command',
                      choices=['init',
                               'sync',
                               'build',
                               'install',
                               'test',
                               'debug'])

  options = parser.parse_args()
  print options
  print options.command
  gyp_defines = 'GYP_DEFINES="OS=android enable_websockets=0 '+ \
      'disable_file_support=1 disable_ftp_support=1 '+ \
      'use_icu_alternatives_on_android=1" '

  if (options.command=='init'):
    return run (gyp_defines + ' gclient runhooks')
  if (options.command=='sync'):
    return run ('git pull --rebase && ' + gyp_defines + ' gclient sync')
  if (options.command=='build'):
    return run ('ninja -C out/Debug cronet_sample_test_apk')
  if (options.command=='install'):
    return run ('build/android/adb_install_apk.py --apk=CronetSample.apk')
  if (options.command=='test'):
    return run ('build/android/test_runner.py instrumentation '+ \
                '--test-apk=CronetSampleTest')

  parser.print_help()
  return 1


if __name__ == '__main__':
  sys.exit(main())
