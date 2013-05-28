# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import sys


if sys.platform == 'linux2' and not os.environ.get('ANDROID_SDK_ROOT'):
  # If envsetup.sh hasn't been sourced and there's no adb in the path,
  # set it here.
  try:
    with file(os.devnull, 'w') as devnull:
      subprocess.call(['adb', 'version'], stdout=devnull, stderr=devnull)
  except OSError:
      print 'No adb found in $PATH, fallback to checked in binary.'
      os.environ['PATH'] += os.pathsep + os.path.abspath(os.path.join(
          os.path.dirname(__file__),
          '..', '..', '..',
          'third_party', 'android_tools', 'sdk', 'platform-tools'))
