#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script to download sdk/extras packages on the bots from google storage.

The script expects arguments that specify zips file in the google storage
bucket named: <dir in SDK extras>_<package name>_<version>.zip. The file will
be extracted in the android_tools/sdk/extras directory on the test bots. This
script will not do anything for developers.

TODO(navabi): Move this script (crbug.com/459819).
"""

import json
import os
import shutil
import subprocess
import sys
import zipfile

SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
CHROME_SRC = os.path.abspath(os.path.join(SCRIPT_DIR, os.pardir))
sys.path.insert(0, os.path.join(SCRIPT_DIR, 'android'))
sys.path.insert(1, os.path.join(CHROME_SRC, 'tools'))

from pylib import constants
import find_depot_tools

DEPOT_PATH = find_depot_tools.add_depot_tools_to_path()
GSUTIL_PATH = os.path.join(DEPOT_PATH, 'gsutil.py')
SDK_EXTRAS_BUCKET = 'gs://chrome-sdk-extras'
SDK_EXTRAS_PATH = os.path.join(constants.ANDROID_SDK_ROOT, 'extras')
SDK_EXTRAS_JSON_FILE = os.path.join(os.path.dirname(__file__),
                                    'android_sdk_extras.json')


def clean_and_extract(dir_name, package_name, zip_file):
  local_dir = '%s/%s/%s' % (SDK_EXTRAS_PATH, dir_name, package_name)
  if os.path.exists(local_dir):
    shutil.rmtree(local_dir)
  local_zip = '%s/%s' % (SDK_EXTRAS_PATH, zip_file)
  with zipfile.ZipFile(local_zip) as z:
    z.extractall(path=SDK_EXTRAS_PATH)


def download_package_if_needed(remote_file, local_file):
  """Download a file from GCS.

  Returns:
    success (bool): True if the download succeeded, False otherwise.
  """
  if not os.path.exists(local_file):
    try:
      subprocess.check_call(['python', GSUTIL_PATH, '--force-version', '4.7',
                             'cp', remote_file, local_file])
    except subprocess.CalledProcessError:
      print ('WARNING: Failed to download SDK packages. If this bot compiles '
             'for Android, it may have errors.')
      return False
  return True

def main():
  if not os.environ.get('CHROME_HEADLESS'):
    # This is not a buildbot checkout.
    return 0
  # Update the android_sdk_extras.json file to update downloaded packages.
  with open(SDK_EXTRAS_JSON_FILE) as json_file:
    packages = json.load(json_file)
  for package in packages:
    local_zip = '%s/%s' % (SDK_EXTRAS_PATH, package['zip'])
    package_zip = '%s/%s' % (SDK_EXTRAS_BUCKET, package['zip'])
    for attempt in xrange(2):
      print '(%d) Downloading package %s' % (attempt + 1, package['zip'])
      if not download_package_if_needed(package_zip, local_zip):
        # Ignore errors when download failed to keep the corresponding build
        # step green. The error we're ignoring here is essentially
        # 'permission denied', because we're using the presence or absence of
        # credentials on a build machine as the way to mark android builders.
        # See crbug.com/460463 for more context.
        return 0
      try:
        # Always clean dir and extract zip to ensure correct contents.
        clean_and_extract(package['dir_name'],
                          package['package'],
                          package['zip'])
        break
      except zipfile.BadZipfile:
        print 'Failed unpacking zip file. Deleting and retrying...'
        os.remove(local_zip)

    else:
      print ('WARNING: Failed to unpack SDK packages. If this bot compiles '
             'for Android, it may have errors.')
      return 1


if __name__ == '__main__':
  sys.exit(main())
