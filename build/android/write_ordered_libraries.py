#!/usr/bin/env python
#
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Writes dependency ordered list of native libraries.

This list of libraries is used for several steps of building an APK.
"""

import json
import optparse
import os
import sys

from pylib import build_utils


def main(argv):
  parser = optparse.OptionParser()

  parser.add_option('--input-libraries',
      help='A list of top-level input libraries')
  parser.add_option('--output', help='Path to the generated .json file')
  parser.add_option('--stamp', help='Path to touch on success')

  options, _ = parser.parse_args()

  libraries = build_utils.ParseGypList(options.input_libraries)
  libraries = [os.path.basename(lib) for lib in libraries]

  with open(options.output, 'w') as outfile:
    json.dump(libraries, outfile)

  if options.stamp:
    build_utils.Touch(options.stamp)


if __name__ == '__main__':
  sys.exit(main(sys.argv))


