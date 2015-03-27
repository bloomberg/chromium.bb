# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""cros chroot: Enter the chroot for the current build environment."""

from __future__ import print_function

import argparse

from chromite.cli import command
from chromite.lib import commandline
from chromite.lib import cros_build_lib


@command.CommandDecorator('chroot')
class ChrootCommand(command.CliCommand):
  """Enter the chroot."""

  # Override base class property to enable stats upload.
  upload_stats = True

  @classmethod
  def AddParser(cls, parser):
    """Adds a parser."""
    super(cls, ChrootCommand).AddParser(parser)
    parser.add_argument(
        'command', nargs=argparse.REMAINDER,
        help='(optional) Command to execute inside the chroot.')

  def Run(self):
    """Runs `cros chroot`."""
    self.options.Freeze()

    commandline.RunInsideChroot(self, auto_detect_brick=False)

    cmd = self.options.command

    # If -- was used to separate out the command from arguments, ignore it.
    if cmd and cmd[0] == '--':
      cmd = cmd[1:]

    # If there is no command, run bash.
    if not cmd:
      cmd = ['bash']

    result = cros_build_lib.RunCommand(cmd, print_cmd=False, error_code_ok=True,
                                       mute_output=False)
    return result.returncode
