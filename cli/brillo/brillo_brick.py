# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""brillo brick: Create and manage bricks."""

from __future__ import print_function

import os

from chromite.cli import command
from chromite.lib import brick_lib
from chromite.lib import cros_build_lib


@command.CommandDecorator('brick')
class BrickCommand(command.CliCommand):
  """Create and manage bricks."""

  EPILOG = """
To create a brick in foo/bar:
  brillo brick create foo/bar

All bricks can be referred to using a locator. This locator is either:
  * a relative path to the brick from the current working directory.
  * the relative path to the brick in the current workspace, prefixed with '//'.
  * board:<board name> for legacy, public board overlays.

To create a brick in ${workspace}/foo that depends on ${workspace}/bar:
  brillo brick create //foo --dependency //bar

To create a brick that depend on the gizmo overlay:
  brillo brick create //foo -d board:gizmo

Multiple dependencies can be specified by repeating -d/--dependency. The order
matters here. Dependencies should be ordered from the lowest priority to the
highest:
  brillo brick create //foo -d //bar -d //baz
"""

  def __init__(self, options):
    super(BrickCommand, self).__init__(options)
    self.brick = options.brick
    self.dependencies = options.dependencies

  @classmethod
  def AddParser(cls, parser):
    super(cls, BrickCommand).AddParser(parser)
    subparser = parser.add_subparsers()
    create_parser = subparser.add_parser('create')
    create_parser.add_argument('brick', help='The current brick locator.')
    create_parser.add_argument('-d', '--dependency', action='append',
                               dest='dependencies',
                               help='Add a dependency to this brick. This must '
                               'be a brick locator.')
    create_parser.set_defaults(handler_func='Create', dependencies=[])

  def Create(self):
    """Create a new brick.

    This will create config.json and generate portage's configuration based on
    it.
    """
    try:
      # TODO(bsimonnet): remove the name attribute once we do the stacking
      # without relying on it.
      conf = {'name': os.path.basename(self.brick),
              'dependencies': self.dependencies}
      brick_lib.Brick(self.brick, initial_config=conf)

    except brick_lib.BrickCreationFailed as e:
      cros_build_lib.Die('Brick creation failed: %s' % e)

  def Run(self):
    """Dispatch the call to the right handler."""
    self.options.Freeze()
    getattr(self, self.options.handler_func)()
