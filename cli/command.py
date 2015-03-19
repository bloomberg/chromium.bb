# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module that contains meta-logic related to Cros Commands.

This module contains two important definitions used by all commands:
  CrosCommand: The parent class of all cros commands.
  CommandDecorator: Decorator that must be used to ensure that the command shows
    up in _commands and is discoverable by cros.

Commands can be either imported directly or looked up using this module's
ListCommands() function.
"""

from __future__ import print_function

import glob
import os

from chromite.lib import brick_lib
from chromite.lib import cros_import


_commands = dict()


def _FindModules(subdir_path):
  """Returns a list of all the relevant python modules in |sub_dir_path|"""
  # We only load cros_[!unittest] modules.
  modules = []
  for file_path in glob.glob(subdir_path + '/cros_*.py'):
    if not file_path.endswith('_unittest.py'):
      modules.append(file_path)

  return modules


def _ImportCommands():
  """Directly imports all cros_[!unittest] python modules.

  This method imports the cros_[!unittest] modules which may contain
  commands. When these modules are loaded, declared commands (those that use
  the cros.CommandDecorator) will automatically get added to _commands.
  """
  subdir_path = os.path.join(os.path.dirname(__file__), 'cros')
  for file_path in _FindModules(subdir_path):
    file_name = os.path.basename(file_path)
    mod_name = os.path.splitext(file_name)[0]
    cros_import.ImportModule(('chromite', 'cli', 'cros', mod_name))


def ListCommands():
  """Return a dictionary mapping command names to classes."""
  _ImportCommands()
  return _commands.copy()


class InvalidCommandError(Exception):
  """Error that occurs when command class fails sanity checks."""
  pass


def CommandDecorator(command_name):
  """Decorator that sanity checks and adds class to list of usable commands."""

  def InnerCommandDecorator(original_class):
    """"Inner Decorator that actually wraps the class."""
    if not hasattr(original_class, '__doc__'):
      raise InvalidCommandError('All handlers must have docstrings: %s' %
                                original_class)

    if not issubclass(original_class, CrosCommand):
      raise InvalidCommandError('All Commands must derive from CrosCommand: '
                                '%s' % original_class)

    _commands[command_name] = original_class
    original_class.command_name = command_name

    return original_class

  return InnerCommandDecorator


class CrosCommand(object):
  """All CrosCommands must derive from this class.

  This class provides the abstract interface for all Cros Commands. When
  designing a new command, you must sub-class from this class and use the
  CommandDecorator decorator. You must specify a class docstring as that will be
  used as the usage for the sub-command.

  In addition your command should implement AddParser which is passed in a
  parser that you can add your own custom arguments. See argparse for more
  information.
  """
  # Indicates whether command stats should be uploaded for this command.
  # Override to enable command stats uploading.
  upload_stats = False
  # We set the default timeout to 1 second, to prevent overly long waits for
  # commands to complete.  From manual tests, stat uploads usually take
  # between 0.35s-0.45s in MTV.
  upload_stats_timeout = 1

  # Indicates whether command uses cache related commandline options.
  use_caching_options = False

  def __init__(self, options):
    self.options = options
    brick = brick_lib.FindBrickInPath()
    self.curr_brick_locator = brick.brick_locator if brick else None

  @classmethod
  def AddParser(cls, parser):
    """Add arguments for this command to the parser."""
    parser.set_defaults(cros_class=cls)

  def Run(self):
    """The command to run."""
    raise NotImplementedError()
