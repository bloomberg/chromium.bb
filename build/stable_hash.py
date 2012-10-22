#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Print the stable hash of a file/directory.

Compute a sha1 hash of a file/directory.
Ignore timestamps.
"""

import sys

import hashing_tools


def Main(args):
  if len(args) != 1:
    sys.stderr.write('Usage: %s <path>\n' % sys.argv[0])
    sys.exit(1)
  print hashing_tools.StableHashPath(args[0])


if __name__ == '__main__':
  Main(sys.argv[1:])
