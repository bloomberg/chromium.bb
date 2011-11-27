# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import platform
import sys

import util


def GetTestDataPath(relative_path):
  """Returns the path to the given path under chromedriver's test data dir."""
  return os.path.join(TEST_DATA_PATH, relative_path)


def GetChromeTestDataPath(relative_path):
  """Returns the path to the given path under chrome's test data dir."""
  return os.path.join(CHROME_TEST_DATA_PATH, relative_path)


def _SetupPaths():
  start_dir = os.path.abspath(os.path.dirname(__file__))
  J = os.path.join

  global SRC_PATH
  SRC_PATH = J(start_dir, os.pardir, os.pardir, os.pardir, os.pardir)

  global TEST_DATA_PATH
  TEST_DATA_PATH = start_dir

  global CHROME_TEST_DATA_PATH
  CHROME_TEST_DATA_PATH = J(SRC_PATH, 'chrome', 'test', 'data')

  global SRC_THIRD_PARTY, PYTHON_BINDINGS, WEBDRIVER_TEST_DATA
  SRC_THIRD_PARTY = J(SRC_PATH, 'third_party')
  webdriver = J(SRC_THIRD_PARTY, 'webdriver')
  PYTHON_BINDINGS = J(webdriver, 'pylib')
  WEBDRIVER_TEST_DATA = J(webdriver, 'test_data')

  global CHROMEDRIVER_EXE, CHROME_EXE
  def _FindDriver():
    cd_exe_name = 'chromedriver'
    if util.IsWin():
      cd_exe_name += '.exe'
    for dir in _DefaultExeLocations():
      path = os.path.abspath(os.path.join(dir, cd_exe_name))
      if os.path.exists(path):
        return path
    return None
  CHROMEDRIVER_EXE = _FindDriver()

  def _FindChrome():
    possible_paths = []
    if util.IsWin():
      possible_paths += ['chrome.exe']
    elif util.IsMac():
      possible_paths += ['Chromium.app/Contents/MacOS/Chromium',
                         'Google Chrome.app/Contents/MacOS/Google Chrome']
    elif util.IsLinux():
      possible_paths += ['chrome']
    for dir in _DefaultExeLocations():
      for chrome_path in possible_paths:
        path = os.path.abspath(os.path.join(dir, chrome_path))
        if os.path.exists(path):
          return path
    return None
  CHROME_EXE = _FindChrome()


def _DefaultExeLocations():
  """Returns the paths that are used to find the ChromeDriver executable.

  Returns:
    a list of directories that would be searched for the executable
  """
  bin_dirs = {
    'linux2': [ os.path.join(SRC_PATH, 'out', 'Debug'),
                os.path.join(SRC_PATH, 'sconsbuild', 'Debug'),
                os.path.join(SRC_PATH, 'out', 'Release'),
                os.path.join(SRC_PATH, 'sconsbuild', 'Release')],
    'linux3': [ os.path.join(SRC_PATH, 'out', 'Debug'),
                os.path.join(SRC_PATH, 'sconsbuild', 'Debug'),
                os.path.join(SRC_PATH, 'out', 'Release'),
                os.path.join(SRC_PATH, 'sconsbuild', 'Release')],
    'darwin': [ os.path.join(SRC_PATH, 'xcodebuild', 'Debug'),
                os.path.join(SRC_PATH, 'xcodebuild', 'Release')],
    'win32':  [ os.path.join(SRC_PATH, 'chrome', 'Debug'),
                os.path.join(SRC_PATH, 'build', 'Debug'),
                os.path.join(SRC_PATH, 'chrome', 'Release'),
                os.path.join(SRC_PATH, 'build', 'Release')],
  }
  return bin_dirs.get(sys.platform, [])


_SetupPaths()
