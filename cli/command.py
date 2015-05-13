# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module that contains meta-logic related to CLI commands.

CLI commands are the subcommands available to the user, such as "deploy" in
`cros deploy` or "shell" in `brillo shell`.

This module contains two important definitions used by all commands:
  CliCommand: The parent class of all CLI commands.
  CommandDecorator: Decorator that must be used to ensure that the command shows
    up in |_commands| and is discoverable.

Commands can be either imported directly or looked up using this module's
ListCommands() function.
"""

from __future__ import print_function

import glob
import os
import sys

from chromite.cbuildbot import constants
from chromite.lib import brick_lib
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_import
from chromite.lib import cros_logging as logging
from chromite.lib import osutils
from chromite.lib import workspace_lib


_commands = dict()


def SetupFileLogger(filename='brillo.log', log_level=logging.DEBUG):
  """Store log messages to a file.

  In case of an error, this file can be made visible to the user.
  """
  workspace_path = workspace_lib.WorkspacePath()
  if workspace_path is None:
    return
  path = os.path.join(workspace_path, workspace_lib.WORKSPACE_LOGS_DIR,
                      filename)
  osutils.Touch(path, makedirs=True)
  logger = logging.getLogger()
  fh = logging.FileHandler(path, mode='w')
  fh.setLevel(log_level)
  fh.setFormatter(
      logging.Formatter(fmt=constants.LOGGER_FMT,
                        datefmt=constants.LOGGER_DATE_FMT))
  logger.addHandler(fh)


def UseProgressBar():
  """Determine whether the progress bar is to be used or not.

  We only want the progress bar to display for the brillo commands which operate
  at logging level NOTICE. If the user wants to see the noisy output, then they
  can execute the command at logging level INFO or DEBUG.
  """
  return logging.getLogger().getEffectiveLevel() == logging.NOTICE


def GetToolset():
  """Return the CLI toolset invoked by the user.

  For example, if the user is executing `cros flash`, this will return 'cros'.

  This won't work for unittests so if a certain toolset must be loaded for
  a unittest this should be mocked out to return the desired string.
  """
  return os.path.basename(sys.argv[0])


def _FindModules(subdir_path, toolset):
  """Returns a list of all the relevant python modules in |subdir_path|.

  The modules returned are based on |toolset|, so if |toolset| is 'cros'
  then cros_xxx.py modules will be found.

  Args:
    subdir_path: directory (string) to search for modules in.
    toolset: toolset (string) to find.

  Returns:
    List of filenames (strings).
  """
  modules = []
  for file_path in glob.glob(os.path.join(subdir_path, toolset + '_*.py')):
    if not file_path.endswith('_unittest.py'):
      modules.append(file_path)
  return modules


def _ImportCommands(toolset):
  """Directly imports all |toolset| python modules.

  This method imports the |toolset|_[!unittest] modules which may contain
  commands. When these modules are loaded, declared commands (those that use
  CommandDecorator) will automatically get added to |_commands|.

  Args:
    toolset: toolset (string) to import.
  """
  subdir_path = os.path.join(os.path.dirname(__file__), toolset)
  for file_path in _FindModules(subdir_path, toolset):
    file_name = os.path.basename(file_path)
    mod_name = os.path.splitext(file_name)[0]
    cros_import.ImportModule(('chromite', 'cli', toolset, mod_name))


def ListCommands(toolset=None):
  """Return a dictionary mapping command names to classes.

  Args:
    toolset: toolset (string) to list, None to determine from the commandline.

  Returns:
    A dictionary mapping names (strings) to commands (classes).
  """
  _ImportCommands(toolset or GetToolset())
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
    parser.set_defaults(command_class=cls)

  @classmethod
  def AddDeviceArgument(cls, parser, schemes=commandline.DEVICE_SCHEME_SSH,
                        optional=None):
    """Add a device argument to the parser.

    This has a few advantages over adding a device argument directly:
      - Standardizes the device --help message for all tools.
      - May allow `brillo` and `cros` to use the same source.

    The device argument is normally positional in cros but optional in
    brillo. If that is the only difference between a cros and brillo
    tool, this function allows the same source be shared for both.

    Args:
      parser: The parser to add the device argument to.
      schemes: List of device schemes or single scheme to allow.
      optional: Whether the device is an optional or positional
        argument; None to auto-determine based on toolset.
    """
    if optional is None:
      optional = (GetToolset() == 'brillo')
    help_strings = []
    schemes = list(cros_build_lib.iflatten_instance(schemes))
    if commandline.DEVICE_SCHEME_SSH in schemes:
      help_strings.append('Target a device with [user@]hostname[:port].')
    if commandline.DEVICE_SCHEME_USB in schemes:
      help_strings.append('Target removable media with usb://[path].')
    if commandline.DEVICE_SCHEME_FILE in schemes:
      help_strings.append('Target a local file with file://path.')
    parser.add_argument('--device' if optional else 'device',
                        type=commandline.DeviceParser(schemes),
                        help=' '.join(help_strings))

  def Run(self):
    """The command to run."""
    raise NotImplementedError()
