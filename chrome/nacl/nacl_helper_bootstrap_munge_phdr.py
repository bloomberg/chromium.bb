#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This takes three command-line arguments:
      MUNGE-PHDR-PROGRAM      file name of program built from
                              nacl_helper_bootstrap_munge_phdr.c
      INFILE                  raw linked ELF file name
      OUTFILE                 output file name

We just run the MUNGE-PHDR-PROGRAM on a copy of INFILE.
That modifies the file in place.  Then we move it to OUTFILE.

We only have this wrapper script because nacl_helper_bootstrap_munge_phdr.c
wants to modify a file in place (and it would be a much longer and more
fragile program if it created a fresh ELF output file instead).
"""

import shutil
import subprocess
import sys


def Main(argv):
  if len(argv) != 4:
    print 'Usage: %s MUNGE-PHDR-PROGRAM INFILE OUTFILE' % argv[0]
    return 1
  [prog, munger, infile, outfile] = argv
  tmpfile = outfile + '.tmp'
  shutil.copy(infile, tmpfile)
  segment_num = '2'
  subprocess.check_call([munger, tmpfile, segment_num])
  shutil.move(tmpfile, outfile)
  return 0


if __name__ == '__main__':
  sys.exit(Main(sys.argv))
