# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Logic to handle uprevving packages."""

from __future__ import print_function

import os

from chromite.cbuildbot import manifest_version
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import osutils
from chromite.lib import parallel
from chromite.lib import portage_util


class UprevOverlayManager(object):
  """Class to handle the uprev process for a set of overlays.

  This handles the standard uprev process that covers most packages. There are
  also specialized uprev processes for a few specific packages not handled by
  this class, e.g. chrome and android.

  TODO (saklein): The manifest object for this class is used deep in the
    portage_util uprev process. Look into whether it's possible to redo it so
    the manifest isn't required.
  """

  def __init__(self, overlays, manifest, build_targets=None, chroot=None,
               output_dir=None):
    """Init function.

    Args:
      overlays (list[str]): The overlays to search for ebuilds.
      manifest (git.ManifestCheckout): The manifest object.
      build_targets (list[build_target_util.BuildTarget]|None): The build
        targets to clean in |chroot|, if desired. No effect unless |chroot| is
        provided.
      chroot (chroot_lib.Chroot|None): The chroot to clean, if desired.
      output_dir (str|None): The path to optionally dump result files.
    """
    self.overlays = overlays
    self.manifest = manifest
    self.build_targets = build_targets or []
    self.chroot = chroot
    self.output_dir = output_dir

    self._revved_packages = None
    self._new_package_atoms = None
    self._new_ebuild_files = None
    self._removed_ebuild_files = None
    self._overlay_ebuilds = None

  @property
  def modified_ebuilds(self):
    if self._new_ebuild_files is not None:
      return self._new_ebuild_files + self._removed_ebuild_files
    else:
      return []

  def uprev(self):
    self._populate_overlay_ebuilds()

    with parallel.Manager() as manager:
      # Contains the list of packages we actually revved.
      self._revved_packages = manager.list()
      # The new package atoms for cleanup.
      self._new_package_atoms = manager.list()
      # The list of added ebuild files.
      self._new_ebuild_files = manager.list()
      # The list of removed ebuild files.
      self._removed_ebuild_files = manager.list()

      inputs = [[overlay] for overlay in self.overlays]
      parallel.RunTasksInProcessPool(self._uprev_overlay, inputs)

      self._revved_packages = list(self._revved_packages)
      self._new_package_atoms = list(self._new_package_atoms)
      self._new_ebuild_files = list(self._new_ebuild_files)
      self._removed_ebuild_files = list(self._removed_ebuild_files)

    self._clean_stale_packages()

    if self.output_dir and os.path.exists(self.output_dir):
      # Write out dumps of the results. This is largely meant for sanity
      # checking results.
      osutils.WriteFile(os.path.join(self.output_dir, 'revved_packages'),
                        '\n'.join(self._revved_packages))
      osutils.WriteFile(os.path.join(self.output_dir, 'new_package_atoms'),
                        '\n'.join(self._new_package_atoms))
      osutils.WriteFile(os.path.join(self.output_dir, 'new_ebuild_files'),
                        '\n'.join(self._new_ebuild_files))
      osutils.WriteFile(os.path.join(self.output_dir, 'removed_ebuild_files'),
                        '\n'.join(self._removed_ebuild_files))

  def _uprev_overlay(self, overlay):
    """Execute uprevs for an overlay.

    Args:
      overlay: The overlay to uprev.
    """
    if not os.path.isdir(overlay):
      logging.warning('Skipping %s, which is not a directory.', overlay)
      return

    ebuilds = self._overlay_ebuilds.get(overlay, [])
    if not ebuilds:
      return

    inputs = [[overlay, ebuild] for ebuild in ebuilds]
    parallel.RunTasksInProcessPool(self._uprev_ebuild, inputs)

  def _uprev_ebuild(self, overlay, ebuild):
    """Work on a single ebuild.

    Args:
      overlay: The overlay the ebuild belongs to.
      ebuild: The ebuild to work on.
    """
    logging.debug('Working on %s, info %s', ebuild.package,
                  ebuild.cros_workon_vars)
    try:
      result = ebuild.RevWorkOnEBuild(
          os.path.join(constants.SOURCE_ROOT, 'src'), self.manifest)
    except (OSError, IOError):
      logging.warning(
          'Cannot rev %s\n'
          'Note you will have to go into %s '
          'and reset the git repo yourself.', ebuild.package, overlay)
      raise

    if result:
      new_package, ebuild_path_to_add, ebuild_path_to_remove = result

      if ebuild_path_to_add:
        self._new_ebuild_files.append(ebuild_path_to_add)
      if ebuild_path_to_remove:
        osutils.SafeUnlink(ebuild_path_to_remove)
        self._removed_ebuild_files.append(ebuild_path_to_remove)

      self._revved_packages.append(ebuild.package)
      self._new_package_atoms.append('=%s' % new_package)

  def _populate_overlay_ebuilds(self):
    """Populates the overlay to ebuilds mapping."""
    # See crrev.com/c/1257944 for origins of this.
    root_version = manifest_version.VersionInfo.from_repo(constants.SOURCE_ROOT)
    subdir_removal = manifest_version.VersionInfo('10363.0.0')
    require_subdir_support = root_version < subdir_removal

    # Parameters lost to the current narrow implementation.
    use_all = True
    package_list = []
    force = False

    overlay_ebuilds = {}
    inputs = [[overlay, use_all, package_list, force, require_subdir_support]
              for overlay in self.overlays]
    result = parallel.RunTasksInProcessPool(portage_util.GetOverlayEBuilds,
                                            inputs)
    for idx, ebuilds in enumerate(result):
      overlay_ebuilds[self.overlays[idx]] = ebuilds

    self._overlay_ebuilds = overlay_ebuilds

  def _clean_stale_packages(self):
    """Cleans up stale package info from a previous build."""
    if not self.chroot or not self.chroot.exists():
      return

    if self._new_package_atoms:
      logging.info('Cleaning up stale packages %s.', self._new_package_atoms)

    # First unmerge all the packages for a board, then eclean it.
    # We need these two steps to run in order (unmerge/eclean),
    # but we can let all the boards run in parallel.
    def _do_clean_stale_packages(board):
      if board:
        suffix = '-' + board
        runcmd = cros_build_lib.RunCommand
      else:
        suffix = ''
        runcmd = cros_build_lib.SudoRunCommand

      emerge, eclean = 'emerge' + suffix, 'eclean' + suffix
      if not osutils.FindMissingBinaries([emerge, eclean]):
        if self._new_package_atoms:
          # If nothing was found to be unmerged, emerge will exit(1).
          result = runcmd([emerge, '-q', '--unmerge'] + self._new_package_atoms,
                          enter_chroot=True,
                          chroot_args=self.chroot.get_enter_args(),
                          extra_env={'CLEAN_DELAY': '0'}, error_code_ok=True,
                          cwd=constants.SOURCE_ROOT)
          if result.returncode not in (0, 1):
            raise cros_build_lib.RunCommandError('unexpected error', result)

        runcmd([eclean, '-d', 'packages'],
               cwd=constants.SOURCE_ROOT, enter_chroot=True,
               chroot_args=self.chroot.get_enter_args(), redirect_stdout=True,
               redirect_stderr=True)

    tasks = []
    for build_target in self.build_targets:
      tasks.append([build_target.name])
    tasks.append([None])

    parallel.RunTasksInProcessPool(_do_clean_stale_packages, tasks)
