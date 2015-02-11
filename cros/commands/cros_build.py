# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""cros build: Build the requested packages."""

from __future__ import print_function

import os
import logging

from chromite.cbuildbot import constants
from chromite.lib import cros_build_lib
from chromite.lib import parallel
from chromite.lib import project
from chromite.scripts import cros_list_modified_packages as workon
from chromite.scripts import cros_setup_toolchains as toolchain
from chromite import cros

_HOST_PKGS = ('virtual/target-sdk', 'world',)


def GetToolchainPackages():
  """Get a list of host toolchain packages."""
  # Load crossdev cache first for faster performance.
  toolchain.Crossdev.Load(False)
  packages = toolchain.GetTargetPackages('host')
  return [toolchain.GetPortagePackage('host', x) for x in packages]

@cros.CommandDecorator('build')
class BuildCommand(cros.CrosCommand):
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
    cros.CrosCommand.__init__(self, options)
    self.chroot_update = options.chroot_update and options.deps
    if options.chroot_update and not options.deps:
      cros_build_lib.Debug('Skipping chroot update due to --nodeps')
    self.build_pkgs = None
    self.host = self.options.host
    self.board = self.options.board or self.curr_project_name
    self.project = project.FindProjectByName(self.board)

  @classmethod
  def AddParser(cls, parser):
    super(cls, BuildCommand).AddParser(parser)
    board = parser.add_mutually_exclusive_group()
    board.add_argument('--board', '--project',
                       help='The board or project to build packages for')
    board.add_argument('--host', help='Build packages for the chroot itself',
                       default=False, action='store_true')
    parser.add_argument('--no-binary', help="Don't use binary packages",
                        default=True, dest='binary', action='store_false')
    parser.add_argument('--no-chroot-update', help="Don't update chroot",
                        default=True, dest='chroot_update',
                        action='store_false')
    deps = parser.add_mutually_exclusive_group()
    deps.add_argument('--no-deps', help="Don't update dependencies",
                      default=True, dest='deps', action='store_false')
    deps.add_argument('--rebuild-deps', default=False, action='store_true',
                      help='Automatically rebuild dependencies')
    parser.add_argument('packages',
                        help='Packages to build. If no packages listed, uses '
                        'the current project main package.',
                        nargs='*')

    # Advanced options.
    advanced = parser.add_argument_group('Advanced options')
    advanced.add_argument('--nofast', help='Disable parallel emerge.',
                          default=True, action='store_false', dest='fast')
    advanced.add_argument('--jobs', default=None, type=int,
                          help='Maximium job count to run in parallel '
                               '(Default: Use all available cores)')
    advanced.add_argument('--norebuild', default=True, dest='rebuild_deps',
                          action='store_false',
                          help='Don\'t automatically rebuild dependencies.')

  def _CheckDependencies(self):
    """Verify emerge dependencies.

    Verify all board packages can be emerged from scratch, without any
    backtracking. This ensures that no updates are skipped by Portage due to
    the fallback behavior enabled by the backtrack option, and helps catch
    cases where Portage skips an update due to a typo in the ebuild.

    Only print the output if this step fails or if we're in debug mode.
    """
    if self.options.deps and not self.host:
      cmd = self._GetEmergeCommand(self.board)
      cmd += ['-pe', '--backtrack=0'] + self.build_pkgs
      try:
        cros_build_lib.RunCommand(cmd, combine_stdout_stderr=True,
                                  debug_level=logging.DEBUG)
      except cros_build_lib.RunCommandError as ex:
        ex.msg += self._BAD_DEPEND_MSG
        raise

  def _GetEmergeCommand(self, board):
    if self.options.fast:
      cmd = [os.path.join(constants.CHROMITE_BIN_DIR, 'parallel_emerge')]
      if board is not None:
        cmd += ['--board=%s' % board]
    else:
      cmd = ['emerge'] if board is None else ['emerge-%s' % board]
    return cmd

  def _Emerge(self, packages, board=None):
    """Emerge the specified packages to the specified board.

    Args:
      packages: Packages to emerge.
      board: Board to emerge to. If None, emerge to host.
    """
    modified_packages = list(workon.ListModifiedWorkonPackages(board,
                                                               board is None))
    cmd = self._GetEmergeCommand(board) + [
        '-uNv',
        '--reinstall-atoms=%s' % ' '.join(modified_packages),
        '--usepkg-exclude=%s' % ' '.join(modified_packages),
    ]
    cmd.append('--deep' if self.options.deps else '--nodeps')
    if self.options.binary:
      cmd += ['-g', '--with-bdeps=y']
      if board is None:
        # Only update toolchains in the chroot when binpkgs are available. The
        # toolchain rollout process only takes place when the chromiumos sdk
        # builder finishes a successful build and pushes out binpkgs.
        cmd += ['--useoldpkg-atoms=%s' % ' '.join(GetToolchainPackages())]
    if self.options.rebuild_deps:
      cmd.append('--rebuild-if-unbuilt')
    if self.options.jobs:
      cmd.append('--jobs=%d' % self.options.jobs)
    if self.options.log_level.lower() == 'debug':
      cmd.append('--show-output')
    cros_build_lib.SudoRunCommand(cmd + packages)

  def _UpdateChroot(self):
    """Update the chroot if needed."""
    if self.chroot_update:
      # Run chroot update hooks.
      cmd = [os.path.join(constants.CROSUTILS_DIR, 'run_chroot_version_hooks')]
      cros_build_lib.RunCommand(cmd, debug_level=logging.DEBUG)

      # Update toolchains.
      cmd = [os.path.join(constants.CHROMITE_BIN_DIR, 'cros_setup_toolchains')]
      cros_build_lib.SudoRunCommand(cmd, debug_level=logging.DEBUG)

      # Update the host before updating the board.
      self._Emerge(list(_HOST_PKGS))

      # Automatically discard all CONFIG_PROTECT'ed files. Those that are
      # protected should not be overwritten until the variable is changed.
      # Autodiscard is option "-9" followed by the "YES" confirmation.
      cros_build_lib.SudoRunCommand(['etc-update'], input='-9\nYES\n',
                                    debug_level=logging.DEBUG)
      self.chroot_update = False

  def _Build(self):
    """Update the chroot, then merge the requested packages."""
    self._UpdateChroot()
    self._Emerge(self.build_pkgs, None if self.host else self.board)

  def _SetupBoardIfNeeded(self):
    """Create the board if it's missing."""
    if not self.host:
      self.project.GeneratePortageConfig()

      self._UpdateChroot()
      cmd = [os.path.join(constants.CROSUTILS_DIR, 'setup_board'),
             '--skip_toolchain_update', '--skip_chroot_upgrade']
      cmd.append('--board=%s' % self.board)
      if not self.options.binary:
        cmd.append('--nousepkg')
      cros_build_lib.RunCommand(cmd)

  def Run(self):
    """Run cros build."""
    if not self.host:
      if not self.board:
        cros_build_lib.Die('You did not specify a board/project to build for. '
                           'You need to be in a project directory or set '
                           '--board/--project')
      if not self.project:
        cros_build_lib.Die('Could not find project for %s' % self.board)

    self.RunInsideChroot(auto_detect_project=True)

    self.build_pkgs = self.options.packages or (self.project and
                                                self.project.MainPackages())
    if not self.build_pkgs:
      cros_build_lib.Die('No packages found, nothing to build.')

    self._SetupBoardIfNeeded()
    parallel.RunParallelSteps([self._CheckDependencies, self._Build])
