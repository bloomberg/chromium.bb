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


def install(out_dir, release_arg):
  cmd = 'BUILDTYPE={0} build/android/adb_install_apk.py {1} --apk={2}'
  build_dir = out_dir.split('/', 1)[1] # the 'Foo' part of 'out/Foo'
  return run(cmd.format(build_dir, release_arg, 'CronetTest.apk')) or \
    run(cmd.format(build_dir, release_arg, 'ChromiumNetTestSupport.apk'))


def test(out_dir, extra_options):
  return run(out_dir + '/bin/run_cronet_test_instrumentation_apk ' + \
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
                      choices=['gn',
                               'sync',
                               'build',
                               'install',
                               'proguard',
                               'test',
                               'build-test',
                               'stack',
                               'debug',
                               'build-debug'])
  parser.add_argument('-g', '--gn', action='store_true',
                      help='use gn output directory suffix')
  parser.add_argument('-d', '--out_dir', action='store',
                      help='name of the build directory')
  parser.add_argument('-i', '--iphoneos', action='store_true',
                      help='build for physical iphone')
  parser.add_argument('-r', '--release', action='store_true',
                      help='use release configuration')
  parser.add_argument('-a', '--asan', action='store_true',
                      help='use address sanitizer')

  options, extra_options_list = parser.parse_known_args()
  print options
  print extra_options_list

  is_os = (sys.platform == 'darwin')
  if is_os:
    target_os = 'ios'
    test_target = 'cronet_test'
    gn_args = 'is_cronet_build=true is_component_build=false '
    gn_extra = '--ide=xcode'
    if options.iphoneos:
      gn_args += ' target_cpu="arm64" '
      out_dir_suffix = '-iphoneos'
    else:
      gn_args += ' target_cpu="x64" '
      out_dir_suffix = '-iphonesimulator'
      if options.asan:
        gn_args += ' is_asan=true use_xcode_clang=true '
        out_dir_suffix += '-asan'
  else:
    target_os = 'android'
    test_target = 'cronet_test_instrumentation_apk'
    gn_args = 'use_errorprone_java_compiler=true arm_use_neon=false '
    gn_extra = ''
    out_dir_suffix = ''

  gn_args += 'target_os="' + target_os + '" enable_websockets=false '+ \
      'disable_file_support=true disable_ftp_support=true '+ \
      'use_platform_icu_alternatives=true '+ \
      'disable_brotli_filter=true is_component_build=false ' + \
      'ignore_elf32_limitations=true use_partition_alloc=false'

  extra_options = ' '.join(extra_options_list)
  if options.gn:
    out_dir_suffix += "-gn"

  if options.release:
    out_dir = 'out/Release' + out_dir_suffix
    release_arg = ' --release'
    gn_args += ' is_debug=false '
  else:
    out_dir = 'out/Debug' + out_dir_suffix
    release_arg = ''

  if options.out_dir:
    out_dir = options.out_dir

  if (options.command=='gn'):
    return run ('gn gen %s --args=\'%s\' %s' % (out_dir, gn_args, gn_extra))
  if (options.command=='sync'):
    return run ('git pull --rebase && gclient sync')
  if (options.command=='build'):
    return build(out_dir, test_target, extra_options)
  if (not is_os):
    if (options.command=='install'):
      return install(out_dir, release_arg)
    if (options.command=='proguard'):
      return run ('ninja -C ' + out_dir + ' cronet_sample_proguard_apk')
    if (options.command=='test'):
      return install(out_dir, release_arg) or test(out_dir, extra_options)
    if (options.command=='build-test'):
      return build(out_dir, test_target) or install(out_dir, release_arg) or \
          test(out_dir, extra_options)
    if (options.command=='stack'):
      return stack(out_dir)
    if (options.command=='debug'):
      return install(out_dir, release_arg) or debug(extra_options)
    if (options.command=='build-debug'):
      return build(out_dir, test_target) or install(out_dir, release_arg) or \
          debug(extra_options)
  else:
    if (options.command=='test'):
      return test_ios(out_dir, extra_options)
    if (options.command=='build-test'):
      return build(out_dir, test_target) or test_ios(out_dir, extra_options)

  parser.print_help()
  return 1


if __name__ == '__main__':
  sys.exit(main())
