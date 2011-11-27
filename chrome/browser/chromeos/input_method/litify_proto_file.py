#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Litify a .proto file.

This program add a line
  "option optimize_for = LITE_RUNTIME;"
to the input .proto file.

Run it like:
  litify_proto_file.py input.proto output.proto
"""

import fileinput
import sys


def main(argv):
  if len(argv) != 3:
    print 'Usage: litify_proto_file.py [input] [output]'
    return 1
  output_file = open(sys.argv[2], 'w')
  for line in fileinput.input(sys.argv[1]):
    output_file.write(line)

  output_file.write("\noption optimize_for = LITE_RUNTIME;\n")
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
