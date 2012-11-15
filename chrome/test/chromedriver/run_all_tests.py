#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs all ChromeDriver end to end tests."""

import os
import sys

_THIS_DIR = os.path.abspath(os.path.dirname(__file__))
sys.path.append(os.path.join(_THIS_DIR, os.pardir, 'pylib'))

from common import chrome_paths
from common import util


def Main():
  print '@@@BUILD_STEP chromedriver2_tests@@@'
  chromedriver_map = {
    'win': 'chromedriver2.dll',
    'mac': 'chromedriver2.so',
    'linux': 'libchromedriver2.so',
  }
  chromedriver = chromedriver_map[util.GetPlatformName()]
  build_dir = chrome_paths.GetBuildDir([chromedriver])
  cmd = [
    sys.executable,
    os.path.join(_THIS_DIR, 'test.py'),
    os.path.join(build_dir, chromedriver),
  ]
  code = util.RunCommand(cmd)
  if code != 0:
    print '@@@STEP_FAILURE@@@'
  return code


if __name__ == '__main__':
  sys.exit(Main())
