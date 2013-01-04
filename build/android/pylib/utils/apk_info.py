# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Gathers information about APKs."""

import logging
import os
import re

# TODO(frankf): Move cmd_helper to utils.
from pylib import cmd_helper


def GetPackageNameForApk(apk_path):
  """Returns the package name of this APK."""
  aapt_output = cmd_helper.GetCmdOutput(
      ['aapt', 'dump', 'badging', apk_path]).split('\n')
  package_name_re = re.compile(r'package: .*name=\'(\S*)\'')
  for line in aapt_output:
    m = package_name_re.match(line)
    if m:
      return m.group(1)
  raise Exception('Failed to determine package name of %s' % apk_path)


class ApkInfo(object):
  """Helper class for inspecting APKs."""

  def __init__(self, apk_path):
    if not os.path.exists(apk_path):
      raise Exception('%s not found, please build it' % apk_path)
    self._apk_path = apk_path

  def GetApkPath(self):
    return self._apk_path

  def GetPackageName(self):
    """Returns the package name of this APK."""
    return GetPackageNameForApk(self._apk_path)

