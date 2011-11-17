# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import fileinput
import os
import re

from chromite.lib import cros_build_lib


class _BlackListManager(object):
  """Small wrapper class to manage black lists for marking all packages."""
  BLACK_LIST_FILE = os.path.join(os.path.dirname(os.path.realpath(__file__)),
                                 'cros_mark_as_stable_blacklist')

  def __init__(self):
    """Initializes the black list manager."""
    self.black_list_re_array = None
    self._Initialize()

  def _Initialize(self):
    """Initializes the black list manager from a black list file."""
    self.black_list_re_array = []
    with open(self.BLACK_LIST_FILE) as file_handle:
      for line in file_handle.readlines():
        line = line.strip()
        # Ignore comment lines.
        if line and not line.startswith('#'):
          line = line.rstrip()
          package_array = line.split('/')
          assert len(package_array) == 2, \
              'Line %s does not match package format.' % line
          category, package_name = package_array
          self.black_list_re_array.append(
              re.compile('.*/%s/%s/%s-.*\.ebuild' % (category, package_name,
                                                     package_name)))

  def IsPackageBlackListed(self, path_to_ebuild):
    """Returns True if the package given by the path is blacklisted."""
    assert self.black_list_re_array != None, 'Black list not initialized.'

    for pattern in self.black_list_re_array:
      if pattern.match(path_to_ebuild):
        return True

    return False


class EBuild(object):
  """Wrapper class for information about an ebuild."""

  verbose = False

  def __init__(self, path):
    """Sets up data about an ebuild from its path."""
    from portage.versions import pkgsplit
    _path, self.category, self.pkgname, filename = path.rsplit('/', 3)
    _pkgname, self.version_no_rev, rev = pkgsplit(
        filename.replace('.ebuild', ''))

    self.ebuild_path_no_version = os.path.join(
        os.path.dirname(path), self.pkgname)
    self.ebuild_path_no_revision = '%s-%s' % (self.ebuild_path_no_version,
                                              self.version_no_rev)
    self.current_revision = int(rev.replace('r', ''))
    self.version = '%s-%s' % (self.version_no_rev, rev)
    self.package = '%s/%s' % (self.category, self.pkgname)
    self.ebuild_path = path

    self.is_workon = False
    self.is_stable = False

    for line in fileinput.input(path):
      if line.startswith('inherit ') and 'cros-workon' in line:
        self.is_workon = True
      elif (line.startswith('KEYWORDS=') and '~' not in line and
            ('amd64' in line or 'x86' in line or 'arm' in line)):
        self.is_stable = True
    fileinput.close()

  def _RunCommand(self, command):
    command_result = cros_build_lib.RunCommand(
      command, redirect_stdout=True, print_cmd=self.verbose, shell=True)
    return command_result.output

  def GetCommitId(self, srcroot):
    """Get the commit id for this ebuild."""
    # Grab and evaluate CROS_WORKON variables from this ebuild.
    unstable_ebuild = '%s-9999.ebuild' % self.ebuild_path_no_version
    cmd = ('export CROS_WORKON_LOCALNAME="%s" CROS_WORKON_PROJECT="%s"; '
           'eval $(grep -E "^CROS_WORKON" %s) && '
           'echo $CROS_WORKON_PROJECT '
           '$CROS_WORKON_LOCALNAME/$CROS_WORKON_SUBDIR'
           % (self.pkgname, self.pkgname, unstable_ebuild))
    project, subdir = self._RunCommand(cmd).split()

    # Calculate srcdir.
    if self.category == 'chromeos-base':
      dir_ = 'platform'
    else:
      dir_ = 'third_party'

    srcdir = os.path.join(srcroot, dir_, subdir)

    if not os.path.isdir(srcdir):
      cros_build_lib.Die('Cannot find commit id for %s' % self.ebuild_path)

    # Verify that we're grabbing the commit id from the right project name.
    # NOTE: chromeos-kernel has the wrong project name, so it fails this
    # check.
    # TODO(davidjames): Fix the project name in the chromeos-kernel ebuild.
    cmd = ('cd %s && ( git config --get remote.cros.projectname || '
           'git config --get remote.cros-internal.projectname )') % srcdir
    actual_project = self._RunCommand(cmd).rstrip()
    if project not in (actual_project, 'chromeos-kernel'):
      cros_build_lib.Die('Project name mismatch for %s (%s != %s)' % (
          unstable_ebuild, project, actual_project))

    # Get commit id.
    output = self._RunCommand('cd %s && git rev-parse HEAD' % srcdir)
    if not output:
      cros_build_lib.Die('Missing commit id for %s' % self.ebuild_path)
    return output.rstrip()


def BestEBuild(ebuilds):
  """Returns the newest EBuild from a list of EBuild objects."""
  from portage.versions import vercmp
  winner = ebuilds[0]
  for ebuild in ebuilds[1:]:
    if vercmp(winner.version, ebuild.version) < 0:
      winner = ebuild
  return winner


def _FindUprevCandidates(files, blacklist):
  """Return a list of uprev candidates from specified list of files.

  Usually an uprev candidate is a the stable ebuild in a cros_workon directory.
  However, if no such stable ebuild exists (someone just checked in the 9999
  ebuild), this is the unstable ebuild.

  Args:
    files: List of files.
  """
  workon_dir = False
  stable_ebuilds = []
  unstable_ebuilds = []
  for path in files:
    if path.endswith('.ebuild') and not os.path.islink(path):
      ebuild = EBuild(path)
      if ebuild.is_workon and not blacklist.IsPackageBlackListed(path):
        workon_dir = True
        if ebuild.is_stable:
          stable_ebuilds.append(ebuild)
        else:
          unstable_ebuilds.append(ebuild)

  # If we found a workon ebuild in this directory, apply some sanity checks.
  if workon_dir:
    if len(unstable_ebuilds) > 1:
      cros_build_lib.Die('Found multiple unstable ebuilds in %s' %
                         os.path.dirname(path))
    if len(stable_ebuilds) > 1:
      stable_ebuilds = [BestEBuild(stable_ebuilds)]

      # Print a warning if multiple stable ebuilds are found in the same
      # directory. Storing multiple stable ebuilds is error-prone because
      # the older ebuilds will not get rev'd.
      cros_build_lib.Warning('Found multiple stable ebuilds in %s' %
                             os.path.dirname(path))

    if not unstable_ebuilds:
      cros_build_lib.Die('Missing 9999 ebuild in %s' % os.path.dirname(path))
    if not stable_ebuilds:
      cros_build_lib.Warning('Missing stable ebuild in %s' %
                             os.path.dirname(path))
      return unstable_ebuilds[0]

  if stable_ebuilds:
    return stable_ebuilds[0]
  else:
    return None


def BuildEBuildDictionary(overlays, use_all, packages):
  """Build a dictionary of the ebuilds in the specified overlays.

  overlays: A map which maps overlay directories to arrays of stable EBuilds
    inside said directories.
  use_all: Whether to include all ebuilds in the specified directories.
    If true, then we gather all packages in the directories regardless
    of whether they are in our set of packages.
  packages: A set of the packages we want to gather.
  """
  blacklist = _BlackListManager()
  for overlay in overlays:
    for package_dir, _dirs, files in os.walk(overlay):
      # Add stable ebuilds to overlays[overlay].
      paths = [os.path.join(package_dir, path) for path in files]
      ebuild = _FindUprevCandidates(paths, blacklist)

      # If the --all option isn't used, we only want to update packages that
      # are in packages.
      if ebuild and (use_all or ebuild.package in packages):
        overlays[overlay].append(ebuild)
