# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""cros brick: Create and manage bricks."""

from __future__ import print_function

import os
import shutil

from chromite import cros
from chromite.cbuildbot import constants
from chromite.lib import brick_lib
from chromite.lib import cros_build_lib
from chromite.lib import osutils


@cros.CommandDecorator('brick')
class BrickCommand(cros.CrosCommand):
  """Create and manage bricks."""

  EPILOG = """
To create a brick that depends on gizmo:
  cros brick create testbrick --dependency gizmo
"""

  def __init__(self, options):
    cros.CrosCommand.__init__(self, options)
    self.brick_name = options.brick_name
    self.dependencies = options.dependencies

  @classmethod
  def AddParser(cls, parser):
    super(cls, BrickCommand).AddParser(parser)
    subparser = parser.add_subparsers()
    create_parser = subparser.add_parser('create')
    # TODO(bsimonnet): brick_name should allow unambiguous name (//brick/foo).
    create_parser.add_argument('brick_name', help='Name of the brick')
    create_parser.add_argument('-d', '--dependency', action='append',
                               dest='dependencies',
                               help='Add a dependency to this brick.')
    create_parser.set_defaults(handler_func='Create', dependencies=[])

  def Create(self):
    """Create a new brick.

    This will create config.json and generate portage's configuration based on
    it.
    """
    brick_dir = os.path.join(constants.SOURCE_ROOT, 'projects', self.brick_name)
    if os.path.exists(brick_dir):
      cros_build_lib.Die('Directory %s already exists.' % brick_dir)

    with osutils.TempDir() as tempdir:
      temp_brick_dir = os.path.join(tempdir, 'directory')

      conf = {'name': self.brick_name,
              'dependencies': [{'name': d} for d in self.dependencies]}

      brick_lib.Brick(temp_brick_dir, initial_config=conf)
      shutil.move(temp_brick_dir, brick_dir)

  def Run(self):
    """Dispatch the call to the right handler."""
    self.options.Freeze()
    getattr(self, self.options.handler_func)()
