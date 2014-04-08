#!/usr/bin/python
# Copyright (c) 2014 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
NACL_DIR = os.path.dirname(SCRIPT_DIR)
BUILD_DIR = os.path.join(NACL_DIR, 'build')

PACKAGE_VERSION_DIR = os.path.join(BUILD_DIR, 'package_version')
PACKAGE_VERSION_SCRIPT = os.path.join(PACKAGE_VERSION_DIR, 'package_version.py')

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

def UploadPackages(filename, is_try):
  """ Upload packages to Google Storage.

  Args:
    filename: File to read package descriptions from.
    is_try: True if the run is for a trybot, False if for a real buildbot.
  """

  if not is_try:
    upload_rev = BUILDBOT_REVISION
    upload_args = []
  else:
    upload_rev = '%s/%s' % (BUILDBOT_BUILDERNAME, BUILDBOT_BUILDNUMBER)
    upload_args = ['--cloud-bucket', 'nativeclient-trybot/packages']

  with open(filename, 'rt') as f:
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
