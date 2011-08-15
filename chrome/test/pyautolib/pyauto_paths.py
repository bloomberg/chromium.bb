#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common paths for pyauto tests."""

import os
import sys


def GetSourceDir():
  """Returns src/ directory."""
  script_dir = os.path.abspath(os.path.dirname(__file__))
  return os.path.join(script_dir, os.pardir, os.pardir, os.pardir)


def GetThirdPartyDir():
  """Returns src/third_party directory."""
  return os.path.join(GetSourceDir(), 'third_party')


def GetBuildDirs():
  """Returns list of possible build directories."""
  build_dirs = {
      'linux2': [ os.path.join('out', 'Debug'),
                  os.path.join('sconsbuild', 'Debug'),
                  os.path.join('out', 'Release'),
                  os.path.join('sconsbuild', 'Release')],
      'linux3': [ os.path.join('out', 'Debug'),
                  os.path.join('sconsbuild', 'Debug'),
                  os.path.join('out', 'Release'),
                  os.path.join('sconsbuild', 'Release')],
      'darwin': [ os.path.join('xcodebuild', 'Debug'),
                  os.path.join('xcodebuild', 'Release')],
      'win32':  [ os.path.join('chrome', 'Debug'),
                  os.path.join('build', 'Debug'),
                  os.path.join('chrome', 'Release'),
                  os.path.join('build', 'Release')],
      'cygwin': [ os.path.join('chrome', 'Debug'),
                  os.path.join('chrome', 'Release')],
  }
  src_dir = GetSourceDir()
  return map(lambda dir: os.path.join(src_dir, dir),
             build_dirs.get(sys.platform, []))


def GetChromeDriverExe():
  """Returns path to ChromeDriver executable, or None if cannot be found."""
  exe_name = 'chromedriver'
  if sys.platform == 'win32':
    exe_name += '.exe'
  for dir in GetBuildDirs():
    exe = os.path.join(dir, exe_name)
    if os.path.exists(exe):
      return exe
  return None
