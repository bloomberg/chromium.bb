#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Main entry point for toolchain_build buildbots.

Passes its arguments to toolchain_build.py.
"""

import os
import subprocess
import sys


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
NACL_DIR = os.path.dirname(SCRIPT_DIR)


if sys.platform == 'win32':
  print '@@@BUILD_STEP install mingw@@@'
  sys.stdout.flush()
  subprocess.check_call([os.path.join(NACL_DIR, 'buildbot', 'mingw_env.bat')])

print '@@@BUILD_STEP run_build_tests.py@@@'
sys.stdout.flush()
subprocess.check_call([
    sys.executable, os.path.join(NACL_DIR, 'build', 'run_build_tests.py')])

# Toolchain build emits its own annotator stages.
sys.stdout.flush()
subprocess.check_call([
    sys.executable,
    os.path.join(NACL_DIR, 'toolchain_build', 'toolchain_build.py')] +
    sys.argv[1:])
