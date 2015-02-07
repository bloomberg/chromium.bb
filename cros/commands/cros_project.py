# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""cros project: Create and manage projects."""

from __future__ import print_function

import os
import shutil

from chromite import cros
from chromite.cbuildbot import constants
from chromite.lib import cros_build_lib
from chromite.lib import osutils
from chromite.lib import project


@cros.CommandDecorator('project')
class ProjectCommand(cros.CrosCommand):
  """Create and manage projects."""

  EPILOG = """
To create a project based on gizmo:
  cros project create testproject --dependency gizmo
"""

  def __init__(self, options):
    cros.CrosCommand.__init__(self, options)
    self.project_name = options.project_name
    self.dependencies = options.dependencies

  @classmethod
  def AddParser(cls, parser):
    super(cls, ProjectCommand).AddParser(parser)
    subparser = parser.add_subparsers()
    create_parser = subparser.add_parser('create')
    create_parser.add_argument('project_name', help='Name of the project')
    create_parser.add_argument('-d', '--dependency', action='append',
                               dest='dependencies',
                               help='Add a dependency to this project.')
    create_parser.set_defaults(handler_func='Create', dependencies=[])

  def Create(self):
    """Create a new project.

    This will create project.conf and generate portage's configuration based on
    it.
    """
    project_dir = os.path.join(constants.SOURCE_ROOT, 'projects',
                               self.project_name)
    if os.path.exists(project_dir):
      cros_build_lib.Die('Directory %s already exists.' % project_dir)

    with osutils.TempDir() as tempdir:
      temp_project_dir = os.path.join(tempdir, 'directory')

      conf = {'name': self.project_name,
              'dependencies': [{'name': d} for d in self.dependencies]}

      project.Project(temp_project_dir, initial_config=conf)
      shutil.move(temp_project_dir, project_dir)

  def Run(self):
    """Dispatch the call to the right handler."""
    self.options.Freeze()
    getattr(self, self.options.handler_func)()
