#!/usr/bin/env python
#
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse
import os
import sys

from pylib import build_utils


def StripLibrary(android_strip, android_strip_args, library_path, output_path):
  strip_cmd = ([android_strip] +
               android_strip_args +
               ['-o', output_path, library_path])
  build_utils.CheckCallDie(strip_cmd)


def main(argv):
  parser = optparse.OptionParser()

  parser.add_option('--android-strip',
      help='Path to the toolchain\'s strip binary')
  parser.add_option('--android-strip-arg', action='append',
      help='Argument to be passed to strip')
  parser.add_option('--stripped-libraries-dir',
      help='Directory for stripped libraries')

  options, paths = parser.parse_args()

  build_utils.MakeDirectory(options.stripped_libraries_dir)

  for library_path in paths:
    stripped_library_path = os.path.join(options.stripped_libraries_dir,
        os.path.basename(library_path))
    StripLibrary(options.android_strip, options.android_strip_arg, library_path,
        stripped_library_path)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
