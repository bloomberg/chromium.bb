#!/usr/bin/env python
#
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import sys
import tempfile

from util import build_utils

sys.path.append(os.path.abspath(os.path.join(
    os.path.dirname(__file__), os.pardir)))
from pylib import constants


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--android-sdk-tools', required=True,
                      help='Android sdk build tools directory.')
  parser.add_argument('--main-dex-rules-path', action='append', default=[],
                      dest='main_dex_rules_paths',
                      help='A file containing a list of proguard rules to use '
                           'in determining the class to include in the '
                           'main dex.')
  parser.add_argument('--main-dex-list-path', required=True,
                      help='The main dex list file to generate.')
  parser.add_argument('paths', nargs='+',
                      help='JARs for which a main dex list should be '
                           'generated.')

  args = parser.parse_args()

  with open(args.main_dex_list_path, 'w') as main_dex_list_file:

    shrinked_android_jar = os.path.abspath(
        os.path.join(args.android_sdk_tools, 'lib', 'shrinkedAndroid.jar'))
    dx_jar = os.path.abspath(
        os.path.join(args.android_sdk_tools, 'lib', 'dx.jar'))
    paths_arg = ':'.join(args.paths)
    rules_file = os.path.abspath(
        os.path.join(args.android_sdk_tools, 'mainDexClasses.rules'))

    with tempfile.NamedTemporaryFile(suffix='.jar') as temp_jar:
      proguard_cmd = [
        constants.PROGUARD_SCRIPT_PATH,
        '-forceprocessing',
        '-dontwarn', '-dontoptimize', '-dontobfuscate', '-dontpreverify',
        '-injars', paths_arg,
        '-outjars', temp_jar.name,
        '-libraryjars', shrinked_android_jar,
        '-include', rules_file,
      ]
      for m in args.main_dex_rules_paths:
        proguard_cmd.extend(['-include', m])

      main_dex_list = ''
      try:
        build_utils.CheckOutput(proguard_cmd)

        java_cmd = [
          'java', '-cp', dx_jar,
          'com.android.multidex.MainDexListBuilder',
          temp_jar.name, paths_arg
        ]
        main_dex_list = build_utils.CheckOutput(java_cmd)
      except build_utils.CalledProcessError as e:
        if 'output jar is empty' in e.output:
          pass
        elif "input doesn't contain any classes" in e.output:
          pass
        else:
          raise

      main_dex_list_file.write(main_dex_list)

  return 0


if __name__ == '__main__':
  sys.exit(main())

