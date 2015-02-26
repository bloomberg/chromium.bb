# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""cros package: Create and use packages in projects.

This command can be used for creating basic packages (ebuilds) in user projects
and determining how they are built. The aim is not to contain ebuilds
management or hide them from the user, rather to help bootstrap and build an
elementary project more easily.
"""

from __future__ import print_function

import os

from chromite import cros
from chromite.lib import cros_build_lib
from chromite.lib import osutils
from chromite.lib import portage_util
from chromite.lib import project


# Ebuild template for source packages in user projects.
_SOURCE_EBUILD_TEMPLATE = """EAPI="4"

CROS_WORKON_SRCPATH="%(src_path)s"

inherit cros-workon

DESCRIPTION="Package %(package_name)s of project %(project_name)s"
SRC_URI=""

# Please assign a proper license for your package. You may use a standard
# license (e.g. "BSD") or add a custom one. For documentation, see:
# https://devmanual.gentoo.org/eclass-reference/ebuild/index.html
LICENSE="None"
SLOT="0"
KEYWORDS="~*"

src_install() {
	# Pleaes add install commands for your package below. It is recommended
	# to use ebuild primitives like dobin, dosbin, dolib, and so on. For a
	# complete documentation of such promitives, see:
	# https://devmanual.gentoo.org/eclass-reference/ebuild/index.html
	# Once you are done, please remove the 'die' line below.
	die "Please add install commands; see instructions in the ebuild."
}
"""


@cros.CommandDecorator('package')
class PackageCommand(cros.CrosCommand):
  """Create and manage packages."""

  EPILOG = """
To create a source package in the current project:
  cros package --create-source path/to/source foo-category/bar-package
"""

  def __init__(self, options):
    cros.CrosCommand.__init__(self, options)
    self.project = None

  @classmethod
  def AddParser(cls, parser):
    super(cls, PackageCommand).AddParser(parser)
    parser.add_argument('package_name', metavar='package',
                        help='Name of package (category/package).')
    parser.add_argument('--project',
                        help='The project to use. Auto-detected by default.')
    parser.add_argument('--create-source', metavar='src_path',
                        help='Create package from code in specified path. Path '
                        'is relative to project source root.')
    parser.add_argument('--force', action='store_true',
                        help='Ignore checks, clobber everything.')

  def _CreateSource(self):
    """Create a source package."""
    src_path = self.options.create_source.rstrip(os.path.sep)

    # Extract the package category and name.
    pkg_attrs = portage_util.SplitCPV(self.options.package_name, strict=False)
    if not (pkg_attrs.category and pkg_attrs.package and not pkg_attrs.version):
      cros_build_lib.Die('Package name must be of the form category/package')
    pkg_category = pkg_attrs.category
    pkg_name = pkg_attrs.package

    # Check that the source directory exists.
    if not (self.options.force or
            os.path.isdir(os.path.join(self.project.SourceDir(), src_path))):
      cros_build_lib.Die('Package source directory (%s) not found', src_path)

    # Do not clobber ebuild unless forced.
    ebuild_file = os.path.join(self.project.OverlayDir(), pkg_category,
                               pkg_name, '%s-9999.ebuild' % pkg_name)
    if not self.options.force and os.path.exists(ebuild_file):
      cros_build_lib.Die('Package ebuild already exists')

    # Create the ebuild.
    ebuild_content = _SOURCE_EBUILD_TEMPLATE % {
        'src_path': src_path,
        'package_name': self.options.package_name,
        'project_name': self.options.project,
    }
    try:
      osutils.WriteFile(ebuild_file, ebuild_content, makedirs=True)
    except EnvironmentError as e:
      cros_build_lib.Die('Failed creating the package ebuild: %s', e)

    # Register category as needed.
    categories_file = os.path.join(self.project.OverlayDir(), 'profiles',
                                   'categories')
    category_line = '%s\n' % pkg_category
    try:
      add_category = category_line not in open(categories_file).readlines()
    except IOError:
      add_category = True

    if add_category:
      osutils.WriteFile(categories_file, category_line, mode='a',
                        makedirs=True)

  def _ReadOptions(self):
    """Process arguments and set variables, then freeze options."""
    if not self.options.project:
      self.options.project = self.curr_project_name
    self.project = project.FindProjectByName(self.options.project)
    if not self.project:
      cros_build_lib.Die('Could not find project')

    self.options.Freeze()

  def Run(self):
    """Dispatch the call to the right handler."""
    self._ReadOptions()
    self.RunInsideChroot(auto_detect_project=True)
    if self.options.create_source:
      self._CreateSource()
