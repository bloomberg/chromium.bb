#!/usr/bin/python

# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import platform
import sys


def _SetupPaths():
  start_dir = os.path.abspath(os.path.dirname(__file__))
  J = os.path.join

  global CHROMEDRIVER_TEST_DATA
  CHROMEDRIVER_TEST_DATA = start_dir

  global SRC_THIRD_PARTY, PYTHON_BINDINGS, WEBDRIVER_TEST_DATA
  SRC_THIRD_PARTY = J(start_dir, os.pardir, os.pardir, os.pardir, os.pardir,
                      'third_party')
  webdriver = J(SRC_THIRD_PARTY, 'webdriver')
  PYTHON_BINDINGS = J(webdriver, 'pylib')
  WEBDRIVER_TEST_DATA = J(webdriver, 'test_data')

  global CHROMEDRIVER_EXE
  exe_name = 'chromedriver'
  if platform.system() == 'Windows':
    exe_name += '.exe'
  for dir in _DefaultExeLocations():
    path = os.path.join(dir, exe_name)
    if os.path.exists(path):
      CHROMEDRIVER_EXE = os.path.abspath(path)
      break
  else:
    raise RuntimeError('ChromeDriver exe could not be found in its default '
                       'location. Searched in following directories: ' +
                       ', '.join(_DefaultExeLocations()))


def _DefaultExeLocations():
  """Returns the paths that are used to find the ChromeDriver executable.

  Returns:
    a list of directories that would be searched for the executable
  """
  script_dir = os.path.dirname(__file__)
  chrome_src = os.path.abspath(os.path.join(
      script_dir, os.pardir, os.pardir, os.pardir, os.pardir))
  bin_dirs = {
    'linux2': [ os.path.join(chrome_src, 'out', 'Debug'),
                os.path.join(chrome_src, 'sconsbuild', 'Debug'),
                os.path.join(chrome_src, 'out', 'Release'),
                os.path.join(chrome_src, 'sconsbuild', 'Release')],
    'linux3': [ os.path.join(chrome_src, 'out', 'Debug'),
                os.path.join(chrome_src, 'sconsbuild', 'Debug'),
                os.path.join(chrome_src, 'out', 'Release'),
                os.path.join(chrome_src, 'sconsbuild', 'Release')],
    'darwin': [ os.path.join(chrome_src, 'xcodebuild', 'Debug'),
                os.path.join(chrome_src, 'xcodebuild', 'Release')],
    'win32':  [ os.path.join(chrome_src, 'chrome', 'Debug'),
                os.path.join(chrome_src, 'build', 'Debug'),
                os.path.join(chrome_src, 'chrome', 'Release'),
                os.path.join(chrome_src, 'build', 'Release')],
  }
  return [os.getcwd()] + bin_dirs.get(sys.platform, [])


def DataDir():
  """Returns the path to the data dir chrome/test/data."""
  return os.path.normpath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, "data"))


def TestDir():
  """Returns the path to the test dir chrome/test/webdriver/test."""
  return os.path.normpath(
    os.path.join(os.path.dirname(__file__)))


_SetupPaths()
