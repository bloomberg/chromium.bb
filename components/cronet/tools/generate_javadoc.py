#!/usr/bin/env python
#
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import fnmatch
import optparse
import os
import sys

REPOSITORY_ROOT = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..', '..'))

sys.path.append(os.path.join(REPOSITORY_ROOT, 'build/android/gyp/util'))
import build_utils


def GenerateJavadoc(options):
  source_dir = options.source_dir
  output_dir = options.output_dir
  working_dir = options.working_dir

  build_utils.DeleteDirectory(output_dir)
  build_utils.MakeDirectory(output_dir)
  javadoc_cmd = ['ant', '-Dsource.dir=' + source_dir,
             '-Ddoc.dir=' + os.path.abspath(output_dir), 'doc']
  build_utils.CheckOutput(javadoc_cmd, cwd=working_dir)


def main():
  parser = optparse.OptionParser()
  parser.add_option('--source-dir', help='Source directory')
  parser.add_option('--output-dir', help='Directory to put javadoc')
  parser.add_option('--working-dir', help='Working directory')

  options, _ = parser.parse_args()

  GenerateJavadoc(options)

if __name__ == '__main__':
  sys.exit(main())

