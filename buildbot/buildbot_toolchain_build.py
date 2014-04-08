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

import packages

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
NACL_DIR = os.path.dirname(SCRIPT_DIR)
BUILD_DIR = os.path.join(NACL_DIR, 'build')
TOOLCHAIN_BUILD_DIR = os.path.join(NACL_DIR, 'toolchain_build')
TOOLCHAIN_BUILD_OUT_DIR = os.path.join(TOOLCHAIN_BUILD_DIR, 'out')

TEMP_PACKAGES_FILE = os.path.join(TOOLCHAIN_BUILD_OUT_DIR, 'packages.txt')

build_name = sys.argv[1]
bot_type = sys.argv[2]

build_script = os.path.join(NACL_DIR, 'toolchain_build', build_name + '.py')
if not os.path.isfile(build_script):
  print 'Error - Unknown build script: %s' % build_script
  sys.exit(1)

if sys.platform == 'win32':
  print '@@@BUILD_STEP install mingw@@@'
  sys.stdout.flush()
  subprocess.check_call([os.path.join(NACL_DIR, 'buildbot', 'mingw_env.bat')])

print '@@@BUILD_STEP run_pynacl_tests.py@@@'
sys.stdout.flush()
subprocess.check_call([
    sys.executable, os.path.join(NACL_DIR, 'pynacl', 'run_pynacl_tests.py')])

# Toolchain build emits its own annotator stages.
sys.stdout.flush()
subprocess.check_call([sys.executable,
                       build_script,
                       '--packages-file', TEMP_PACKAGES_FILE]
                       + sys.argv[2:])

if bot_type == '--buildbot' or bot_type == '--trybot':
  packages.UploadPackages(TEMP_PACKAGES_FILE, bot_type == '--trybot')
