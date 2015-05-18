# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from .config import Config


class Paths(object):
  """Provides commonly used paths"""

  def __init__(self, config):
    """Generate paths to binary artifacts from a Config object."""
    self.src_root = os.path.abspath(os.path.join(__file__,
      os.pardir, os.pardir, os.pardir, os.pardir))
    self.mojo_dir = os.path.join(self.src_root, "mojo")
    self.adb_path = os.path.join(self.src_root, 'third_party', 'android_tools',
                                 'sdk', 'platform-tools', 'adb')

    self.build_dir = config.build_dir
    if self.build_dir is None:
      subdir = ""
      if config.target_os == Config.OS_ANDROID:
        subdir += "android_"
        if config.target_cpu != Config.ARCH_ARM:
          subdir += config.target_cpu + "_"
      elif config.target_os == Config.OS_CHROMEOS:
        subdir += "chromeos_"
      subdir += "Debug" if config.is_debug else "Release"
      if config.is_asan:
        subdir += "_asan"
      if not(config.is_debug) and config.dcheck_always_on:
        subdir += "_dcheck"
      self.build_dir = os.path.join(self.src_root, "out", subdir)

    self.mojo_runner = os.path.join(self.build_dir, "mojo_runner")
    if config.target_os == Config.OS_WINDOWS:
      self.mojo_runner += ".exe"
    if config.target_os == Config.OS_ANDROID:
      self.mojo_runner = os.path.join(self.build_dir, "apks", config.apk_name)

  def RelPath(self, path):
    """Returns the given path, relative to the current directory."""
    return os.path.relpath(path)

  def SrcRelPath(self, path):
    """Returns the given path, relative to self.src_root."""
    return os.path.relpath(path, self.src_root)
