#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import glob
import os
import shutil
import stat
import sys

def usage():
  print("""Usage: squashdir.py <dest-dir> <source-dir> ...

Basic tool to copy a directory heirarchy into a flat space.

This crawls an arbitrarily deep heirarchy of files and directories, and copies
each file into the destination directory.  The destination file name will
include the relative path to the source file, with '^^' inserted between each
directory name.

The resulting directory can then be imported into the file manager test harness,
which will reconstitute the directory structure.

This is used to work around the fact that the FileList and File objects
presented by <input type=file multiple> do not allow users to recurse a
selected directory, nor do they provide information about directory structure.
""")


def status(msg):
  sys.stderr.write(msg + '\n')


def scan_path(dest, src, path):
  abs_src = os.path.join(src, path)
  statinfo = os.stat(abs_src)

  basename = os.path.basename(path)

  if not stat.S_ISDIR(statinfo.st_mode):
    newname = os.path.join(dest, path.replace('/', '^^'))
    status(newname)
    shutil.copyfile(abs_src, newname)
  else:
    for child_path in glob.glob(abs_src + '/*'):
      scan_path(dest, src, child_path[len(src) + 1:])


def main():
  if len(sys.argv) < 3 or sys.argv[1][0] == '-':
    usage()
    return 1

  dest = sys.argv[1]
  for src in sys.argv[2:]:
    abs_src = os.path.abspath(src)
    path = os.path.basename(abs_src)
    abs_src = os.path.dirname(abs_src)
    scan_path(dest, abs_src, path)
  return 0


if __name__ == '__main__':
  sys.exit(main())
