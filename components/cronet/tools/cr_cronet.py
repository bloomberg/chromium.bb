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

def run(command, extra_options=''):
  command = command + ' ' + extra_options
  print command
  return os.system(command)


def build(out_dir, test_target, extra_options=''):
  return run('ninja -C ' + out_dir + ' ' + test_target,
             extra_options)


def install(release_arg):
  return run('build/android/adb_install_apk.py ' + release_arg + \
             ' --apk=CronetTest.apk') or \
             run('build/android/adb_install_apk.py ' + release_arg + \
             ' --apk=ChromiumNetTestSupport.apk')


def test(release_arg, extra_options):
  return run('build/android/test_runner.py instrumentation '+ \
             release_arg + ' --test-apk=CronetTestInstrumentation' + \
             ' --fast-local-dev',
             extra_options)

def test_ios(out_dir, extra_options):
  return run(out_dir + '/iossim ' + out_dir + '/cronet_test.app', extra_options)

def debug(extra_options):
  return run('build/android/adb_gdb --start ' + \
             '--activity=.CronetTestActivity ' + \
             '--program-name=CronetTest ' + \
             '--package-name=org.chromium.net',
             extra_options)


def stack(out_dir):
  return run('adb logcat -d | CHROMIUM_OUTPUT_DIR=' + out_dir +
          ' third_party/android_platform/development/scripts/stack')

def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('command',
                      choices=['gyp',
                               'gn',
                               'sync',
                               'build',
                               'install',
                               'proguard',
                               'test',
                               'build-test',
                               'stack',
                               'debug',
                               'build-debug'])
  parser.add_argument('-i', '--iphoneos', action='store_true',
                      help='build for physical iphone')
  parser.add_argument('-r', '--release', action='store_true',
                      help='use release configuration')

  options, extra_options_list = parser.parse_known_args()
  print options
  print extra_options_list

  is_os = (sys.platform == 'darwin')
  if is_os:
    target_os = 'ios'
    test_target = 'cronet_test'
    out_dir_suffix = '-iphonesimulator'
    if options.iphoneos:
      out_dir_suffix = '-iphoneos'
  else:
    target_os = 'android'
    test_target = 'cronet_test_instrumentation_apk'
    out_dir_suffix = ''

  gyp_defines = 'GYP_DEFINES="OS=' + target_os + ' enable_websockets=0 '+ \
      'disable_file_support=1 disable_ftp_support=1 '+ \
      'enable_errorprone=1 use_platform_icu_alternatives=1 ' + \
      'disable_brotli_filter=1"'
  gn_args = 'target_os="' + target_os + '" enable_websockets=false '+ \
      'disable_file_support=true disable_ftp_support=true '+ \
      'use_errorprone_java_compiler=true use_platform_icu_alternatives=true '+ \
      'disable_brotli_filter=true'

  out_dir = 'out/Debug' + out_dir_suffix
  release_arg = ''
  extra_options = ' '.join(extra_options_list)
  if options.release:
    out_dir = 'out/Release' + out_dir_suffix
    release_arg = ' --release'
    gn_args += ' is_debug=false '

  if (options.command=='gyp'):
    return run (gyp_defines + ' gclient runhooks')
  if (options.command=='gn'):
    return run ('gn gen ' + out_dir + ' --args=\'' + gn_args + '\'')
  if (options.command=='sync'):
    return run ('git pull --rebase && ' + gyp_defines + ' gclient sync')
  if (options.command=='build'):
    return build(out_dir, test_target, extra_options)
  if (not is_os):
    if (options.command=='install'):
      return install(release_arg)
    if (options.command=='proguard'):
      return run ('ninja -C ' + out_dir + ' cronet_sample_proguard_apk')
    if (options.command=='test'):
      return install(release_arg) or test(release_arg, extra_options)
    if (options.command=='build-test'):
      return build(out_dir, test_target) or install(release_arg) or \
          test(release_arg, extra_options)
    if (options.command=='stack'):
      return stack(out_dir)
    if (options.command=='debug'):
      return install(release_arg) or debug(extra_options)
    if (options.command=='build-debug'):
      return build(out_dir) or install(release_arg) or debug(extra_options)
  else:
    if (options.command=='test'):
      return test_ios(out_dir, extra_options)
    if (options.command=='build-test'):
      return build(out_dir, test_target) or test_ios(out_dir, extra_options)

  parser.print_help()
  return 1


if __name__ == '__main__':
  sys.exit(main())
