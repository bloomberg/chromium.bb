# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module that contains meta-logic related to CLI commands.

This module contains two important definitions used by all commands:
  CliCommand: The parent class of all CLI commands.
  CommandDecorator: Decorator that must be used to ensure that the command shows
    up in |_commands| and is discoverable.

Commands can be either imported directly or looked up using this module's
ListCommands() function.
"""

from __future__ import print_function

import importlib
import os

from chromite.lib import constants
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging


# Paths for finding and importing subcommand modules.
_SUBCOMMAND_MODULE_DIRECTORY = os.path.join(os.path.dirname(__file__), 'cros')
_SUBCOMMAND_MODULE_PREFIX = 'cros_'


_commands = dict()


def UseProgressBar():
  """Determine whether the progress bar is to be used or not.

  We only want the progress bar to display for the brillo commands which operate
  at logging level NOTICE. If the user wants to see the noisy output, then they
  can execute the command at logging level INFO or DEBUG.
  """
  return logging.getLogger().getEffectiveLevel() == logging.NOTICE


def ImportCommand(name):
  """Directly import the specified subcommand.

  This method imports the module which must contain the single subcommand.  When
  the module is loaded, the declared command (those that use CommandDecorator)
  will automatically get added to |_commands|.

  Args:
    name: The subcommand to load.

  Returns:
    A reference to the subcommand class.
  """
  module_path = os.path.join(_SUBCOMMAND_MODULE_DIRECTORY,
                             'cros_%s' % (name.replace('-', '_'),))
  import_path = os.path.relpath(os.path.realpath(module_path),
                                os.path.dirname(constants.CHROMITE_DIR))
  module_parts = import_path.split(os.path.sep)
  importlib.import_module('.'.join(module_parts))
  return _commands[name]


def ListCommands():
  """Return the set of available subcommands.

  We assume that there is a direct one-to-one relationship between the module
  name on disk and the command that module implements.  We assume this as a
  performance requirement (to avoid importing every subcommand every time even
  though we'd only ever run a single one), and to avoid 3rd party module usage
  in one subcommand breaking all other subcommands (not a great solution).
  """
  # Filenames use underscores due to python naming limitations, but subcommands
  # use dashes as they're easier for humans to type.
  # Strip off the leading "cros_" and the trailing ".py".
  return set(x[5:-3].replace('_', '-')
             for x in os.listdir(_SUBCOMMAND_MODULE_DIRECTORY)
             if (x.startswith(_SUBCOMMAND_MODULE_PREFIX) and x.endswith('.py')
                 and not x.endswith('_unittest.py')))


class InvalidCommandError(Exception):
  """Error that occurs when command class fails sanity checks."""


def CommandDecorator(command_name):
  """Decorator that sanity checks and adds class to list of usable commands."""

  def InnerCommandDecorator(original_class):
    """Inner Decorator that actually wraps the class."""
    if not hasattr(original_class, '__doc__'):
      raise InvalidCommandError('All handlers must have docstrings: %s' %
                                original_class)

    if not issubclass(original_class, CliCommand):
      raise InvalidCommandError('All Commands must derive from CliCommand: %s' %
                                original_class)

    _commands[command_name] = original_class
    original_class.command_name = command_name

    return original_class

  return InnerCommandDecorator


class CliCommand(object):
  """All CLI commands must derive from this class.

  This class provides the abstract interface for all CLI commands. When
  designing a new command, you must sub-class from this class and use the
  CommandDecorator decorator. You must specify a class docstring as that will be
  used as the usage for the sub-command.

  In addition your command should implement AddParser which is passed in a
  parser that you can add your own custom arguments. See argparse for more
  information.
  """
  # Indicates whether command uses cache related commandline options.
  use_caching_options = False

  def __init__(self, options):
    self.options = options

  @classmethod
  def AddParser(cls, parser):
    """Add arguments for this command to the parser."""
    parser.set_defaults(command_class=cls)

  @classmethod
  def AddDeviceArgument(cls, parser, schemes=commandline.DEVICE_SCHEME_SSH):
    """Add a device argument to the parser.

    This standardizes the help message across all subcommands.

    Args:
      parser: The parser to add the device argument to.
      schemes: List of device schemes or single scheme to allow.
    """
    help_strings = []
    schemes = list(cros_build_lib.iflatten_instance(schemes))
    if commandline.DEVICE_SCHEME_SSH in schemes:
      help_strings.append('Target a device with [user@]hostname[:port]. '
                          'IPv4/IPv6 addresses are allowed, but IPv6 must '
                          'use brackets (e.g. [::1]).')
    if commandline.DEVICE_SCHEME_USB in schemes:
      help_strings.append('Target removable media with usb://[path].')
    if commandline.DEVICE_SCHEME_SERVO in schemes:
      help_strings.append('Target a servo by port or serial number with '
                          'servo:port[:port] or servo:serial:serial-number. '
                          'e.g. servo:port:1234 or servo:serial:C1230024192.')
    if commandline.DEVICE_SCHEME_FILE in schemes:
      help_strings.append('Target a local file with file://path.')
    parser.add_argument('device',
                        type=commandline.DeviceParser(schemes),
                        help=' '.join(help_strings))

  def Run(self):
    """The command to run."""
    raise NotImplementedError()
