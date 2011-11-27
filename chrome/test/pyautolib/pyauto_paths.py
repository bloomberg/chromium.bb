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
  # List of dirs that can contain a Debug/Release build.
  outer_dirs = {
      'linux2': ['out', 'sconsbuild'],
      'linux3': ['out', 'sconsbuild'],
      'darwin': ['out', 'xcodebuild'],
      'win32':  ['chrome', 'build'],
      'cygwin': ['chrome'],
  }.get(sys.platform, [])
  src_dir = GetSourceDir()
  build_dirs = []
  for dir in outer_dirs:
    build_dirs += [os.path.join(src_dir, dir, 'Debug')]
    build_dirs += [os.path.join(src_dir, dir, 'Release')]
  return build_dirs


def GetChromeDriverExe():
  """Returns path to ChromeDriver executable, or None if cannot be found."""
  exe_name = 'chromedriver'
  if sys.platform == 'win32':
    exe_name += '.exe'

  import pyautolib
  dir = os.path.dirname(pyautolib.__file__)
  exe = os.path.join(dir, exe_name)
  if os.path.exists(exe):
    return exe
  return None
