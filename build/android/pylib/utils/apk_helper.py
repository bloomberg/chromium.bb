# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing utilities for apk packages."""

import os.path
import re

from pylib import cmd_helper
from pylib import constants


def GetPackageName(apk_path):
  """Returns the package name of the apk."""
  aapt = os.path.join(constants.ANDROID_SDK_TOOLS, 'aapt')
  aapt_output = cmd_helper.GetCmdOutput(
      [aapt, 'dump', 'badging', apk_path]).split('\n')
  package_name_re = re.compile(r'package: .*name=\'(\S*)\'')
  for line in aapt_output:
    m = package_name_re.match(line)
    if m:
      return m.group(1)
  raise Exception('Failed to determine package name of %s' % apk_path)
