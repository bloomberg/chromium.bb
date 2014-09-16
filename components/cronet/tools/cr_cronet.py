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
                      choices=['gyp',
                               'sync',
                               'build',
                               'install',
                               'proguard',
                               'test',
                               'debug'])
  parser.add_argument('-r', '--release', action='store_true',
                      help='use release configuration')

  options = parser.parse_args()
  print options
  gyp_defines = 'GYP_DEFINES="OS=android enable_websockets=0 '+ \
      'disable_file_support=1 disable_ftp_support=1 '+ \
      'use_icu_alternatives_on_android=1" '
  out_dir = 'out/Debug'
  release_arg = ''
  if options.release:
    out_dir = 'out/Release'
    release_arg = ' --release'

  if (options.command=='gyp'):
    return run (gyp_defines + ' gclient runhooks')
  if (options.command=='sync'):
    return run ('git pull --rebase && ' + gyp_defines + ' gclient sync')
  if (options.command=='build'):
    return run ('ninja -C ' + out_dir + ' cronet_test_instrumentation_apk')
  if (options.command=='install'):
    return run ('build/android/adb_install_apk.py ' + release_arg + \
                ' --apk=CronetTest.apk')
  if (options.command=='proguard'):
    return run ('ninja -C ' + out_dir + ' cronet_sample_proguard_apk')
  if (options.command=='test'):
    return run ('build/android/test_runner.py instrumentation '+ \
                release_arg + ' --test-apk=CronetTestInstrumentation')

  parser.print_help()
  return 1


if __name__ == '__main__':
  sys.exit(main())
