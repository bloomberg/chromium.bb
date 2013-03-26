#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs all ChromeDriver end to end tests."""

import optparse
import os
import sys

_THIS_DIR = os.path.abspath(os.path.dirname(__file__))
sys.path.insert(0, os.path.join(_THIS_DIR, os.pardir, 'pylib'))

from common import chrome_paths
from common import util
import continuous_archive


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


def _GenerateTestCommand(script, chromedriver, chrome=None,
                         chrome_revision=None, android_package=None):
  cmd = [
      sys.executable,
      os.path.join(_THIS_DIR, script),
      '--chromedriver=' + chromedriver,
  ]
  if chrome:
    cmd.append('--chrome=' + chrome)
  if chrome_revision:
    cmd.append('--chrome-revision=' + chrome_revision)

  if android_package:
    cmd.append('--android-package=' + android_package)
  return cmd


def RunPythonTests(chromedriver, chrome=None, chrome_revision=None,
                   android_package=None):
  revision_info = ''
  if chrome_revision:
    revision_info = '(r%s)' % chrome_revision
  print '@@@BUILD_STEP chromedriver2_python_tests%s@@@' % revision_info
  if util.IsMac():
    # In Mac, chromedriver2.so is a 32-bit build, so run with the 32-bit python.
    os.environ['VERSIONER_PYTHON_PREFER_32_BIT'] = 'yes'
  code = util.RunCommand(
      _GenerateTestCommand('run_py_tests.py', chromedriver, chrome,
                           chrome_revision, android_package))
  if code:
    print '\n@@@STEP_FAILURE@@@'
  return code


def RunJavaTests(chromedriver, chrome=None, chrome_revision=None,
                 android_package=None):
  revision_info = ''
  if chrome_revision:
    revision_info = '(r%s)' % chrome_revision
  print '@@@BUILD_STEP chromedriver2_java_tests%s@@@' % revision_info
  code = util.RunCommand(
      _GenerateTestCommand('run_java_tests.py', chromedriver, chrome,
                           chrome_revision, android_package))
  if code:
    print '@@@STEP_FAILURE@@@'
  return code


def RunCppTests(cpp_tests):
  print '@@@BUILD_STEP chromedriver2_tests@@@'
  code = util.RunCommand([cpp_tests])
  if code:
    print '@@@STEP_FAILURE@@@'
  return code


def main():
  parser = optparse.OptionParser()
  parser.add_option(
      '', '--android-package',
      help='Application package name, if running tests on Android.')
  options, _ = parser.parse_args()

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
    server_name = 'chromedriver2_server.exe'
  else:
    cpp_tests_name = 'chromedriver2_tests'
    server_name = 'chromedriver2_server'

  required_build_outputs = [chromedriver_name]
  if not options.android_package:
    required_build_outputs += [cpp_tests_name, server_name]
  build_dir = chrome_paths.GetBuildDir(required_build_outputs)
  print 'Using build outputs from', build_dir

  chromedriver = os.path.join(build_dir, chromedriver_name)
  chromedriver_server = os.path.join(build_dir, server_name)

  if util.IsLinux():
    # Set LD_LIBRARY_PATH to enable successful loading of shared object files,
    # when chromedriver2.so is not a static build.
    _AppendEnvironmentPath('LD_LIBRARY_PATH', os.path.join(build_dir, 'lib'))
  elif util.IsWindows():
    # For Windows bots: add ant, java(jre) and the like to system path.
    _AddToolsToSystemPathForWindows()

  if options.android_package:
    os.environ['PATH'] += os.pathsep + os.path.join(_THIS_DIR, 'chrome')
    code1 = RunPythonTests(chromedriver,
                           android_package=options.android_package)
    code2 = RunJavaTests(chromedriver_server,
                         android_package=options.android_package)
    return code1 or code2
  else:
    chrome_tip_of_tree = os.path.join(build_dir, chrome_name)
    cpp_tests = os.path.join(build_dir, cpp_tests_name)

    chrome_26 = continuous_archive.DownloadChrome(
        continuous_archive.CHROME_26_REVISION, util.MakeTempDir())
    chrome_path_revisions = [
        {'path': chrome_tip_of_tree, 'revision': 'HEAD'},
        {'path': chrome_26, 'revision': continuous_archive.CHROME_26_REVISION}
    ]
    code = 0
    for chrome in chrome_path_revisions:
      code1 = RunPythonTests(chromedriver, chrome=chrome['path'],
                             chrome_revision=chrome['revision'])
      code2 = RunJavaTests(chromedriver_server, chrome=chrome['path'],
                           chrome_revision=chrome['revision'])
      code = code or code1 or code2
    return RunCppTests(cpp_tests) or code


if __name__ == '__main__':
  sys.exit(main())
