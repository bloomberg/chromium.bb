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


def build(out_dir):
  return run ('ninja -C ' + out_dir + ' cronet_test_instrumentation_apk')


def install(release_arg):
  return run ('build/android/adb_install_apk.py ' + release_arg + \
              ' --apk=CronetTest.apk')


def test(release_arg, extra_options):
  return run ('build/android/test_runner.py instrumentation '+ \
              release_arg + ' --test-apk=CronetTestInstrumentation ' + \
              extra_options)


def debug(extra_options):
  return run ('build/android/adb_gdb --start ' + \
              '--activity=.CronetTestActivity ' + \
              '--program-name=CronetTest ' + \
              '--package-name=org.chromium.net ' + \
              ' '.join(extra_options))


def stack(out_dir):
  return run ('adb logcat -d | third_party/android_tools/ndk/ndk-stack ' + \
              '-sym ' + out_dir + '/lib')


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('command',
                      choices=['gyp',
                               'sync',
                               'build',
                               'install',
                               'proguard',
                               'test',
                               'build-test',
                               'stack',
                               'debug',
                               'build-debug'])
  parser.add_argument('-r', '--release', action='store_true',
                      help='use release configuration')

  options, extra_options_list = parser.parse_known_args()
  print options
  print extra_options_list
  gyp_defines = 'GYP_DEFINES="OS=android enable_websockets=0 '+ \
      'disable_file_support=1 disable_ftp_support=1 '+ \
      'use_icu_alternatives_on_android=1" '
  out_dir = 'out/Debug'
  release_arg = ''
  extra_options = ' '.join(extra_options_list)
  if options.release:
    out_dir = 'out/Release'
    release_arg = ' --release'

  if (options.command=='gyp'):
    return run (gyp_defines + ' gclient runhooks')
  if (options.command=='sync'):
    return run ('git pull --rebase && ' + gyp_defines + ' gclient sync')
  if (options.command=='build'):
    return build(out_dir)
  if (options.command=='install'):
    return install(release_arg)
  if (options.command=='proguard'):
    return run ('ninja -C ' + out_dir + ' cronet_sample_proguard_apk')
  if (options.command=='test'):
    return install(release_arg) or test(release_arg, extra_options)
  if (options.command=='build-test'):
    return build(out_dir) or install(release_arg) or \
        test(release_arg, extra_options)
  if (options.command=='stack'):
    return stack(out_dir)
  if (options.command=='debug'):
    return install(release_arg) or debug(extra_options)
  if (options.command=='build-debug'):
    return build(out_dir) or install(release_arg) or debug(extra_options)

  parser.print_help()
  return 1


if __name__ == '__main__':
  sys.exit(main())
