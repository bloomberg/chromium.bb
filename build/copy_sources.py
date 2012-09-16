#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import shutil
import sys

"""Copy Sources

Copy from a source file or directory to a new file or directory.  This
supports renaming of the file.
"""


def MakeDir(outdir):
  if outdir and not os.path.exists(outdir):
    os.makedirs(outdir)


def Main(argv):
  if len(argv) != 3:
    print 'Expecting: copy_sources.py <source file> <dest file/dir>'
    return 1

  if not os.path.exists(argv[1]):
    print 'File not found: %s' % argv[1]
    return 1

  shutil.copy(argv[1], argv[2])
  return 0

if __name__ == '__main__':
  sys.exit(Main(sys.argv))

