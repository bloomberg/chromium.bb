#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs all ChromeDriver end to end tests."""

import optparse
import os
import sys

import archive
import chrome_paths
import util

_THIS_DIR = os.path.abspath(os.path.dirname(__file__))


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
                         chrome_version=None, android_package=None):
  cmd = [
      sys.executable,
      os.path.join(_THIS_DIR, script),
      '--chromedriver=' + chromedriver,
  ]
  if chrome:
    cmd.append('--chrome=' + chrome)
  if chrome_version:
    cmd.append('--chrome-version=' + chrome_version)

  if android_package:
    cmd.append('--android-package=' + android_package)
  return cmd


def RunPythonTests(chromedriver, chrome=None, chrome_version=None,
                   chrome_version_name=None, android_package=None):
  version_info = ''
  if chrome_version_name:
    version_info = '(v%s)' % chrome_version_name
  util.MarkBuildStepStart('python_tests%s' % version_info)
  code = util.RunCommand(
      _GenerateTestCommand('run_py_tests.py', chromedriver, chrome,
                           chrome_version, android_package))
  if code:
    util.MarkBuildStepError()
  return code


def RunJavaTests(chromedriver, chrome=None, chrome_version=None,
                 chrome_version_name=None, android_package=None):
  version_info = ''
  if chrome_version_name:
    version_info = '(v%s)' % chrome_version_name
  util.MarkBuildStepStart('java_tests%s' % version_info)
  code = util.RunCommand(
      _GenerateTestCommand('run_java_tests.py', chromedriver, chrome,
                           chrome_version, android_package))
  if code:
    util.MarkBuildStepError()
  return code


def RunCppTests(cpp_tests):
  util.MarkBuildStepStart('chromedriver2_tests')
  code = util.RunCommand([cpp_tests])
  if code:
    util.MarkBuildStepError()
  return code


def main():
  parser = optparse.OptionParser()
  parser.add_option(
      '', '--android-package',
      help='Application package name, if running tests on Android.')
  # Option 'chrome-version' is for desktop only.
  parser.add_option(
      '', '--chrome-version',
      help='Version of chrome, e.g., \'HEAD\', \'27\', or \'26\'.'
           'Default is to run tests against all of these versions.'
           'Notice: this option only applies to desktop.')
  options, _ = parser.parse_args()

  postfix = ''
  if util.IsWindows():
    postfix = '.exe'
  cpp_tests_name = 'chromedriver2_tests' + postfix
  server_name = 'chromedriver2_server' + postfix

  required_build_outputs = [server_name]
  if not options.android_package:
    required_build_outputs += [cpp_tests_name]
  build_dir = chrome_paths.GetBuildDir(required_build_outputs)
  print 'Using build outputs from', build_dir

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
    code1 = RunPythonTests(chromedriver_server,
                           android_package=options.android_package)
    code2 = RunJavaTests(chromedriver_server,
                         android_package=options.android_package)
    return code1 or code2
  else:
    latest_snapshot_revision = archive.GetLatestRevision(archive.Site.SNAPSHOT)
    versions = [
        ['HEAD', latest_snapshot_revision],
        ['28', archive.CHROME_28_REVISION],
        ['27', archive.CHROME_27_REVISION]
    ]
    code = 0
    for version in versions:
      if options.chrome_version and version[0] != options.chrome_version:
        continue
      download_site = archive.Site.CONTINUOUS
      version_name = version[0]
      if version_name == 'HEAD':
        version_name = version[1]
        download_site = archive.Site.SNAPSHOT
      chrome_path = archive.DownloadChrome(version[1],
                                           util.MakeTempDir(),
                                           download_site)
      code1 = RunPythonTests(chromedriver_server, chrome=chrome_path,
                             chrome_version=version[0],
                             chrome_version_name=version_name)
      code2 = RunJavaTests(chromedriver_server, chrome=chrome_path,
                           chrome_version=version[0],
                           chrome_version_name=version_name)
      code = code or code1 or code2
    cpp_tests = os.path.join(build_dir, cpp_tests_name)
    return RunCppTests(cpp_tests) or code


if __name__ == '__main__':
  sys.exit(main())
