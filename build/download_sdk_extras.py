#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script to download sdk/extras packages on the bots from google storage.

The script expects an argument that specifies the packet name in the following
format: <dir_in_sdk_extras>_<package_name>_<version>. There will be a
correpsonding bucket in google storage with that name, and it will be downloaded
to android_tools/sdk/extras/.
"""

import os
import subprocess
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'android'))
from pylib import constants

GSUTIL_PATH = os.path.join(os.path.dirname(__file__), os.pardir, os.pardir,
    os.pardir, os.pardir, os.pardir, os.pardir, 'depot_tools', 'gsutil.py')
SDK_EXTRAS_BUCKET = 'gs://chrome-sdk-extras'
SDK_EXTRAS_PATH = os.path.join(constants.ANDROID_SDK_ROOT, 'extras')


def GetCmdOutputAndStatus(cmd_lst):
  process = subprocess.Popen(cmd_lst, stdout=subprocess.PIPE)
  stdout, _ = process.communicate()
  return stdout, process.returncode

def is_android_buildbot_checkout():
  if not os.path.exists(GSUTIL_PATH) or not os.path.exists(SDK_EXTRAS_PATH):
    return False
  stdout, rc = GetCmdOutputAndStatus([GSUTIL_PATH, 'ls', SDK_EXTRAS_BUCKET])
  # If successfully read bucket, then this must be a bot with permissions
  return not rc

def main(args):
  if is_android_buildbot_checkout():
    success = True
    for arg in args[1:]:
      # Package is named <folder>_<package_name>_<version>
      first_underscore = arg.find('_')
      last_underscore = arg.rfind('_')
      folder = arg[0:first_underscore]
      package = arg[first_underscore+1:last_underscore]
      # Package bucket is <SDK_EXTRAS_BUCKET>/<folder>_<package_name>_<version>
      # and in that bucket will be the directory <folder>/<package_name> to cp.
      package_bucket = '%s/%s/%s/%s' % (SDK_EXTRAS_BUCKET, arg, folder, package)
      package_dir = '%s/%s/%s' % (SDK_EXTRAS_PATH, folder, package)
      if not os.path.exists(package_dir):
        os.makedirs(package_dir)
      # rsync is only supported by gsutil version 4.x
      cmd_lst = [GSUTIL_PATH, '--force-version', '4.6', '-m', 'rsync', '-r',
                 '-d', package_bucket, package_dir]
      stdout, rc = GetCmdOutputAndStatus(cmd_lst)
      success = (rc == 0) and success
    if not success:
      return 1
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
