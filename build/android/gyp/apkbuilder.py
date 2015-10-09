#!/usr/bin/env python
#
# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Adds the code parts to a resource APK."""

import argparse
import os
import shutil
import sys
import zipfile

from util import build_utils


def _ParseArgs(args):
  parser = argparse.ArgumentParser()
  build_utils.AddDepfileOption(parser)
  parser.add_argument('--resource-apk',
                      help='An .ap_ file built using aapt',
                      required=True)
  parser.add_argument('--output-apk',
                      help='Path to the output file',
                      required=True)
  parser.add_argument('--dex-file',
                      help='Path to the classes.dex to use')
  # TODO(agrieve): Switch this to be a list of files rather than a directory.
  parser.add_argument('--native-libs-dir',
                      help='Directory containing native libraries to include',
                      default=[])
  parser.add_argument('--android-abi',
                      help='Android architecture to use for native libraries')
  parser.add_argument('--create-placeholder-lib',
                      action='store_true',
                      help='Whether to add a dummy library file')
  options = parser.parse_args(args)
  if not options.android_abi and (options.native_libs_dir or
                                  options.create_placeholder_lib):
    raise Exception('Must specify --android-abi with --native-libs-dir')
  return options


def _ListSubPaths(path):
  """Returns a list of full paths to all files in the given path."""
  return [os.path.join(path, name) for name in os.listdir(path)]


def main(args):
  args = build_utils.ExpandFileArgs(args)
  options = _ParseArgs(args)

  native_libs = []
  if options.native_libs_dir:
    native_libs = _ListSubPaths(options.native_libs_dir)

  input_paths = [options.resource_apk] + native_libs
  if options.dex_file:
    input_paths.append(options.dex_file)

  def on_stale_md5():
    tmp_apk = options.output_apk + '.tmp'
    try:
      # Use a temp file to avoid creating an output if anything goes wrong.
      shutil.copyfile(options.resource_apk, tmp_apk)

      # TODO(agrieve): It would be more efficient to combine this step
      # with finalize_apk(), which sometimes aligns and uncompresses the
      # native libraries.
      with zipfile.ZipFile(tmp_apk, 'a', zipfile.ZIP_DEFLATED) as apk:
        for path in native_libs:
          basename = os.path.basename(path)
          apk.write(path, 'lib/%s/%s' % (options.android_abi, basename))
        if options.create_placeholder_lib:
          # Make it non-empty so that its checksum is non-zero and is not
          # ignored by md5_check.
          apk.writestr('lib/%s/libplaceholder.so' % options.android_abi, ':-)')
        if options.dex_file:
          apk.write(options.dex_file, 'classes.dex')

      shutil.move(tmp_apk, options.output_apk)
    finally:
      if os.path.exists(tmp_apk):
        os.unlink(tmp_apk)

  build_utils.CallAndWriteDepfileIfStale(
      on_stale_md5,
      options,
      input_paths=input_paths,
      input_strings=[options.create_placeholder_lib, options.android_abi],
      output_paths=[options.output_apk])


if __name__ == '__main__':
  main(sys.argv[1:])
