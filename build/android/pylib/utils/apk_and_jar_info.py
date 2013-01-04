# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Gathers information about APKs/JARs."""

import logging
import os
import re

from pylib import cmd_helper

import apk_info
import jar_info

class ApkAndJarInfo(apk_info.ApkInfo, jar_info.JarInfo):
  """Helper class for for wrapping ApkInfo/JarInfo."""

  def __init__(self, apk_path, jar_path):
    apk_info.ApkInfo.__init__(self, apk_path)
    jar_info.JarInfo.__init__(self, jar_path)

