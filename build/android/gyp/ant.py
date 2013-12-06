#!/usr/bin/env python
#
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""An Ant wrapper that suppresses useless Ant output.

Ant build scripts output "BUILD SUCCESSFUL" and build timing at the end of
every build. In the Android build, this just adds a lot of useless noise to the
build output. This script forwards its arguments to ant, and prints Ant's
output up until the BUILD SUCCESSFUL line.
"""

import sys
import traceback

from util import build_utils


def main(argv):
  try:
    stdout = build_utils.CheckOutput(['ant'] + argv[1:])
  except build_utils.CalledProcessError as e:
    traceback.print_exc()
    if '-quiet' in e.args:
      sys.stderr.write('Tip: run the ant command above without the -quiet flag '
          'to see more details on the error\n')
    sys.exit(1)
  stdout = stdout.strip().split('\n')
  for line in stdout:
    if line.strip() == 'BUILD SUCCESSFUL':
      break
    print line


if __name__ == '__main__':
  sys.exit(main(sys.argv))
