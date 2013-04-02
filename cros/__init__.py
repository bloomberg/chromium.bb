# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module that contains meta-logic related to Cros Commands.

This module contains two important definitions used by all commands.

  CrosCommand: The parent class of all cros commands.
  CommandDecorator: Decorator that must be used to ensure that the command shows
    up in _commands and is discoverable by cros.
"""


_commands = dict()


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

  def __init__(self, options):
    self.options = options

  @classmethod
  def AddParser(cls, parser):
    """Add arguments for this command to the parser."""
    parser.set_defaults(cros_class=cls)

  def Run(self):
    """The command to run."""
    raise NotImplementedError()
