# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""brillo blueprint: Create and manage blueprints."""

from __future__ import print_function

from chromite.cli import command
from chromite.lib import blueprint_lib
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging


@command.CommandDecorator('blueprint')
class BlueprintCommand(command.CliCommand):
  """Create blueprints."""

  EPILOG = """
To create a blueprint "foo" in workspace/blueprints/:
  brillo blueprint create foo

Specify a path to use a directory other than workspace/blueprints/:
  brillo blueprint create //my_blueprints/foo
  brillo blueprint create ./foo

You can configure the initial setup as well:
  brillo blueprint create foo --bsp <bsp> --brick <brick>

Multiple bricks can be specified by repeating -b/--brick:
  brillo blueprint create foo --bsp <bsp> -b <brick> -b <brick>
"""

  @classmethod
  def AddParser(cls, parser):
    super(cls, BlueprintCommand).AddParser(parser)
    subparser = parser.add_subparsers()
    create_parser = subparser.add_parser('create', epilog=cls.EPILOG)
    create_parser.add_argument(
        'blueprint', type='blueprint_path', help='Blueprint path/locator.')
    create_parser.add_argument(
        '--brick', '-b', type='brick_path', action='append', dest='bricks',
        help='Add an existing brick to the blueprint.', metavar='BRICK')
    create_parser.add_argument(
        '--bsp', type='bsp_path', help='Set the blueprint BSP.')
    create_parser.set_defaults(handler_func='Create')

  def Create(self):
    """Create a new blueprint."""
    if not self.options.bsp:
      logging.warning('No BSP was specified; blueprint will not build until a '
                      'valid BSP is set in %s.', self.options.blueprint)

    config = {blueprint_lib.BSP_FIELD: self.options.bsp,
              blueprint_lib.BRICKS_FIELD: self.options.bricks}
    blueprint_lib.Blueprint(self.options.blueprint, initial_config=config)

  def Run(self):
    """Dispatch the call to the right handler."""
    self.options.Freeze()
    try:
      getattr(self, self.options.handler_func)()
    except Exception as e:
      if self.options.debug:
        raise
      else:
        cros_build_lib.Die(e)
