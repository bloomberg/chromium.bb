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
BUILD_DIR = os.path.join(NACL_DIR, 'build')
PACKAGE_VERSION_DIR = os.path.join(BUILD_DIR, 'package_version')
TOOLCHAIN_BUILD_DIR = os.path.join(NACL_DIR, 'toolchain_build')
TOOLCHAIN_BUILD_OUT_DIR = os.path.join(TOOLCHAIN_BUILD_DIR, 'out')

PACKAGE_VERSION_SCRIPT = os.path.join(PACKAGE_VERSION_DIR, 'package_version.py')
TEMP_PACKAGES_FILE = os.path.join(TOOLCHAIN_BUILD_OUT_DIR, 'packages.txt')
BUILDBOT_REVISION = os.getenv('BUILDBOT_GOT_REVISION', None)
BUILDBOT_BUILDERNAME = os.getenv('BUILDBOT_BUILDERNAME', None)
BUILDBOT_BUILDNUMBER = os.getenv('BUILDBOT_BUILDNUMBER', None)


if BUILDBOT_REVISION is None:
  print 'Error - Could not obtain buildbot revision number'
  sys.exit(1)
elif BUILDBOT_BUILDERNAME is None:
  print 'Error - could not obtain buildbot builder name'
  sys.exit(1)
elif BUILDBOT_BUILDNUMBER is None:
  print 'Error - could not obtain buildbot build number'
  sys.exit(1)

if len(sys.argv) < 3:
  print 'Error - expected args <build_name> <--bottype>'
  sys.exit(1)

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

# Upload the package file when building using bots
upload_rev = None
upload_args = []
if bot_type == '--buildbot':
  upload_rev = BUILDBOT_REVISION
elif bot_type == '--trybot':
  upload_rev = '%s/%s' % (BUILDBOT_BUILDERNAME, BUILDBOT_BUILDNUMBER)
  upload_args.extend(['--cloud-bucket', 'nativeclient-trybot/packages'])

if upload_rev is not None:
  print '@@@BUILD_STEP upload_package_info@@@'
  sys.stdout.flush()
  with open(TEMP_PACKAGES_FILE, 'rt') as f:
    for package_file in f.readlines():
      package_file = package_file.strip()
      pkg_name, pkg_ext = os.path.splitext(os.path.basename(package_file))
      pkg_target = os.path.basename(os.path.dirname(package_file))
      full_package_name = '%s/%s' % (pkg_target, pkg_name)

      subprocess.check_call([sys.executable,
                             PACKAGE_VERSION_SCRIPT] +
                             upload_args +
                             ['--annotate',
                             'upload',
                             '--skip-missing',
                             '--upload-package', full_package_name,
                             '--revision', upload_rev,
                             '--package-file', package_file])
