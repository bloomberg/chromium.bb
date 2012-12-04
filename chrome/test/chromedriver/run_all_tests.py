#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs all ChromeDriver end to end tests."""

import os
import sys

_THIS_DIR = os.path.abspath(os.path.dirname(__file__))
sys.path.insert(0, os.path.join(_THIS_DIR, os.pardir, 'pylib'))

from common import chrome_paths
from common import util

def _AppendEnvironmentPath(env_name, path):
  if env_name in os.environ:
    lib_path = os.environ[env_name]
    if path not in lib_path:
      os.environ[env_name] += os.pathsep + path
  else:
    os.environ[env_name] = path

def _FindChromeBinary(path):
  if util.IsLinux():
    exes = ['chrome']
  elif util.IsMac():
    exes = [
      'Google Chrome.app/Contents/MacOS/Google Chrome',
      'Chromium.app/Contents/MacOS/Chromium'
    ]
  elif util.IsWindows():
    exes = ['chrome.exe']
  else:
    exes = []
  for exe in exes:
    binary = os.path.join(path, exe)
    if os.path.exists(binary):
      return binary
  return None

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
  # Set the built chrome binary.
  chrome_binary = _FindChromeBinary(build_dir)
  if chrome_binary is not None:
    cmd.append(chrome_binary)
  if util.IsLinux():
    # Set LD_LIBRARY_PATH to enable successful loading of shared object files,
    # when chromedriver2.so is not a static build.
    _AppendEnvironmentPath('LD_LIBRARY_PATH', os.path.join(build_dir, 'lib'))
  elif util.IsMac():
    # In Mac, chromedriver2.so is a 32-bit build, so run with the 32-bit python.
    os.environ['VERSIONER_PYTHON_PREFER_32_BIT'] = 'yes'
  code = util.RunCommand(cmd)
  if code != 0:
    print '@@@STEP_FAILURE@@@'
  return code


if __name__ == '__main__':
  sys.exit(Main())
