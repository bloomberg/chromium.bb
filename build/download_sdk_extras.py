#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script to download sdk/extras packages on the bots from google storage.

The script expects arguments that specify zips file in the google storage
bucket named: <dir in SDK extras>_<package name>_<version>.zip. The file will
be extracted in the android_tools/sdk/extras directory.
"""

import os
import shutil
import subprocess
import sys
import zipfile

sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'android'))
from pylib import constants

GSUTIL_PATH = os.path.join(os.path.dirname(__file__), os.pardir, os.pardir,
    os.pardir, os.pardir, os.pardir, os.pardir, 'depot_tools', 'gsutil.py')
SDK_EXTRAS_BUCKET = 'gs://chrome-sdk-extras'
SDK_EXTRAS_PATH = os.path.join(constants.ANDROID_SDK_ROOT, 'extras')


def clean_and_extract(zip_file):
  # Extract the directory name and package name from zip file name. The format
  # of the zip file is <dir in sdk/extras>_<package_name>_<version>.zip.
  dir_name = zip_file[:zip_file.index('_')]
  package_name = zip_file[zip_file.index('_')+1:zip_file.rindex('_')]
  local_dir = '%s/%s/%s' % (SDK_EXTRAS_PATH, dir_name, package_name)
  if os.path.exists(local_dir):
    shutil.rmtree(local_dir)
  local_zip = '%s/%s' % (SDK_EXTRAS_PATH, zip_file)
  with zipfile.ZipFile(local_zip) as z:
    z.extractall(path=SDK_EXTRAS_PATH)


def main(args):
  if not os.path.exists(GSUTIL_PATH) or not os.path.exists(SDK_EXTRAS_PATH):
    # This is not a buildbot checkout.
    return 0
  for arg in args[1:]:
    local_zip = '%s/%s' % (SDK_EXTRAS_PATH, arg)
    if not os.path.exists(local_zip):
      package_zip = '%s/%s' % (SDK_EXTRAS_BUCKET, arg)
      subprocess.check_call([GSUTIL_PATH, '--force-version', '4.7', 'cp',
                             package_zip, local_zip])
    # Always clean dir and extract zip to ensure correct contents.
    clean_and_extract(arg)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
