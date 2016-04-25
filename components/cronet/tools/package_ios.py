#!/usr/bin/python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
package_ios.py - Build and Package Release and Rebug fat libraries for iOS.
"""

import argparse
import os
import shutil
import sys

def run(command, extra_options=''):
  command = command + ' ' + extra_options
  print command
  return os.system(command)


def build(out_dir, test_target, extra_options=''):
  return run('ninja -C ' + out_dir + ' ' + test_target,
             extra_options)


def lipo_libraries(out_dir, input_dirs, out_lib, input_lib):
  lipo = "lipo -create "
  for input_dir in input_dirs:
    lipo += input_dir + "/" + input_lib + " "
  lipo += '-output ' + out_dir + "/" + out_lib
  return run(lipo)


def copy_build_dir(target_dir, build_dir):
  try:
    shutil.copytree(build_dir, target_dir, ignore=shutil.ignore_patterns('*.a'))
  except OSError as e:
    print('Directory not copied. Error: %s' % e)
  return 0

def package_ios(out_dir, build_dir, build_config):
  build_dir_sim = build_dir
  build_dir_dev = build_dir +'-iphoneos'
  build_target = 'cronet_package'
  target_dir = out_dir + "/Cronet"
  return build(build_dir_sim, build_target) or \
         build(build_dir_dev, build_target) or \
         copy_build_dir(target_dir, build_dir_dev + "/cronet") or \
         lipo_libraries(target_dir, [build_dir_sim, build_dir_dev], \
                        "libcronet_" + build_config + ".a", \
                        "cronet/libcronet_standalone.a")


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('out_dir', nargs=1, help='path to output directory')
  parser.add_argument('-g', '--skip_gyp', action='store_true',
                      help='skip gyp')
  parser.add_argument('-d', '--debug', action='store_true',
                      help='use release configuration')
  parser.add_argument('-r', '--release', action='store_true',
                      help='use release configuration')

  options, extra_options_list = parser.parse_known_args()
  print options
  print extra_options_list

  gyp_defines = 'GYP_DEFINES="OS=ios enable_websockets=0 '+ \
      'disable_file_support=1 disable_ftp_support=1 '+ \
      'enable_errorprone=1 use_platform_icu_alternatives=1 ' + \
      'disable_brotli_filter=1 target_subarch=both"'
  if not options.skip_gyp:
    run (gyp_defines + ' gclient runhooks')

  return package_ios(options.out_dir[0], "out/Release", "opt") or \
         package_ios(options.out_dir[0], "out/Debug", "dbg")


if __name__ == '__main__':
  sys.exit(main())
