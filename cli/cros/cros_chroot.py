# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""cros chroot: Enter the chroot for the current build environment."""

from __future__ import print_function

import argparse

from chromite.cli import command
from chromite.lib import cros_build_lib
from chromite.lib import workspace_lib


@command.CommandDecorator('chroot')
class ChrootCommand(command.CliCommand):
  """Enter the chroot."""

  # Override base class property to enable stats upload.
  upload_stats = True

  def _SpecifyNewChrootLocation(self, chroot_dir):
    """Specify a new location for a workspace's chroot.

    Args:
      chroot_dir: Directory in which to specify the new chroot.
    """
    workspace_path = workspace_lib.WorkspacePath()

    if not workspace_path:
      cros_build_lib.Die('You must be in a workspace, to move its chroot.')

    # TODO(dgarrett): Validate chroot_dir, somehow.
    workspace_lib.SetChrootDir(workspace_path, chroot_dir)

  def _RunChrootCommand(self, cmd):
    """Run the specified command inside the chroot.

    Args:
      cmd: A list or tuple of strings to use as a command and its arguments.
           If empty, run 'bash'.

    Returns:
      The commands result code.
    """
    # If there is no command, run bash.
    if not cmd:
      cmd = ['bash']

    result = cros_build_lib.RunCommand(cmd, print_cmd=False, error_code_ok=True,
                                       mute_output=False,
                                       enter_chroot=True,
                                       chroot_args=['--log-level',
                                                    self.options.log_level])
    return result.returncode

  @classmethod
  def AddParser(cls, parser):
    """Adds a parser."""
    super(cls, ChrootCommand).AddParser(parser)
    parser.add_argument(
        '--move', help='Specify new directory for workspace chroot.')
    parser.add_argument(
        'command', nargs=argparse.REMAINDER,
        help='(optional) Command to execute inside the chroot.')

  def Run(self):
    """Runs `cros chroot`."""
    self.options.Freeze()

    # Handle the special case of moving the chroot.
    if self.options.move:
      if self.options.command:
        cros_build_lib.Die(
            "You can't move a chroot, and use it at the same time.")

      self._SpecifyNewChrootLocation(self.options.move)
      return 0

    # Handle the standard case.
    cmd = self.options.command

    # If -- was used to separate out the command from arguments, ignore it.
    if cmd and cmd[0] == '--':
      cmd = cmd[1:]

    return self._RunChrootCommand(cmd)
