# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Build target class and related functionality."""

from __future__ import print_function

import os


class Error(Exception):
  """Base module error class."""


class InvalidNameError(Error):
  """Error for invalid target name argument."""


class BuildTarget(object):
  """Class to handle the build target information."""

  def __init__(self, name, profile=None, build_root=None):
    """Build Target init.

    Args:
      name (str): The full name of the target.
      profile (str): The profile name.
      build_root (str): The path to the buildroot.
    """
    if not name:
      raise InvalidNameError('Name is required.')

    self._name = name
    self.board, _, self.variant = name.partition('_')
    self.profile = profile

    if build_root:
      self.root = os.path.normpath(build_root)
    else:
      self.root = GetDefaultSysrootPath(self.name)

  def __eq__(self, other):
    if self.__class__ is other.__class__:
      return (self.name == other.name and self.profile == other.profile and
              self.root == other.root)

    return NotImplemented

  def __hash__(self):
    return hash(self.name)

  def __str__(self):
    return self.name

  @property
  def name(self):
    return self._name

  def get_command(self, base_command):
    """Get the build target's variant of the given base command.

    We create wrappers for many scripts that handle the build target's
    arguments. Build the target-specific variant for such a command.
    e.g. emerge -> emerge-eve.

    TODO: Add optional validation the command exists.

    Args:
      base_command (str): The wrapped command.

    Returns:
      str: The build target's command wrapper.
    """
    return '%s-%s' % (base_command, self.name)

def GetDefaultSysrootPath(target_name):
  if target_name:
    return os.path.join('/build', target_name)
  else:
    raise InvalidNameError('Target name is required.')
