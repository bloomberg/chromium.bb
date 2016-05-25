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
  command = command + ' ' + ' '.join(extra_options)
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
  target_dir = out_dir
  return build(build_dir_sim, build_target) or \
         build(build_dir_dev, build_target) or \
         copy_build_dir(target_dir, build_dir_dev + "/cronet") or \
         lipo_libraries(target_dir, [build_dir_sim, build_dir_dev], \
                        "libcronet_" + build_config + ".a", \
                        "cronet/libcronet_standalone.a")


def package_ios_framework(out_dir='out/Framework', extra_options=''):
  print 'Building Cronet Dynamic Framework...'

  # Use Ninja to build all possible combinations.
  build_dirs = ['Debug-iphonesimulator',
                'Debug-iphoneos',
                'Release-iphonesimulator',
                'Release-iphoneos']
  for build_dir in build_dirs:
    print 'Building ' + build_dir
    build_result = run('ninja -C out/' + build_dir + ' cronet_framework',
                       extra_options)
    if build_result != 0:
      return build_result

  # Package all builds in the output directory
  os.makedirs(out_dir)
  for build_dir in build_dirs:
    shutil.copytree(os.path.join('out', build_dir, 'Cronet.framework'),
                    os.path.join(out_dir, build_dir, 'Cronet.framework'))
    if 'Release' in build_dir:
      shutil.copytree(os.path.join('out', build_dir, 'Cronet.framework.dSYM'),
                      os.path.join(out_dir, build_dir, 'Cronet.framework.dSYM'))
  # Copy the version file
  shutil.copy2('chrome/VERSION', out_dir)
  # Copy the headers
  shutil.copytree(os.path.join(out_dir, build_dirs[0],
                               'Cronet.framework', 'Headers'),
                  os.path.join(out_dir, 'Headers'))


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('out_dir', nargs=1, help='path to output directory')
  parser.add_argument('-g', '--skip_gyp', action='store_true',
                      help='skip gyp')
  parser.add_argument('-d', '--debug', action='store_true',
                      help='use release configuration')
  parser.add_argument('-r', '--release', action='store_true',
                      help='use release configuration')
  parser.add_argument('--framework', action='store_true',
                      help='build Cronet dynamic framework')

  options, extra_options_list = parser.parse_known_args()
  print options
  print extra_options_list

  out_dir = options.out_dir[0]

  # Make sure that the output directory does not exist
  if os.path.exists(out_dir):
    print >>sys.stderr, 'The output directory already exists: ' + out_dir
    return 1

  gyp_defines = 'GYP_DEFINES="OS=ios enable_websockets=0 '+ \
      'disable_file_support=1 disable_ftp_support=1 '+ \
      'enable_errorprone=1 use_platform_icu_alternatives=1 ' + \
      'disable_brotli_filter=1 chromium_ios_signing=0 ' + \
      'target_subarch=both"'
  if not options.skip_gyp:
    run (gyp_defines + ' gclient runhooks')

  if options.framework:
    return package_ios_framework(out_dir, extra_options_list)

  return package_ios(out_dir, "out/Release", "opt") or \
         package_ios(out_dir, "out/Debug", "dbg")


if __name__ == '__main__':
  sys.exit(main())
