#!/usr/bin/env python
#
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates the obfuscated jar and test jar for an apk.

If proguard is not enabled or 'Release' is not in the configuration name,
obfuscation will be a no-op.
"""

import fnmatch
import optparse
import os
import sys
import zipfile

from util import build_utils

def ParseArgs(argv):
  parser = optparse.OptionParser()
  parser.add_option('--android-sdk', help='path to the Android SDK folder')
  parser.add_option('--android-sdk-tools',
                    help='path to the Android SDK build tools folder')
  parser.add_option('--android-sdk-jar',
                    help='path to Android SDK\'s android.jar')
  parser.add_option('--proguard-jar-path',
                    help='Path to proguard.jar in the sdk')

  parser.add_option('--input-jars-paths',
                    help='Path to jars to include in obfuscated jar')

  parser.add_option('--proguard-config-files',
                    help='Paths to proguard config files')

  parser.add_option('--configuration-name',
                    help='Gyp configuration name (i.e. Debug, Release)')
  parser.add_option('--proguard-enabled', action='store_true',
                    help='Set if proguard is enabled for this target.')

  parser.add_option('--obfuscated-jar-path',
                    help='Output path for obfuscated jar.')

  parser.add_option('--testapp', action='store_true',
                    help='Set this if building an instrumentation test apk')
  parser.add_option('--test-jar-path',
                    help='Output path for jar containing all the test apk\'s '
                    'code.')

  parser.add_option('--stamp', help='File to touch on success')

  (options, args) = parser.parse_args(argv)

  if args:
    parser.error('No positional arguments should be given. ' + str(args))

  # Check that required options have been provided.
  required_options = (
      'android_sdk',
      'android_sdk_tools',
      'android_sdk_jar',
      'proguard_jar_path',
      'input_jars_paths',
      'configuration_name',
      'obfuscated_jar_path',
      )
  build_utils.CheckOptions(options, parser, required=required_options)

  return options, args


def main(argv):
  options, _ = ParseArgs(argv)

  library_classpath = [options.android_sdk_jar]
  javac_custom_classpath = build_utils.ParseGypList(options.input_jars_paths)

  dependency_class_filters = [
      '*R.class', '*R$*.class', '*Manifest.class', '*BuildConfig.class']

  def DependencyClassFilter(name):
    for name_filter in dependency_class_filters:
      if fnmatch.fnmatch(name, name_filter):
        return False
    return True

  if options.testapp:
    with zipfile.ZipFile(options.test_jar_path, 'w') as test_jar:
      for jar in build_utils.ParseGypList(options.input_jars_paths):
        with zipfile.ZipFile(jar, 'r') as jar_zip:
          for name in filter(DependencyClassFilter, jar_zip.namelist()):
            with jar_zip.open(name) as zip_entry:
              test_jar.writestr(name, zip_entry.read())

  if options.configuration_name == 'Release' and options.proguard_enabled:
    proguard_project_classpath = javac_custom_classpath

    proguard_cmd = [
        'java', '-jar', options.proguard_jar_path,
        '-forceprocessing',
        '-injars', ':'.join(proguard_project_classpath),
        '-libraryjars', ':'.join(library_classpath),
        '-outjars', options.obfuscated_jar_path,
        '-dump', options.obfuscated_jar_path + '.dump',
        '-printseeds', options.obfuscated_jar_path + '.seeds',
        '-printusage', options.obfuscated_jar_path + '.usage',
        '-printmapping', options.obfuscated_jar_path + '.mapping',
        ]

    for proguard_file in build_utils.ParseGypList(
        options.proguard_config_files):
      proguard_cmd += ['-include', proguard_file]

    build_utils.CheckOutput(proguard_cmd)
  else:
    output_files = [
        options.obfuscated_jar_path,
        options.obfuscated_jar_path + '.dump',
        options.obfuscated_jar_path + '.seeds',
        options.obfuscated_jar_path + '.usage',
        options.obfuscated_jar_path + '.mapping']
    for f in output_files:
      if os.path.exists(f):
        os.remove(f)
      build_utils.Touch(f)

  if options.stamp:
    build_utils.Touch(options.stamp)

if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
