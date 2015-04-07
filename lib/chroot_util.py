# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilities for updating and building in the chroot environment."""

from __future__ import print_function

import os

from chromite.cbuildbot import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging

if cros_build_lib.IsInsideChroot():
  # These import libraries outside chromite. See brbug.com/472.
  from chromite.scripts import cros_setup_toolchains as toolchain
  from chromite.scripts import cros_list_modified_packages as workon


_HOST_PKGS = ('virtual/target-sdk', 'world',)


def _GetToolchainPackages():
  """Get a list of host toolchain packages."""
  # Load crossdev cache first for faster performance.
  toolchain.Crossdev.Load(False)
  packages = toolchain.GetTargetPackages('host')
  return [toolchain.GetPortagePackage('host', x) for x in packages]


def GetEmergeCommand(board=None):
  """Returns the emerge command to use for |board| (host if None)."""
  cmd = [os.path.join(constants.CHROMITE_BIN_DIR, 'parallel_emerge')]
  if board is not None:
    cmd += ['--board=%s' % board]
  return cmd


def Emerge(packages, brick=None, board=None, host=False, with_deps=True,
           rebuild_deps=True, use_binary=True, jobs=None, debug_output=False):
  """Emerge the specified |packages|.

  Args:
    packages: List of packages to emerge.
    brick: The brick to build packages for. Ignored if |host|.
    board: The board name to build for. Ignored if |host| or |brick|.
    host: If True, emerge to host.
    with_deps: Whether to include dependencies.
    rebuild_deps: Whether to rebuild dependencies.
    use_binary: Whether to use binary packages.
    jobs: Number of jobs to run in parallel.
    debug_output: Emit debug level output.

  Raises:
    cros_build_lib.RunCommandError: If emerge returns an error.
  """
  if not packages:
    raise ValueError('No packages provided')

  if host:
    brick = board = None
  elif brick:
    board = brick.FriendlyName()

  cmd = GetEmergeCommand(board=board)
  cmd.append('-uNv')

  modified_packages = list(workon.ListModifiedWorkonPackages(
      None if brick else board, brick, host))
  if modified_packages:
    mod_pkg_list = ' '.join(modified_packages)
    cmd += ['--reinstall-atoms=' + mod_pkg_list,
            '--usepkg-exclude=' + mod_pkg_list]

  cmd.append('--deep' if with_deps else '--nodeps')
  if use_binary:
    cmd += ['-g', '--with-bdeps=y']
    if host:
      # Only update toolchains in the chroot when binpkgs are available. The
      # toolchain rollout process only takes place when the chromiumos sdk
      # builder finishes a successful build and pushes out binpkgs.
      cmd += ['--useoldpkg-atoms=%s' % ' '.join(_GetToolchainPackages())]

  if rebuild_deps:
    cmd.append('--rebuild-if-unbuilt')
  if jobs:
    cmd.append('--jobs=%d' % jobs)
  if debug_output:
    cmd.append('--show-output')

  cros_build_lib.SudoRunCommand(cmd + packages)


def UpdateChroot(brick=None, board=None, update_host_packages=True):
  """Update the chroot."""
  # Run chroot update hooks.
  cmd = [os.path.join(constants.CROSUTILS_DIR, 'run_chroot_version_hooks')]
  cros_build_lib.RunCommand(cmd, debug_level=logging.DEBUG)

  # Update toolchains.
  cmd = [os.path.join(constants.CHROMITE_BIN_DIR, 'cros_setup_toolchains')]
  if brick:
    cmd += ['--targets=bricks', '--include-bricks=%s' % brick.brick_locator]
  elif board:
    cmd += ['--targets=boards', '--include-boards=%s' % board]
  cros_build_lib.SudoRunCommand(cmd, debug_level=logging.DEBUG)

  # Update the host before updating the board.
  if update_host_packages:
    Emerge(list(_HOST_PKGS), host=True)

  # Automatically discard all CONFIG_PROTECT'ed files. Those that are
  # protected should not be overwritten until the variable is changed.
  # Autodiscard is option "-9" followed by the "YES" confirmation.
  cros_build_lib.SudoRunCommand(['etc-update'], input='-9\nYES\n',
                                debug_level=logging.DEBUG)


def SetupBoard(brick=None, board=None, update_chroot=True,
               update_host_packages=True, use_binary=True):
  """Set up a sysroot for |brick| or |board| (either must be provided).

  This invokes UpdateChroot() with the given brick/board values, unless
  otherwise instructed.

  Args:
    brick: Brick object we need to set up a sysroot for.
    board: Board name to set up a sysroot for. Ignored if |brick| is provided.
    update_chroot: Whether we should update the chroot first.
    update_host_packages: Whether to update host packages in the chroot.
    use_binary: If okay to use binary packages during the update.
  """
  if update_chroot:
    UpdateChroot(brick=brick, board=board,
                 update_host_packages=update_host_packages)

  cmd = [os.path.join(constants.CROSUTILS_DIR, 'setup_board'),
         '--skip_toolchain_update', '--skip_chroot_upgrade']
  if brick:
    brick.GeneratePortageConfig()
    cmd.append('--brick=%s' % brick.brick_locator)
  elif board:
    cmd.append('--board=%s' % board)
  else:
    raise ValueError('Either brick or board must be provided')

  if not use_binary:
    cmd.append('--nousepkg')

  cros_build_lib.RunCommand(cmd)
