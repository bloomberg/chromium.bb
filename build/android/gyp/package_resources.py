#!/usr/bin/env python
#
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=C0301
"""Package resources into an apk.

See https://android.googlesource.com/platform/tools/base/+/master/legacy/ant-tasks/src/main/java/com/android/ant/AaptExecTask.java
and
https://android.googlesource.com/platform/sdk/+/master/files/ant/build.xml
"""
# pylint: enable=C0301

import optparse
import os

from util import build_utils

def ParseArgs():
  """Parses command line options.

  Returns:
    An options object as from optparse.OptionsParser.parse_args()
  """
  parser = optparse.OptionParser()
  parser.add_option('--android-sdk', help='path to the Android SDK folder')
  parser.add_option('--android-sdk-tools',
                    help='path to the Android SDK build tools folder')

  parser.add_option('--configuration-name',
                    help='Gyp\'s configuration name (Debug or Release).')

  parser.add_option('--android-manifest', help='AndroidManifest.xml path')
  parser.add_option('--version-code', help='Version code for apk.')
  parser.add_option('--version-name', help='Version name for apk.')
  parser.add_option('--resource-dirs',
                    help='directories containing resources to be packaged')
  parser.add_option('--asset-dir',
                    help='directories containing assets to be packaged')

  parser.add_option('--apk-path',
                    help='Path to output (partial) apk.')

  (options, args) = parser.parse_args()

  if args:
    parser.error('No positional arguments should be given.')

  # Check that required options have been provided.
  required_options = ('android_sdk', 'android_sdk_tools', 'configuration_name',
                      'android_manifest', 'version_code', 'version_name',
                      'asset_dir', 'apk_path')

  build_utils.CheckOptions(options, parser, required=required_options)

  return options


def main():
  options = ParseArgs()
  android_jar = os.path.join(options.android_sdk, 'android.jar')
  aapt = os.path.join(options.android_sdk_tools, 'aapt')

  package_command = [aapt,
                     'package',
                     '--version-code', options.version_code,
                     '--version-name', options.version_name,
                     '-M', options.android_manifest,
                     '--no-crunch',
                     '-f',
                     '--auto-add-overlay',

                     '-I', android_jar,
                     '-F', options.apk_path,
                     ]

  if os.path.exists(options.asset_dir):
    package_command += ['-A', options.asset_dir]

  for p in build_utils.ParseGypList(options.resource_dirs):
    package_command += ['-S', p]

  if 'Debug' in options.configuration_name:
    package_command += ['--debug-mode']

  build_utils.CheckOutput(
      package_command, print_stdout=False, print_stderr=False)


if __name__ == '__main__':
  main()
