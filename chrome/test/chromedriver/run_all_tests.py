#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs all ChromeDriver end to end tests."""

import os
import shutil
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


def _AddToolsToSystemPathForWindows():
  path_cfg_file = 'C:\\tools\\bots_path.cfg'
  if not os.path.exists(path_cfg_file):
    print 'Failed to find file', path_cfg_file
  with open(path_cfg_file, 'r') as cfg:
    paths = cfg.read().split('\n')
  os.environ['PATH'] = os.pathsep.join(paths) + os.pathsep + os.environ['PATH']


def RunPythonTests(chromedriver, chrome):
  print '@@@BUILD_STEP chromedriver2_python_tests@@@'
  cmd = [
    sys.executable,
    os.path.join(_THIS_DIR, 'run_py_tests.py'),
    '--chromedriver=' + chromedriver,
    '--chrome=' + chrome,
  ]
  if util.IsMac():
    # In Mac, chromedriver2.so is a 32-bit build, so run with the 32-bit python.
    os.environ['VERSIONER_PYTHON_PREFER_32_BIT'] = 'yes'
  if util.RunCommand(cmd):
    print '@@@STEP_FAILURE@@@'


def RunJavaTests(chromedriver, chrome):
  print '@@@BUILD_STEP chromedriver2_java_tests@@@'
  cmd = [
    sys.executable,
    os.path.join(_THIS_DIR, 'run_java_tests.py'),
    '--chromedriver=' + chromedriver,
    '--chrome=' + chrome,
  ]
  if util.RunCommand(cmd):
    print '@@@STEP_FAILURE@@@'


def RunCppTests(cpp_tests):
  print '@@@BUILD_STEP chromedriver2_tests@@@'
  if util.RunCommand([cpp_tests]):
    print '@@@STEP_FAILURE@@@'


def main():
  chromedriver_map = {
    'win': 'chromedriver2.dll',
    'mac': 'chromedriver2.so',
    'linux': 'libchromedriver2.so',
  }
  chromedriver_name = chromedriver_map[util.GetPlatformName()]

  chrome_map = {
    'win': 'chrome.exe',
    'mac': 'Chromium.app/Contents/MacOS/Chromium',
    'linux': 'chrome',
  }
  chrome_name = chrome_map[util.GetPlatformName()]

  if util.IsWindows():
    cpp_tests_name = 'chromedriver2_tests.exe'
  else:
    cpp_tests_name = 'chromedriver2_tests'

  required_build_outputs = [chromedriver_name, chrome_name, cpp_tests_name]
  build_dir = chrome_paths.GetBuildDir(required_build_outputs)
  print 'Using build outputs from', build_dir

  chromedriver = os.path.join(build_dir, chromedriver_name)
  chrome = os.path.join(build_dir, chrome_name)
  cpp_tests = os.path.join(build_dir, cpp_tests_name)

  if util.IsLinux():
    # Set LD_LIBRARY_PATH to enable successful loading of shared object files,
    # when chromedriver2.so is not a static build.
    _AppendEnvironmentPath('LD_LIBRARY_PATH', os.path.join(build_dir, 'lib'))
  elif util.IsWindows():
    # For Windows bots: add ant, java(jre) and the like to system path.
    _AddToolsToSystemPathForWindows()

  code1 = RunPythonTests(chromedriver, chrome)
  code2 = RunJavaTests(chromedriver, chrome)
  code3 = RunCppTests(cpp_tests)
  return code1 or code2 or code3


if __name__ == '__main__':
  sys.exit(main())
