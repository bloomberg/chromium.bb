# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""cros build: Build the requested packages."""

from __future__ import print_function

from chromite.cli import command
from chromite.lib import blueprint_lib
from chromite.lib import brick_lib
from chromite.lib import chroot_util
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import parallel


@command.CommandDecorator('build')
class BuildCommand(command.CliCommand):
  """Build the requested packages."""

  _BAD_DEPEND_MSG = '\nemerge detected broken ebuilds. See error message above.'
  EPILOG = """
To update specified package and all dependencies:
  cros build --board=lumpy power_manager
  cros build --host cros-devutils

To just build a single package:
  cros build --board=lumpy --no-deps power_manager
"""

  def __init__(self, options):
    super(BuildCommand, self).__init__(options)
    self.chroot_update = options.chroot_update and options.deps
    if options.chroot_update and not options.deps:
      logging.debug('Skipping chroot update due to --nodeps')
    self.build_pkgs = None
    self.host = False
    self.board = None
    self.brick = None

    if self.options.host:
      self.host = True
    elif self.options.board:
      self.board = self.options.board
    elif self.options.blueprint:
      bricks = blueprint_lib.Blueprint(self.options.blueprint).GetBricks()
      # TODO(bsimonnet): Support multiple bricks per blueprint (brbug.com/635).
      if len(bricks) != 1:
        cros_build_lib.Die('Blueprint contains multiple bricks, but we can '
                           'only build a single brick at a time')
      self.brick = brick_lib.Brick(bricks[0])
    elif self.options.brick or self.curr_brick_locator:
      self.brick = brick_lib.Brick(self.options.brick
                                   or self.curr_brick_locator)
    else:
      # If nothing is explicitly set, use the default board.
      self.board = cros_build_lib.GetDefaultBoard()

    if self.brick:
      self.board = self.brick.FriendlyName()

  @classmethod
  def AddParser(cls, parser):
    super(cls, BuildCommand).AddParser(parser)
    target = parser.add_mutually_exclusive_group()
    target.add_argument('--board', help='The board to build packages for.')
    target.add_argument('--brick', help='The brick to build packages for.')
    target.add_argument('--blueprint',
                        help='The blueprint to build packages for.')
    target.add_argument('--host', help='Build packages for the chroot itself.',
                        default=False, action='store_true')
    parser.add_argument('--no-binary', help="Don't use binary packages.",
                        default=True, dest='binary', action='store_false')
    parser.add_argument('--no-chroot-update', help="Don't update chroot.",
                        default=True, dest='chroot_update',
                        action='store_false')
    parser.add_argument('--init-only', action='store_true',
                        help="Initialize build environment but don't build "
                        "anything.")
    deps = parser.add_mutually_exclusive_group()
    deps.add_argument('--no-deps', help="Don't update dependencies.",
                      default=True, dest='deps', action='store_false')
    deps.add_argument('--rebuild-deps', default=False, action='store_true',
                      help='Automatically rebuild dependencies.')
    parser.add_argument('packages',
                        help='Packages to build. If no packages listed, uses '
                        'the current brick main package.',
                        nargs='*')

    # Advanced options.
    advanced = parser.add_argument_group('Advanced options')
    advanced.add_argument('--jobs', default=None, type=int,
                          help='Maximium job count to run in parallel '
                               '(uses all available cores by default).')

    # Legacy options, for backward compatibiltiy.
    legacy = parser.add_argument_group('Options for backward compatibility')
    legacy.add_argument('--norebuild', default=True, dest='rebuild_deps',
                        action='store_false', help='Inverse of --rebuild-deps.')

  def _CheckDependencies(self):
    """Verify emerge dependencies.

    Verify all board packages can be emerged from scratch, without any
    backtracking. This ensures that no updates are skipped by Portage due to
    the fallback behavior enabled by the backtrack option, and helps catch
    cases where Portage skips an update due to a typo in the ebuild.

    Only print the output if this step fails or if we're in debug mode.
    """
    if self.options.deps and not self.host:
      cmd = chroot_util.GetEmergeCommand(board=self.board)
      cmd += ['-pe', '--backtrack=0'] + self.build_pkgs
      try:
        cros_build_lib.RunCommand(cmd, combine_stdout_stderr=True,
                                  debug_level=logging.DEBUG)
      except cros_build_lib.RunCommandError as ex:
        ex.msg += self._BAD_DEPEND_MSG
        raise

  def _Build(self):
    """Update the chroot, then merge the requested packages."""
    if self.chroot_update and self.host:
      chroot_util.UpdateChroot()

    chroot_util.Emerge(self.build_pkgs, brick=self.brick, board=self.board,
                       host=self.host, with_deps=self.options.deps,
                       rebuild_deps=self.options.rebuild_deps,
                       use_binary=self.options.binary, jobs=self.options.jobs,
                       debug_output=(self.options.log_level.lower() == 'debug'))

  def Run(self):
    """Run cros build."""
    if not self.host:
      if not (self.board or self.brick):
        cros_build_lib.Die('You did not specify a board/brick to build for. '
                           'You need to be in a brick directory or set '
                           '--board/--brick/--host')

      if self.brick and self.brick.legacy:
        cros_build_lib.Die('--brick should not be used with board names. Use '
                           '--board=%s instead.' % self.brick.config['name'])

    commandline.RunInsideChroot(self, auto_detect_brick=True)

    self.build_pkgs = self.options.packages or (self.brick and
                                                self.brick.MainPackages())
    if not (self.build_pkgs or self.options.init_only):
      cros_build_lib.Die('No packages found, nothing to build.')

    # Set up board if not building for host.
    if not self.host:
      chroot_util.SetupBoard(brick=self.brick, board=self.board,
                             update_chroot=self.chroot_update,
                             use_binary=self.options.binary)

    if not self.options.init_only:
      parallel.RunParallelSteps([self._CheckDependencies, self._Build])
