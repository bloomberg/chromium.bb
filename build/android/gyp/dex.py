#!/usr/bin/env python
#
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import optparse
import os
import sys
import tempfile
import zipfile

from util import build_utils
from util import md5_check


def DoMultiDex(options, paths):
  main_dex_list = []
  main_dex_list_files = build_utils.ParseGypList(options.main_dex_list_paths)
  for m in main_dex_list_files:
    with open(m) as main_dex_list_file:
      main_dex_list.extend(l for l in main_dex_list_file if l)

  with tempfile.NamedTemporaryFile(suffix='.txt') as combined_main_dex_list:
    combined_main_dex_list.write('\n'.join(main_dex_list))
    combined_main_dex_list.flush()

    dex_args = [
      '--multi-dex',
      '--minimal-main-dex',
      '--main-dex-list=%s' % combined_main_dex_list.name
    ]

    DoDex(options, paths, dex_args=dex_args)

  if options.dex_path.endswith('.zip'):
    iz = zipfile.ZipFile(options.dex_path, 'r')
    tmp_dex_path = '%s.tmp.zip' % options.dex_path
    oz = zipfile.ZipFile(tmp_dex_path, 'w', zipfile.ZIP_DEFLATED)
    for i in iz.namelist():
      if i.endswith('.dex'):
        oz.writestr(i, iz.read(i))
    os.remove(options.dex_path)
    os.rename(tmp_dex_path, options.dex_path)


def DoDex(options, paths, dex_args=None):
  dx_binary = os.path.join(options.android_sdk_tools, 'dx')
  # See http://crbug.com/272064 for context on --force-jumbo.
  # See https://github.com/android/platform_dalvik/commit/dd140a22d for
  # --num-threads.
  dex_cmd = [dx_binary, '--num-threads=8', '--dex', '--force-jumbo',
             '--output', options.dex_path]
  if options.no_locals != '0':
    dex_cmd.append('--no-locals')

  if dex_args:
    dex_cmd += dex_args

  dex_cmd += paths

  record_path = '%s.md5.stamp' % options.dex_path
  md5_check.CallAndRecordIfStale(
      lambda: build_utils.CheckOutput(dex_cmd, print_stderr=False),
      record_path=record_path,
      input_paths=paths,
      input_strings=dex_cmd,
      force=not os.path.exists(options.dex_path))
  build_utils.WriteJson(
      [os.path.relpath(p, options.output_directory) for p in paths],
      options.dex_path + '.inputs')


def main():
  args = build_utils.ExpandFileArgs(sys.argv[1:])

  parser = optparse.OptionParser()
  build_utils.AddDepfileOption(parser)

  parser.add_option('--android-sdk-tools',
                    help='Android sdk build tools directory.')
  parser.add_option('--output-directory',
                    default=os.getcwd(),
                    help='Path to the output build directory.')
  parser.add_option('--dex-path', help='Dex output path.')
  parser.add_option('--configuration-name',
                    help='The build CONFIGURATION_NAME.')
  parser.add_option('--proguard-enabled',
                    help='"true" if proguard is enabled.')
  parser.add_option('--proguard-enabled-input-path',
                    help=('Path to dex in Release mode when proguard '
                          'is enabled.'))
  parser.add_option('--no-locals',
                    help='Exclude locals list from the dex file.')
  parser.add_option('--multi-dex', default=False, action='store_true',
                    help='Create multiple dex files.')
  parser.add_option('--inputs', help='A list of additional input paths.')
  parser.add_option('--excluded-paths',
                    help='A list of paths to exclude from the dex file.')
  parser.add_option('--main-dex-list-paths',
                    help='A list of paths containing a list of the classes to '
                         'include in the main dex.')

  options, paths = parser.parse_args(args)

  required_options = ('android_sdk_tools',)
  build_utils.CheckOptions(options, parser, required=required_options)

  if (options.proguard_enabled == 'true'
      and options.configuration_name == 'Release'):
    paths = [options.proguard_enabled_input_path]

  if options.inputs:
    paths += build_utils.ParseGypList(options.inputs)

  if options.excluded_paths:
    # Excluded paths are relative to the output directory.
    exclude_paths = build_utils.ParseGypList(options.excluded_paths)
    paths = [p for p in paths if not
             os.path.relpath(p, options.output_directory) in exclude_paths]

  if options.multi_dex and options.main_dex_list_paths:
    DoMultiDex(options, paths)
  else:
    if options.multi_dex:
      logging.warning('--multi-dex is unused without --main-dex-list-paths')
    elif options.main_dex_list_paths:
      logging.warning('--main-dex-list-paths is unused without --multi-dex')
    DoDex(options, paths)

  if options.depfile:
    build_utils.WriteDepfile(
        options.depfile,
        paths + build_utils.GetPythonDependencies())



if __name__ == '__main__':
  sys.exit(main())
