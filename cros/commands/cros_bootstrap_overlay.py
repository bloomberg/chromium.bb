# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""cros bootstrap-overlay: Create an overlay based on an existing one."""

from __future__ import print_function

from datetime import date
from distutils import dir_util
import logging
import os

from chromite.cbuildbot import constants
from chromite.lib import cros_build_lib, osutils
from chromite import cros


BSP_VIRTUAL_TEMPLATE = \
"""# Copyright %(year)s The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2
EAPI=4

DESCRIPTION="Board specific definition for %(board)s"
HOMEPAGE="http://dev.chromium.org/"

LICENSE="BSD-Google"
SLOT="0"
KEYWORDS="*"
IUSE=""

RDEPEND="chromeos-base/chromeos-bsp-%(board)s"
"""

BSP_IMPL_TEMPLATE = \
"""# Copyright %(year)s The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=4

DESCRIPTION="%(board)s bsp"

LICENSE="BSD-Google"
SLOT="0"
KEYWORDS="*"
IUSE=""

RDEPEND="
	%(app_atom)s
"
"""


def CreateBsp(overlay_name, overlay_path, app_atom):
  """Create virtual/bsp and its implementation in overlay_name.

  Args:
    overlay_name: name of the overlay (board name).
    overlay_path: path to the overlay.
    app_atom: target atom for the app.
  """
  bsp_virtual = os.path.join(overlay_path, 'virtual', 'chromeos-bsp',
                             'chromeos-bsp-2.ebuild')

  osutils.WriteFile(bsp_virtual,
                    BSP_VIRTUAL_TEMPLATE % {'year': date.today().year,
                                            'board': overlay_name},
                    makedirs=True)

  bsp_impl = os.path.join(overlay_path,
                          'chromeos-base',
                          'chromeos-bsp-%s' % overlay_name,
                          'chromeos-bsp-%s-0.0.1.ebuild' % overlay_name)

  osutils.WriteFile(bsp_impl,
                    BSP_IMPL_TEMPLATE % {'year':date.today().year,
                                         'board': overlay_name,
                                         'app_atom': app_atom},
                    makedirs=True)

  os.symlink(os.path.basename(bsp_impl),
             os.path.join(os.path.dirname(bsp_impl),
                          'chromeos-bsp-%s-0.0.1-r1.ebuild' % overlay_name))


def GetBspEbuild(overlay_path, board_name):
  """Return the path to the ebuild implementing chromeos-bsp.

  This assumes that the bsp implementation package is called chromeos-bsp-$BOARD
  where $BOARD is the board name.

  Args:
    overlay_path: path of the overlay.
    board_name: name of the overlay.

  Returns:
    The path to the ebuild implementing chromeos-bsp if it exists,
    None otherwise.
  """

  bsp_name = 'chromeos-bsp-%s' % board_name
  bsp_impl_dir = os.path.join(overlay_path, 'chromeos-base', bsp_name)

  for filename in os.listdir(bsp_impl_dir):
    filepath = os.path.join(bsp_impl_dir, filename)

    # Assume the first regular file whose name matches
    # chromeos-bsp-$BOARD.*.ebuild is the bsp ebuild.
    if filename.startswith(bsp_name) and filename.endswith(".ebuild") and \
        os.path.isfile(filepath) and not os.path.islink(filepath):
      return filepath

  return None


def UpdateCrosBoard(board_name):
  """Add the board name to the list of board names in cros-board.eclass.

  Note: This will break the ordering of the list but this code is simpler and
  less error prone.

  Args:
    board_name: name to add in cros-board.eclass.
  """
  cros_board = os.path.join(constants.SOURCE_ROOT, 'src', 'third_party',
                            'chromiumos-overlay', 'eclass',
                            'cros-board.eclass')

  content = osutils.ReadFile(cros_board)
  osutils.WriteFile(cros_board,
                    content.replace('ALL_BOARDS=(',
                                    'ALL_BOARDS=(\n\t%s' % board_name))


def UpdateLayout(overlay_dir, repo_name):
  """Set the correct repo name field in metadata/layout.conf.

  Args:
    overlay_dir: path to the overlay.
    repo_name: name of the repo.
  """
  layout_conf_path = os.path.join(overlay_dir, 'metadata', 'layout.conf')
  layout_conf = osutils.ReadFile(layout_conf_path).split('\n')

  layout_conf = [line for line in layout_conf \
                 if not line.startswith('repo-name')]
  layout_conf.append('repo-name = %s' % repo_name)

  osutils.WriteFile(layout_conf_path, '\n'.join(layout_conf))


def RemoveLegacyRepoName(overlay_dir):
  """Remove the file profiles/repo_name if it exists.

  Args:
    overlay_dir: path to the overlay.
  """
  repo_name_file = os.path.join(overlay_dir, 'profiles', 'repo_name')

  if os.path.isfile(repo_name_file):
    logging.warn('Replacing deprecated repo_name by layout.conf.')
    os.remove(repo_name_file)


@cros.CommandDecorator('bootstrap-overlay')
class BootstrapOverlayCommand(cros.CrosCommand):
  """Create a new overlay based on an existing one."""

  EPILOG = """
To create new overlay from an existing one:
  cros bootstrap-overlay gizmo myboard

To bootstrap an overlay and copy an example app:
  cros bootstrap-overlay gizmo myboard --app=helloworld

which is equivalent to:
  cros bootstrap-overlay gizmo myboard --app=~/trunk/src/overlays/helloworld
"""

  def __init__(self, options):
    cros.CrosCommand.__init__(self, options)
    self.app_dir = None
    self.app_atom = None
    self.seed_dir = None
    self.overlay_dir = None

  @classmethod
  def AddParser(cls, parser):
    super(cls, BootstrapOverlayCommand).AddParser(parser)
    default_board = cros_build_lib.GetDefaultBoard()
    parser.add_argument('seed', help='Name of the overlay to use as seed',
                        default=default_board)
    parser.add_argument('board', help='Name of the new overlay')
    parser.add_argument('--app', help='App to install in the new overlay. '
                        'This can be the name of a directory in src/overlays '
                        'or the path to the app.',
                        default=None)

  def _ValidateOptions(self):
    overlays = os.path.join(constants.SOURCE_ROOT, 'src', 'overlays')

    # The destination overlay must not exist.
    self.overlay_dir = os.path.join(overlays, 'overlay-' + self.options.board)
    if os.path.isdir(self.overlay_dir):
      cros_build_lib.Die('The overlay directory %s already exists.' %
                         self.overlay_dir)

    # The seed overlay must exist.
    self.seed_dir = os.path.join(overlays, 'overlay-' + self.options.seed)
    if not os.path.isdir(self.seed_dir):
      cros_build_lib.Die('The seed overlay %s could not be found.'
                         % self.seed_dir)

    # The app must be:
    # * the name of a directory in src/overlay.
    # * a path to an existing directory.
    if self.options.app:
      if os.path.isdir(self.options.app):
        self.app_dir = self.options.app
        self.app_atom = 'app-misc/' + os.path.basename(self.app_dir.rstrip('/'))
      elif os.path.isdir(os.path.join(overlays, self.options.app)):
        self.app_dir = os.path.join(overlays, self.options.app)
        self.app_atom = 'app-misc/' + self.options.app
      else:
        cros_build_lib.Die('app %(app_name)s invalid. The app name must be'
                           'a folder in %(overlay) or a path to a directory.'
                           % {'app_name': self.options.app,
                              'overlay': overlays})

  def _InstallApp(self, destination):
    """Install the app in |destination|.

    Args:
      destination: destination directory.
    """
    dir_util.copy_tree(self.app_dir, destination, preserve_symlinks=True)

    # Add the main ebuild to the bsp.
    bsp_file = GetBspEbuild(destination, self.options.seed)

    if bsp_file is not None and os.path.isfile(bsp_file):
      osutils.WriteFile(bsp_file,
                        '\nRDEPEND="${RDEPEND} %s"' % self.app_atom,
                        mode='a')
    else:
      logging.info('No bsp package was found, creating one from scratch.')
      CreateBsp(self.options.board, destination, self.app_atom)

  def Run(self):
    """Run cros bootstrap-overlay."""

    self.options.Freeze()
    self._ValidateOptions()
    with osutils.TempDir() as tmp_overlay:
      dir_util.copy_tree(self.seed_dir, tmp_overlay, preserve_symlinks=True)

      RemoveLegacyRepoName(tmp_overlay)
      UpdateLayout(tmp_overlay, self.options.board)

      if self.app_dir:
        self._InstallApp(tmp_overlay)

      # TODO(bsimonnet): remove this once the dependency on cros-board.eclass
      # is removed (http://crbug.com/407731).
      UpdateCrosBoard(self.options.board)

      dir_util.copy_tree(tmp_overlay, self.overlay_dir, preserve_symlinks=True)
