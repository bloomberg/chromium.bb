# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chroot class.

This is currently a very sparse class, but there's a significant amount of
functionality that can eventually be centralized here.
"""

from __future__ import print_function

from chromite.lib import constants


class Chroot(object):
  """Chroot class."""

  def __init__(self, path=None, cache_dir=None):
    self.path = path or constants.DEFAULT_CHROOT_PATH
    self._is_default_path = path is not None
    self.cache_dir = cache_dir

  def GetEnterArgs(self):
    """Build the arguments to enter this chroot."""
    args = []

    # This check isn't strictly necessary, always passing the --chroot argument
    # is valid, but it's nice for cleaning up commands in logs.
    if not self._is_default_path:
      args.extend(['--chroot', self.path])
    if self.cache_dir:
      args.extend(['--cache-dir', self.cache_dir])

    return args
