# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chroot class.

This is currently a very sparse class, but there's a significant amount of
functionality that can eventually be centralized here.
"""

from __future__ import print_function

import os

from chromite.lib import osutils

from chromite.lib import constants


class Chroot(object):
  """Chroot class."""

  def __init__(self, path=None, cache_dir=None, env=None):
    self.path = path or constants.DEFAULT_CHROOT_PATH
    self._is_default_path = not bool(path)
    self._env = env
    # cache_dir is often '' when not set, but testing and comparing is much
    # easier when the "unset" value is consistent, so do an explicit "or None".
    self.cache_dir = cache_dir or None

  def __eq__(self, other):
    if not isinstance(other, Chroot):
      return False
    return (self.path == other.path and self.cache_dir == other.cache_dir
            and self.env == other.env)

  def exists(self):
    """Checks if the chroot exists."""
    return os.path.exists(self.path)

  @property
  def tmp(self):
    """Get the chroot's tmp dir."""
    return os.path.join(self.path, 'tmp')

  def tempdir(self):
    """Get a TempDir in the chroot's tmp dir."""
    return osutils.TempDir(base_dir=self.tmp)

  def GetEnterArgs(self):
    """Build the arguments to enter this chroot."""
    return self.get_enter_args()

  def get_enter_args(self):
    """Build the arguments to enter this chroot."""
    args = []

    # This check isn't strictly necessary, always passing the --chroot argument
    # is valid, but it's nice for cleaning up commands in logs.
    if not self._is_default_path:
      args.extend(['--chroot', self.path])
    if self.cache_dir:
      args.extend(['--cache-dir', self.cache_dir])

    return args

  @property
  def env(self):
    return self._env.copy() if self._env else None
