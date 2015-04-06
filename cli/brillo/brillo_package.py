# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""brillo package: Create and use packages in bricks.

This command can be used for creating basic packages (ebuilds) in user bricks
and determining how they are built. The aim is not to contain ebuilds
management or hide them from the user, rather to help bootstrap and build an
elementary project more easily.
"""

from __future__ import print_function

import os

from chromite.cli import command
from chromite.lib import brick_lib
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import osutils
from chromite.lib import portage_util


# Ebuild template for source packages in user bricks.
_SOURCE_EBUILD_TEMPLATE = """EAPI="4"

CROS_WORKON_SRCPATH="%(src_path)s"

inherit cros-workon

DESCRIPTION="Package %(package_name)s of brick %(brick_name)s"
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


@command.CommandDecorator('package')
class PackageCommand(command.CliCommand):
  """Create and manage packages."""

  EPILOG = """
To create a source package in a brick:
  brillo package --create-source path/to/source foo-category/bar-package
To enable building a package from latest or stable ebuilds:
  brillo package --enable latest foo-category/bar-package
  brillo package --enable stable foo-category/bar-package
"""

  _GROUP_HAS_LATEST = '@has-latest'
  _GROUP_ONLY_LATEST = '@only-latest'

  def __init__(self, options):
    super(PackageCommand, self).__init__(options)
    self.brick = None

  @classmethod
  def AddParser(cls, parser):
    super(cls, PackageCommand).AddParser(parser)
    parser.add_argument('package_name', metavar='package',
                        help='Name of package (category/package). When using '
                        '--enable, %s applies to all packages that have a '
                        'latest ebuild; %s applies to packages that have only '
                        'a latest ebuild.' %
                        (cls._GROUP_HAS_LATEST, cls._GROUP_ONLY_LATEST))
    parser.add_argument('--brick',
                        help='The brick to use. Auto-detected by default.')
    parser.add_argument('--create-source', metavar='src_path',
                        help='Create package from code in specified path. Path '
                        'is relative to the brick\'s source root.')
    parser.add_argument('--enable', '-e', choices=('latest', 'stable'),
                        help='Build package(s) from latest/stable source.')
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
            os.path.isdir(os.path.join(self.brick.SourceDir(), src_path))):
      cros_build_lib.Die('Package source directory (%s) not found', src_path)

    # Do not clobber ebuild unless forced.
    ebuild_file = os.path.join(self.brick.OverlayDir(), pkg_category,
                               pkg_name, '%s-9999.ebuild' % pkg_name)
    if not self.options.force and os.path.exists(ebuild_file):
      cros_build_lib.Die('Package ebuild already exists')

    # Create the ebuild.
    ebuild_content = _SOURCE_EBUILD_TEMPLATE % {
        'src_path': src_path,
        'package_name': self.options.package_name,
        'brick_name': self.options.brick,
    }
    try:
      osutils.WriteFile(ebuild_file, ebuild_content, makedirs=True)
    except EnvironmentError as e:
      cros_build_lib.Die('Failed creating the package ebuild: %s', e)

    # Register category as needed.
    categories_file = os.path.join(self.brick.OverlayDir(), 'profiles',
                                   'categories')
    category_line = '%s\n' % pkg_category
    try:
      add_category = category_line not in open(categories_file).readlines()
    except IOError:
      add_category = True

    if add_category:
      osutils.WriteFile(categories_file, category_line, mode='a',
                        makedirs=True)

  def _EnableBuild(self):
    """Enable latest/stable build."""
    to_latest = self.options.enable == 'latest'
    cmd = ['cros_workon',
           'start' if to_latest else 'stop',
           '--brick=%s' % self.options.brick]

    if self.options.package_name == self._GROUP_HAS_LATEST:
      cmd.append('--all')
    elif self.options.package_name == self._GROUP_ONLY_LATEST:
      cmd.append('--workon_only')
    else:
      cmd.append(self.options.package_name)

    result = cros_build_lib.RunCommand(cmd, quiet=True, error_code_ok=True)
    if result.returncode:
      cros_build_lib.Die('Failed to %s latest build for %s:\n%s' %
                         ('enable' if to_latest else 'disable',
                          self.options.package_name, result.output))

  def _ReadOptions(self):
    """Process arguments and set variables, then freeze options."""
    if not self.options.brick:
      if not self.curr_brick_locator:
        cros_build_lib.Die('Brick not specified nor discovered')
      self.options.brick = self.curr_brick_locator
    try:
      self.brick = brick_lib.Brick(self.options.brick)
    except brick_lib.BrickNotFound:
      cros_build_lib.Die('Could not find brick %s' % self.options.brick)

    self.options.Freeze()

  def Run(self):
    """Dispatch the call to the right handler."""
    self._ReadOptions()
    commandline.RunInsideChroot(self, auto_detect_brick=True)
    if self.options.create_source:
      self._CreateSource()
    if self.options.enable:
      self._EnableBuild()
