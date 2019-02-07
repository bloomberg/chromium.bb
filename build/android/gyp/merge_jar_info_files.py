#!/usr/bin/env python

# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Merges .jar.info files into one for APKs."""

import argparse
import os
import sys
import zipfile

from util import build_utils
from util import jar_info_utils


def _FullJavaNameFromClassFilePath(path):
  # Input:  base/android/java/src/org/chromium/Foo.class
  # Output: base.android.java.src.org.chromium.Foo
  if not path.endswith('.class'):
    return ''
  path = os.path.splitext(path)[0]
  parts = []
  while path:
    # Use split to be platform independent.
    head, tail = os.path.split(path)
    path = head
    parts.append(tail)
  parts.reverse()  # Package comes first
  return '.'.join(parts)


def _MergeInfoFiles(output, inputs):
  """Merge several .jar.info files to generate an .apk.jar.info.

  Args:
    output: output file path.
    inputs: List of .info.jar or .jar files.
  """
  info_data = dict()
  for path in inputs:
    # android_java_prebuilt adds jar files in the src directory (relative to
    #     the output directory, usually ../../third_party/example.jar).
    # android_aar_prebuilt collects jar files in the aar file and uses the
    #     java_prebuilt rule to generate gen/example/classes.jar files.
    # We scan these prebuilt jars to parse each class path for the FQN. This
    #     allows us to later map these classes back to their respective src
    #     directories.
    # TODO(agrieve): This should probably also check that the mtime of the .info
    #     is newer than that of the .jar, or change prebuilts to always output
    #     .info files so that they always exist (and change the depfile to
    #     depend directly on them).
    if path.endswith('.info'):
      info_data.update(jar_info_utils.ParseJarInfoFile(path))
    else:
      with zipfile.ZipFile(path) as zip_info:
        for name in zip_info.namelist():
          fully_qualified_name = _FullJavaNameFromClassFilePath(name)
          if fully_qualified_name:
            info_data[fully_qualified_name] = '{}/{}'.format(path, name)

  with build_utils.AtomicOutput(output) as f:
    jar_info_utils.WriteJarInfoFile(f, info_data)


def _FindInputs(jar_paths):
  ret = []
  for jar_path in jar_paths:
    jar_info_path = jar_path + '.info'
    if os.path.exists(jar_info_path):
      ret.append(jar_info_path)
    else:
      ret.append(jar_path)
  return ret


def main(args):
  args = build_utils.ExpandFileArgs(args)
  parser = argparse.ArgumentParser(description=__doc__)
  build_utils.AddDepfileOption(parser)
  parser.add_argument('--output', required=True, help='Output .jar.info file')
  parser.add_argument(
      '--apk-jar-file', help='Path to main .jar file (for APKs).')
  parser.add_argument('--dep-jar-files', required=True,
                      help='GN-list of dependent .jar file paths')

  options = parser.parse_args(args)
  options.dep_jar_files = build_utils.ParseGnList(options.dep_jar_files)
  jar_files = list(options.dep_jar_files)
  if options.apk_jar_file:
    jar_files.append(options.apk_jar_file)
  inputs = _FindInputs(jar_files)

  def _OnStaleMd5():
    _MergeInfoFiles(options.output, inputs)

  # Don't bother re-running if no .info files have changed.
  build_utils.CallAndWriteDepfileIfStale(
      _OnStaleMd5,
      options,
      input_paths=inputs,
      output_paths=[options.output],
      depfile_deps=inputs,
      add_pydeps=False)


if __name__ == '__main__':
  main(sys.argv[1:])
