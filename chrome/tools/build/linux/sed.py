# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Usage sed.py <infile> <outfile> [<other args>]
#
# Does the equivalent of
#   sed <options> infile > outfile

import sys
import subprocess

if len(sys.argv) <= 3:
  print "Need at least 3 arguments to sed.py"
  sys.exit(1)

infile = sys.argv[1]
outfile = sys.argv[2]

sys.exit(subprocess.call('sed "' + '" "'.join(sys.argv[3:]) + '" ' + infile +
                         ' > ' + outfile, shell=True))
