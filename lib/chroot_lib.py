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


class Error(Exception):
  """Base chroot_lib error class."""


class ChrootError(Error):
  """An exception raised when something went wrong with a chroot object."""


class Chroot(object):
  """Chroot class."""

  def __init__(self,
               path=None,
               cache_dir=None,
               chrome_root=None,
               env=None,
               goma=None):
    # Strip trailing / if present for consistency.
    self._path = (path or constants.DEFAULT_CHROOT_PATH).rstrip('/')
    self._is_default_path = not bool(path)
    self._env = env
    self.goma = goma
    # String in proto are '' when not set, but testing and comparing is much
    # easier when the "unset" value is consistent, so do an explicit "or None".
    self.cache_dir = cache_dir or None
    self.chrome_root = chrome_root or None

  def __eq__(self, other):
    if self.__class__ is other.__class__:
      return (self.path == other.path and self.cache_dir == other.cache_dir
              and self.chrome_root == other.chrome_root
              and self.env == other.env)

    return NotImplemented

  def __hash__(self):
    return hash(self.path)

  @property
  def path(self):
    return self._path

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

  def chroot_path(self, path):
    """Turn an absolute path into a chroot relative path."""
    if not path.startswith(self.path + os.path.sep):
      raise ChrootError('Path not in chroot: %s' % path)
    return path[len(self.path):]

  def full_path(self, *args):
    """Turn a chroot-relative path into an absolute path."""
    return os.path.join(self.path, *[part.lstrip(os.sep) for part in args])

  def has_path(self, *args):
    """Check if a chroot-relative path exists inside the chroot."""
    return os.path.exists(self.full_path(*args))

  def get_enter_args(self):
    """Build the arguments to enter this chroot."""
    args = []

    # This check isn't strictly necessary, always passing the --chroot argument
    # is valid, but it's nice for cleaning up commands in logs.
    if not self._is_default_path:
      args.extend(['--chroot', self.path])
    if self.cache_dir:
      args.extend(['--cache-dir', self.cache_dir])
    if self.chrome_root:
      args.extend(['--chrome-root', self.chrome_root])
    if self.goma:
      args.extend([
          '--goma_dir', self.goma.linux_goma_dir,
          '--goma_client_json', self.goma.goma_client_json,
      ])

    return args

  @property
  def env(self):
    env = self._env.copy() if self._env else {}
    if self.goma:
      env.update(self.goma.GetChrootExtraEnv())

    return env
