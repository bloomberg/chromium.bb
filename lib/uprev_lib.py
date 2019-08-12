# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Logic to handle uprevving packages."""

from __future__ import print_function

import collections
import filecmp
import functools
import os
import re

from chromite.cbuildbot import manifest_version
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import osutils
from chromite.lib import parallel
from chromite.lib import portage_util
from chromite.lib.chroot_lib import Chroot

CHROME_VERSION_REGEX = r'\d+\.\d+\.\d+\.\d+'

# The directory relative to the source root housing the chrome packages.
_CHROME_OVERLAY_DIR = 'src/third_party/chromiumos-overlay'
_CHROME_OVERLAY_PATH = os.path.join(constants.SOURCE_ROOT, _CHROME_OVERLAY_DIR)

GitRef = collections.namedtuple('GitRef', ['path', 'ref', 'revision'])


class Error(Exception):
  """Base error class for the module."""


class NoUnstableEbuildError(Error):
  """When no unstable ebuild can be found."""


class ChromeEBuild(portage_util.EBuild):
  """Thin sub-class of EBuild that adds a few small helpers."""
  chrome_version_re = re.compile(r'.*-(%s|9999).*' % CHROME_VERSION_REGEX)
  chrome_version = ''

  def __init__(self, path):
    portage_util.EBuild.__init__(self, path)
    re_match = self.chrome_version_re.match(self.ebuild_path_no_revision)
    if re_match:
      self.chrome_version = re_match.group(1)

  def __str__(self):
    return self.ebuild_path

  @property
  def is_unstable(self):
    return not self.is_stable

  @property
  def atom(self):
    return '%s-%s' % (self.package, self.version)


def get_chrome_version_from_refs(refs):
  """Get the chrome version to use from the list of provided tags.

  Args:
    refs (list[GitRef]): The tags to parse for the best chrome version.

  Returns:
    str: The chrome version to use.
  """
  assert refs

  # Each ref (tag) is a chrome version string, e.g. "78.0.3876.1".
  return best_chrome_version(ref.ref for ref in refs)


def best_chrome_version(versions):
  # Convert each version from a string like "78.0.3876.1" to a list of ints
  # to compare them, find the most recent (max), and then reconstruct the
  # version string.
  assert versions

  version = max([int(part) for part in v.split('.')] for v in versions)
  return '.'.join(str(part) for part in version)


def best_chrome_ebuild(ebuilds):
  """Determine the best/newest chrome ebuild from a list of ebuilds."""
  assert ebuilds

  version = best_chrome_version(ebuild.chrome_version for ebuild in ebuilds)
  candidates = [
      ebuild for ebuild in ebuilds if ebuild.chrome_version == version
  ]

  if len(candidates) == 1:
    # Only one, return it.
    return candidates[0]

  # Compare revisions to break a tie.
  best = candidates[0]
  for candidate in candidates[1:]:
    if best.current_revision < candidate.current_revision:
      best = candidate

  return best


def find_chrome_ebuilds(package_dir):
  """Return a tuple of chrome's unstable ebuild and stable ebuilds.

  Args:
    package_dir: The path to where the package ebuild is stored.

  Returns:
    Tuple [unstable_ebuild, stable_ebuilds].

  Raises:
    Exception: if no unstable ebuild exists for Chrome.
  """
  stable_ebuilds = []
  unstable_ebuilds = []

  for ebuild_path in portage_util.EBuild.List(package_dir):
    ebuild = ChromeEBuild(ebuild_path)
    if not ebuild.chrome_version:
      logging.warning('Poorly formatted ebuild found at %s', ebuild_path)
      continue

    if ebuild.is_unstable:
      unstable_ebuilds.append(ebuild)
    else:
      stable_ebuilds.append(ebuild)

  # Apply some sanity checks.
  if not unstable_ebuilds:
    raise NoUnstableEbuildError('Missing 9999 ebuild for %s' % package_dir)
  if not stable_ebuilds:
    logging.warning('Missing stable ebuild for %s', package_dir)

  return best_chrome_ebuild(unstable_ebuilds), stable_ebuilds


def find_chrome_uprev_candidate(stable_ebuilds):
  """Find the ebuild to replace.

  Args:
    stable_ebuilds (list[ChromeEBuild]): All stable ebuilds that were found.

  Returns:
    EBuild: The ebuild being replaced.
  """
  candidates = []
  # This is an artifact from the old process.
  chrome_branch_re = re.compile(r'%s.*_rc.*' % CHROME_VERSION_REGEX)
  for ebuild in stable_ebuilds:
    if chrome_branch_re.search(ebuild.version):
      candidates.append(ebuild)

  if not candidates:
    return None

  return best_chrome_ebuild(candidates)


class UprevChromeManager(object):
  """Class to handle uprevving chrome and its related packages."""

  def __init__(self, version, build_targets=None, overlay_dir=None,
               chroot=None):
    self._version = version
    self._build_targets = build_targets or []
    self._new_ebuild_files = []
    self._removed_ebuild_files = []
    self._overlay_dir = overlay_dir or _CHROME_OVERLAY_PATH
    self._chroot = chroot

  @property
  def modified_ebuilds(self):
    return self._new_ebuild_files + self._removed_ebuild_files

  def uprev(self, package):
    """Uprev a chrome package."""
    package_dir = os.path.join(self._overlay_dir, package)
    package_name = os.path.basename(package)

    # Find the unstable (9999) ebuild and any existing stable ebuilds.
    unstable_ebuild, stable_ebuilds = find_chrome_ebuilds(package_dir)
    # Find the best stable candidate to uprev -- the one that will be replaced.
    candidate = find_chrome_uprev_candidate(stable_ebuilds)
    new_ebuild = self._mark_as_stable(candidate, unstable_ebuild, package_name,
                                      package_dir)

    if not new_ebuild:
      return False

    self._new_ebuild_files.append(new_ebuild.ebuild_path)
    if candidate and not candidate.IsSticky():
      osutils.SafeUnlink(candidate.ebuild_path)
      self._removed_ebuild_files.append(candidate.ebuild_path)

    if self._build_targets:
      self._clean_stale_package(new_ebuild.atom)

    return True

  def _mark_as_stable(self, stable_candidate, unstable_ebuild, package_name,
                      package_dir):
    """Uprevs the chrome ebuild specified by chrome_rev.

    This is the main function that uprevs the chrome_rev from a stable candidate
    to its new version.

    Args:
      stable_candidate: ebuild that corresponds to the stable ebuild we are
        revving from.  If None, builds the a new ebuild given the version
        and logic for chrome_rev type with revision set to 1.
      unstable_ebuild: ebuild corresponding to the unstable ebuild for chrome.
      package_name: package name.
      package_dir: Path to the chromeos-chrome package dir.

    Returns:
      Full portage version atom (including rc's, etc) that was revved.
    """

    def _is_new_ebuild_redundant(uprevved_ebuild, stable_ebuild):
      """Returns True if the new ebuild is redundant.

      This is True if there if the current stable ebuild is the exact same copy
      of the new one.
      """
      if (stable_ebuild and
          stable_candidate.chrome_version == uprevved_ebuild.chrome_version):
        return filecmp.cmp(
            uprevved_ebuild.ebuild_path,
            stable_ebuild.ebuild_path,
            shallow=False)
      else:
        return False

    # Case where we have the last stable candidate with same version just rev.
    if stable_candidate and stable_candidate.chrome_version == self._version:
      new_ebuild_path = '%s-r%d.ebuild' % (
          stable_candidate.ebuild_path_no_revision,
          stable_candidate.current_revision + 1)
    else:
      pf = '%s-%s_rc-r1' % (package_name, self._version)
      new_ebuild_path = os.path.join(package_dir, '%s.ebuild' % pf)

    portage_util.EBuild.MarkAsStable(unstable_ebuild.ebuild_path,
                                     new_ebuild_path, {})
    new_ebuild = ChromeEBuild(new_ebuild_path)

    # Determine whether this is ebuild is redundant.
    if _is_new_ebuild_redundant(new_ebuild, stable_candidate):
      msg = 'Previous ebuild with same version found and ebuild is redundant.'
      logging.info(msg)
      os.unlink(new_ebuild_path)
      return None

    return new_ebuild

  def _clean_stale_package(self, package):
    clean_stale_packages([package], self._build_targets, chroot=self._chroot)


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
    clean_stale_packages(self._new_package_atoms, self.build_targets,
                         chroot=self.chroot)


def clean_stale_packages(new_package_atoms, build_targets, chroot=None):
  """Cleans up stale package info from a previous build."""
  if new_package_atoms:
    logging.info('Cleaning up stale packages %s.', new_package_atoms)

  chroot = chroot or Chroot()

  if cros_build_lib.IsOutsideChroot() and not chroot.exists():
    logging.warning('Unable to clean packages. No chroot to enter.')
    return

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

    if cros_build_lib.IsOutsideChroot():
      # Setup runcmd with the chroot arguments once.
      runcmd = functools.partial(
          runcmd, enter_chroot=True, chroot_args=chroot.get_enter_args())

    emerge, eclean = 'emerge' + suffix, 'eclean' + suffix
    if not osutils.FindMissingBinaries([emerge, eclean]):
      if new_package_atoms:
        # If nothing was found to be unmerged, emerge will exit(1).
        result = runcmd(
            [emerge, '-q', '--unmerge'] + new_package_atoms,
            extra_env={'CLEAN_DELAY': '0'},
            error_code_ok=True,
            cwd=constants.SOURCE_ROOT)
        if result.returncode not in (0, 1):
          raise cros_build_lib.RunCommandError('unexpected error', result)

      runcmd([eclean, '-d', 'packages'],
             cwd=constants.SOURCE_ROOT,
             capture_output=True)

  tasks = []
  for build_target in build_targets:
    tasks.append([build_target.name])
  tasks.append([None])

  parallel.RunTasksInProcessPool(_do_clean_stale_packages, tasks)
