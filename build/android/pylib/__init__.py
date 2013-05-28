# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess


if not os.environ.get('ANDROID_SDK_ROOT'):
  # If envsetup.sh hasn't been sourced and there's no adb in the path,
  # set it here.
  with file(os.devnull, 'w') as devnull:
    ret = subprocess.call(['which', 'adb'], stdout=devnull, stderr=devnull)
    if ret:
      print 'No adb found in $PATH, fallback to checked in binary.'
      os.environ['PATH'] += os.pathsep + os.path.abspath(os.path.join(
          os.path.dirname(__file__),
          '..', '..', '..',
          'third_party', 'android_tools', 'sdk', 'platform-tools'))
