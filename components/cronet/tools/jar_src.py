#!/usr/bin/env python
#
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse
import os
import sys
import tempfile

REPOSITORY_ROOT = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..', '..'))

sys.path.append(os.path.join(REPOSITORY_ROOT, 'build/android/gyp/util'))
import build_utils


def JarSources(src_dir, jar_path):
  # The paths of the files in the jar will be the same as they are passed in to
  # the command. Because of this, the command should be run in
  # options.src_dir so the .java file paths in the jar are correct.
  jar_cwd = src_dir
  jar_path = os.path.abspath(jar_path)
  if os.path.exists(jar_path):
    jar_cmd = ['jar', 'uf', jar_path, '.']
  else:
    jar_cmd = ['jar', 'cf', jar_path, '.']

  build_utils.CheckOutput(jar_cmd, cwd=jar_cwd)

# Uncompress source jars so that they can be combined with other sources
def UnzipSourceJar(jar, unzipped_jar_path):
  if os.path.exists(jar):
    jar_cmd = ['jar', 'xf', os.path.abspath(jar)]
    build_utils.CheckOutput(jar_cmd, cwd=unzipped_jar_path)
  else:
    raise Exception('Jar file does not exist %s' % jar)


def main():
  parser = optparse.OptionParser()
  build_utils.AddDepfileOption(parser)
  parser.add_option('--src-dir', action="append",
      help='Directory containing .java files.')
  parser.add_option('--src-jars', action="append",
      help='A list of source jars to include in addition to source files.')
  parser.add_option('--jar-path', help='Jar output path.')
  parser.add_option('--stamp', help='Path to touch on success.')

  options, _ = parser.parse_args()

  # A temporary directory to put the output of jar files.
  unzipped_jar_path = None
  if options.src_jars:
    unzipped_jar_path = tempfile.mkdtemp(dir=os.path.dirname(options.jar_path))
    jar_list = []
    for gn_list in options.src_jars:
      jar_list.extend(build_utils.ParseGnList(gn_list))

    for jar in jar_list:
      UnzipSourceJar(jar, unzipped_jar_path)

  src_dirs = []
  for src_dir in options.src_dir:
    src_dirs.extend(build_utils.ParseGnList(src_dir))
  if unzipped_jar_path:
    src_dirs += [unzipped_jar_path]

  for src_dir in src_dirs:
    JarSources(src_dir, options.jar_path)

  if options.depfile:
    input_paths = []
    for src_dir in src_dirs:
      for root, _, filenames in os.walk(src_dir):
        input_paths.extend(os.path.join(root, f) for f in filenames)
    build_utils.WriteDepfile(options.depfile,
                             input_paths + build_utils.GetPythonDependencies())
  # Clean up temporary output directory.
  if unzipped_jar_path:
    build_utils.DeleteDirectory(unzipped_jar_path)

  if options.stamp:
    build_utils.Touch(options.stamp)

if __name__ == '__main__':
  sys.exit(main())

