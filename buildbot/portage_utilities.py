# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Routines and classes for working with Portage overlays and ebuilds."""

import collections
import filecmp
import fileinput
import glob
import logging
import multiprocessing
import os
import re
import shutil
import sys

from chromite.buildbot import constants
from chromite.lib import cros_build_lib
from chromite.lib import gerrit
from chromite.lib import git
from chromite.lib import osutils

_PRIVATE_PREFIX = '%(buildroot)s/src/private-overlays'
_GLOBAL_OVERLAYS = [
  '%s/chromeos-overlay' % _PRIVATE_PREFIX,
  '%s/chromeos-partner-overlay' % _PRIVATE_PREFIX,
  '%(buildroot)s/src/third_party/chromiumos-overlay',
  '%(buildroot)s/src/third_party/portage-stable',
]

# Takes two strings, package_name and commit_id.
_GIT_COMMIT_MESSAGE = 'Marking 9999 ebuild for %s with commit(s) %s as stable.'

# Define datastructures for holding PV and CPV objects.
_PV_FIELDS = ['pv', 'package', 'version', 'version_no_rev', 'rev']
PV = collections.namedtuple('PV', _PV_FIELDS)
CPV = collections.namedtuple('CPV', ['category'] + _PV_FIELDS)

# Package matching regexp, as dictated by package manager specification:
# http://www.gentoo.org/proj/en/qa/pms.xml
_pkg = '(?P<package>' + '[\w+][\w+-]*)'
_ver = '(?P<version>' + \
       '(?P<version_no_rev>(\d+)((\.\d+)*)([a-z]?)' + \
       '((_(pre|p|beta|alpha|rc)\d*)*))' + \
       '(-(?P<rev>r(\d+)))?)'
_pvr_re = re.compile('^(?P<pv>%s-%s)$' % (_pkg, _ver), re.VERBOSE)

# This regex matches blank lines, commented lines, and the EAPI line.
_blank_or_eapi_re = re.compile(r'^\s*(?:#|EAPI=|$)')


def _ListOverlays(board=None, buildroot=constants.SOURCE_ROOT):
  """Return the list of overlays to use for a given buildbot.

  Always returns all overlays, and does not perform any filtering.

  Args:
    board: Board to look at.
    buildroot: Source root to find overlays.
  """
  overlays, patterns = [], []
  if board is None:
    patterns += ['overlay*']
  else:
    board_no_variant, _, variant = board.partition('_')
    patterns += ['overlay-%s' % board_no_variant]
    if variant:
      patterns += ['overlay-variant-%s' % board.replace('_', '-')]

  for d in _GLOBAL_OVERLAYS:
    d %= dict(buildroot=buildroot)
    if os.path.isdir(d):
      overlays.append(d)

  for p in patterns:
    overlays += glob.glob('%s/src/overlays/%s' % (buildroot, p))
    overlays += glob.glob('%s/src/private-overlays/%s-private' % (buildroot, p))

  return overlays


def FindOverlays(overlay_type, board=None, buildroot=constants.SOURCE_ROOT):
  """Return the list of overlays to use for a given buildbot.

  Args:
    board: Board to look at.
    buildroot: Source root to find overlays.
    overlay_type: A string describing which overlays you want.
      'private': Just the private overlays.
      'public': Just the public overlays.
      'both': Both the public and private overlays.
  """
  overlays = _ListOverlays(board=board, buildroot=buildroot)
  private_prefix = _PRIVATE_PREFIX % dict(buildroot=buildroot)
  if overlay_type == constants.PRIVATE_OVERLAYS:
    return [x for x in overlays if x.startswith(private_prefix)]
  elif overlay_type == constants.PUBLIC_OVERLAYS:
    return [x for x in overlays if not x.startswith(private_prefix)]
  elif overlay_type == constants.BOTH_OVERLAYS:
    return overlays
  else:
    assert overlay_type is None
    return []


class MissingOverlayException(Exception):
  """This exception indicates that a needed overlay is missing."""


def FindPrimaryOverlay(overlay_type, board, buildroot=constants.SOURCE_ROOT):
  """Return the primary overlay to use for a given buildbot.

  An overlay is only considered a primary overlay if it has a make.conf and a
  toolchain.conf. If multiple primary overlays are found, the first primary
  overlay is returned.

  Args:
    overlay_type: A string describing which overlays you want.
      'private': Just the private overlays.
      'public': Just the public overlays.
      'both': Both the public and private overlays.
    board: Board to look at.
  Raises:
    MissingOverlayException: No primary overlay found.
  """
  for overlay in FindOverlays(overlay_type, board, buildroot):
    if (os.path.exists(os.path.join(overlay, 'make.conf')) and
        os.path.exists(os.path.join(overlay, 'toolchain.conf'))):
      return overlay
  raise MissingOverlayException('No primary overlay found for board=%r' % board)


def GetOverlayName(overlay):
  try:
    return open('%s/profiles/repo_name' % overlay).readline().rstrip()
  except IOError:
    # Not all overlays have a repo_name, so don't make a fuss.
    return None


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


class EBuildVersionFormatException(Exception):
  def __init__(self, filename):
    self.filename = filename
    message = ('Ebuild file name %s '
               'does not match expected format.' % filename)
    super(EBuildVersionFormatException, self).__init__(message)


class EbuildFormatIncorrectException(Exception):
  def __init__(self, filename, message):
    message = 'Ebuild %s has invalid format: %s ' % (filename, message)
    super(EbuildFormatIncorrectException, self).__init__(message)


class EBuild(object):
  """Wrapper class for information about an ebuild."""

  VERBOSE = False
  _PACKAGE_VERSION_PATTERN = re.compile(
    r'.*-(([0-9][0-9a-z_.]*)(-r[0-9]+)?)[.]ebuild')
  _WORKON_COMMIT_PATTERN = re.compile(r'^CROS_WORKON_COMMIT="(.*)"$')

  @classmethod
  def _Print(cls, message):
    """Verbose print function."""
    if cls.VERBOSE:
      cros_build_lib.Info(message)

  @classmethod
  def _RunCommand(cls, command, **kwargs):
    return cros_build_lib.RunCommandCaptureOutput(
        command, print_cmd=cls.VERBOSE, **kwargs).output

  def IsSticky(self):
    """Returns True if the ebuild is sticky."""
    return self.is_stable and self.current_revision == 0

  @classmethod
  def UpdateEBuild(cls, ebuild_path, variables, redirect_file=None,
                   make_stable=True):
    """Static function that updates WORKON information in the ebuild.

    This function takes an ebuild_path and updates WORKON information.

    Args:
      ebuild_path:  The path of the ebuild.
      variables: Dictionary of variables to update in ebuild.
      redirect_file:  Optionally redirect output of new ebuild somewhere else.
      make_stable:  Actually make the ebuild stable.
    """
    written = False
    for line in fileinput.input(ebuild_path, inplace=1):
      # Has to be done here to get changes to sys.stdout from fileinput.input.
      if not redirect_file:
        redirect_file = sys.stdout

      # Always add variables at the top of the ebuild, before the first
      # nonblank line other than the EAPI line.
      if not written and not _blank_or_eapi_re.match(line):
        for key, value in sorted(variables.items()):
          assert key is not None and value is not None
          redirect_file.write('%s=%s\n' % (key, value))
        written = True

      # Mark KEYWORDS as stable by removing ~'s.
      if line.startswith('KEYWORDS=') and make_stable:
        line = line.replace('~', '')

      varname, eq, _ = line.partition('=')
      if not (eq == '=' and varname.strip() in variables):
        # Don't write out the old value of the variable.
        redirect_file.write(line)

    fileinput.close()

  @classmethod
  def MarkAsStable(cls, unstable_ebuild_path, new_stable_ebuild_path,
                   variables, redirect_file=None, make_stable=True):
    """Static function that creates a revved stable ebuild.

    This function assumes you have already figured out the name of the new
    stable ebuild path and then creates that file from the given unstable
    ebuild and marks it as stable.  If the commit_value is set, it also
    set the commit_keyword=commit_value pair in the ebuild.

    Args:
      unstable_ebuild_path: The path to the unstable ebuild.
      new_stable_ebuild_path:  The path you want to use for the new stable
        ebuild.
      variables: Dictionary of variables to update in ebuild.
      redirect_file:  Optionally redirect output of new ebuild somewhere else.
      make_stable:  Actually make the ebuild stable.
    """
    shutil.copyfile(unstable_ebuild_path, new_stable_ebuild_path)
    EBuild.UpdateEBuild(new_stable_ebuild_path, variables, redirect_file,
                        make_stable)

  @classmethod
  def CommitChange(cls, message, overlay):
    """Commits current changes in git locally with given commit message.

    Args:
        message: the commit string to write when committing to git.
        overlay: directory in which to commit the changes.
    Raises:
        RunCommandError: Error occurred while committing.
    """
    logging.info('Committing changes with commit message: %s', message)
    git_commit_cmd = ['git', 'commit', '-a', '-m', message]
    cros_build_lib.RunCommand(git_commit_cmd, cwd=overlay,
                              print_cmd=cls.VERBOSE)

  def __init__(self, path):
    """Sets up data about an ebuild from its path."""
    self._overlay, self._category, self._pkgname, filename = path.rsplit('/', 3)
    m = self._PACKAGE_VERSION_PATTERN.match(filename)
    if not m:
      raise EBuildVersionFormatException(filename)
    self.version, self.version_no_rev, revision = m.groups()
    if revision is not None:
      self.current_revision = int(revision.replace('-r', ''))
    else:
      self.current_revision = 0
    self.package = '%s/%s' % (self._category, self._pkgname)

    self._ebuild_path_no_version = os.path.join(
        os.path.dirname(path), self._pkgname)
    self.ebuild_path_no_revision = '%s-%s' % (
        self._ebuild_path_no_version, self.version_no_rev)
    self._unstable_ebuild_path = '%s-9999.ebuild' % (
        self._ebuild_path_no_version)
    self.ebuild_path = path

    self.is_workon = False
    self.is_stable = False
    self._ReadEBuild(path)

  def _ReadEBuild(self, path):
    """Determine the settings of `is_workon` and `is_stable`.

    `is_workon` is determined by whether the ebuild inherits from
    the 'cros-workon' eclass.  `is_stable` is determined by whether
    there's a '~' in the KEYWORDS setting in the ebuild.

    This function is separate from __init__() to allow unit tests to
    stub it out.
    """
    for line in fileinput.input(path):
      if line.startswith('inherit ') and 'cros-workon' in line:
        self.is_workon = True
      elif line.startswith('KEYWORDS='):
        for keyword in line.split('=', 1)[1].strip("\"'").split():
          if not keyword.startswith('~') and keyword != '-*':
            self.is_stable = True
    fileinput.close()

  def GetGitProjectName(self, path):
    """Read the project variable from a git repository at given path."""
    cmd = ('git config --get remote.cros.projectname || '
           'git config --get remote.cros-internal.projectname')
    return self._RunCommand(cmd, cwd=path, shell=True).rstrip()

  def GetSourcePath(self, srcroot):
    """Get the project and path for this ebuild.

    The path is guaranteed to exist, be a directory, and be absolute.
    """
    workon_vars = (
        'CROS_WORKON_LOCALNAME',
        'CROS_WORKON_PROJECT',
        'CROS_WORKON_SUBDIR',
    )
    env = {
        'CROS_WORKON_LOCALNAME': self._pkgname,
        'CROS_WORKON_PROJECT': self._pkgname,
        'CROS_WORKON_SUBDIR': '',
    }
    settings = osutils.SourceEnvironment(self._unstable_ebuild_path,
                                         workon_vars, env=env)
    localnames = settings['CROS_WORKON_LOCALNAME'].split(',')
    projects = settings['CROS_WORKON_PROJECT'].split(',')
    subdirs = settings['CROS_WORKON_SUBDIR'].split(',')

    # Sanity checks and completion.
    # Each project specification has to have the same amount of items.
    if len(projects) != len(localnames):
      raise EbuildFormatIncorrectException(self._unstable_ebuild_path,
          'Number of _PROJECT and _LOCALNAME items don\'t match.')
    # Subdir must be either 0,1 or len(project)
    if len(projects) != len(subdirs) and len(subdirs) > 1:
      raise EbuildFormatIncorrectException(self._unstable_ebuild_path,
          'Incorrect number of _SUBDIR items.')
    # If there's one, apply it to all.
    if len(subdirs) == 1:
      subdirs = subdirs * len(projects)
    # If there is none, make an empty list to avoid exceptions later.
    if len(subdirs) == 0:
      subdirs = [''] * len(projects)

    # Calculate srcdir.
    if self._category == 'chromeos-base':
      dir_ = 'platform'
    else:
      dir_ = 'third_party'

    subdir_paths = [os.path.realpath(os.path.join(srcroot, dir_, l, s))
                    for l, s in zip(localnames, subdirs)]

    for subdir_path, project in zip(subdir_paths, projects):
      if not os.path.isdir(subdir_path):
        cros_build_lib.Die('Source repository %s '
                           'for project %s does not exist.' % (subdir_path,
                                                               self._pkgname))
      # Verify that we're grabbing the commit id from the right project name.
      real_project = self.GetGitProjectName(subdir_path)
      if project != real_project:
        cros_build_lib.Die('Project name mismatch for %s '
                           '(found %s, expected %s)' % (subdir_path,
                                                        real_project,
                                                        project))
    return projects, subdir_paths

  def GetCommitId(self, srcdir):
    """Get the commit id for this ebuild."""
    output = self._RunCommand(['git', 'rev-parse', 'HEAD'], cwd=srcdir)
    if not output:
      cros_build_lib.Die('Cannot determine HEAD commit for %s' % srcdir)
    return output.rstrip()

  def GetTreeId(self, srcdir):
    """Get the SHA1 of the source tree for this ebuild.

    Unlike the commit hash, the SHA1 of the source tree is unaffected by the
    history of the repository, or by commit messages.
    """
    output = self._RunCommand(['git', 'log', '-1', '--format=%T'], cwd=srcdir)
    if not output:
      cros_build_lib.Die('Cannot determine HEAD tree hash for %s' % srcdir)
    return output.rstrip()

  def GetVersion(self, srcroot, default):
    """Get the base version number for this ebuild.

    The version is provided by the ebuild through a specific script in
    the $FILESDIR (chromeos-version.sh).
    """
    vers_script = os.path.join(os.path.dirname(self._ebuild_path_no_version),
                               'files', 'chromeos-version.sh')

    if not os.path.exists(vers_script):
      return default

    srcdirs = self.GetSourcePath(srcroot)[1]

    # The chromeos-version script will output a usable raw version number,
    # or nothing in case of error or no available version
    try:
      output = self._RunCommand([vers_script] + srcdirs).strip()
    except cros_build_lib.RunCommandError as e:
      cros_build_lib.Die('Package %s chromeos-version.sh failed: %s' %
                         (self._pkgname, e))

    if not output:
      cros_build_lib.Die('Package %s has a chromeos-version.sh script but '
                         'it returned no valid version for "%s"' %
                         (self._pkgname, ' '.join(srcdirs)))

    return output

  @staticmethod
  def FormatBashArray(unformatted_list):
    """Returns a python list in a bash array format.

    If the list only has one item, format as simple quoted value.
    That is both backwards-compatible and more readable.

    Args:
      unformatted_list: an iterable to format as a bash array. This variable
        has to be sanitized first, as we don't do any safeties.

    Returns:
      A text string that can be used by bash as array declaration.
    """
    if len(unformatted_list) > 1:
      return '("%s")' % '" "'.join(unformatted_list)
    else:
      return '"%s"' % unformatted_list[0]

  def RevWorkOnEBuild(self, srcroot, redirect_file=None):
    """Revs a workon ebuild given the git commit hash.

    By default this class overwrites a new ebuild given the normal
    ebuild rev'ing logic.  However, a user can specify a redirect_file
    to redirect the new stable ebuild to another file.

    Args:
        srcroot: full path to the 'src' subdirectory in the source
          repository.
        redirect_file: Optional file to write the new ebuild.  By default
          it is written using the standard rev'ing logic.  This file must be
          opened and closed by the caller.

    Raises:
        OSError: Error occurred while creating a new ebuild.
        IOError: Error occurred while writing to the new revved ebuild file.
    Returns:
      If the revved package is different than the old ebuild, return the full
      revved package name, including the version number. Otherwise, return None.
    """

    if self.is_stable:
      stable_version_no_rev = self.GetVersion(srcroot, self.version_no_rev)
    else:
      # If given unstable ebuild, use preferred version rather than 9999.
      stable_version_no_rev = self.GetVersion(srcroot, '0.0.1')

    new_version = '%s-r%d' % (
        stable_version_no_rev, self.current_revision + 1)
    new_stable_ebuild_path = '%s-%s.ebuild' % (
        self._ebuild_path_no_version, new_version)

    self._Print('Creating new stable ebuild %s' % new_stable_ebuild_path)
    if not os.path.exists(self._unstable_ebuild_path):
      cros_build_lib.Die('Missing unstable ebuild: %s' %
                         self._unstable_ebuild_path)

    srcdirs = self.GetSourcePath(srcroot)[1]
    commit_ids = map(self.GetCommitId, srcdirs)
    tree_ids = map(self.GetTreeId, srcdirs)
    variables = dict(CROS_WORKON_COMMIT=self.FormatBashArray(commit_ids),
                     CROS_WORKON_TREE=self.FormatBashArray(tree_ids))
    self.MarkAsStable(self._unstable_ebuild_path, new_stable_ebuild_path,
                      variables, redirect_file)

    old_ebuild_path = self.ebuild_path
    if filecmp.cmp(old_ebuild_path, new_stable_ebuild_path, shallow=False):
      os.unlink(new_stable_ebuild_path)
      return None
    else:
      self._Print('Adding new stable ebuild to git')
      self._RunCommand(['git', 'add', new_stable_ebuild_path],
                       cwd=self._overlay)

      if self.is_stable:
        self._Print('Removing old ebuild from git')
        self._RunCommand(['git', 'rm', old_ebuild_path],
                         cwd=self._overlay)

      message = _GIT_COMMIT_MESSAGE % (self.package, ','.join(commit_ids))
      self.CommitChange(message, self._overlay)
      return '%s-%s' % (self.package, new_version)

  @classmethod
  def GitRepoHasChanges(cls, directory):
    """Returns True if there are changes in the given directory."""
    # Refresh the index first. This squashes just metadata changes.
    cros_build_lib.RunCommand(['git', 'update-index', '-q', '--refresh'],
                              cwd=directory, print_cmd=cls.VERBOSE)
    ret_obj = cros_build_lib.RunCommand(
        ['git', 'diff-index', '--name-only', 'HEAD'], cwd=directory,
        print_cmd=cls.VERBOSE, redirect_stdout=True)
    return ret_obj.output not in [None, '']

  @staticmethod
  def _GetSHA1ForProject(manifest, project):
    """Get the latest SHA1 for a given project from Gerrit.

    This function looks up the remote and branch for a given project in the
    manifest, and uses this to lookup the SHA1 from Gerrit. This only makes
    sense for unpinned manifests.

    Args:
      manifest: git.ManifestCheckout object.
      project: Project to look up.

    Raises:
      Exception if the manifest is pinned.
    """
    helper = gerrit.GerritHelper.FromManifestProject(manifest, project)
    manifest_branch = manifest.GetAttributeForProject(project, 'revision')
    branch = git.StripRefsHeads(manifest_branch)
    return helper.GetLatestSHA1ForBranch(project, branch)

  @staticmethod
  def _GetEBuildProjects(buildroot, overlay_list, changes):
    """Calculate ebuild->project map for changed ebuilds.

    Args:
      buildroot: Path to root of build directory.
      overlay_list: List of all overlays.
      changes: Changes from Gerrit that are being pushed.

    Returns:
      A dictionary mapping changed ebuilds to lists of associated projects.
    """
    directory_src = os.path.join(buildroot, 'src')
    overlay_dict = dict((o, []) for o in overlay_list)
    BuildEBuildDictionary(overlay_dict, True, None)
    changed_projects = set(c.project for c in changes)
    ebuild_projects = {}
    for ebuilds in overlay_dict.itervalues():
      for ebuild in ebuilds:
        projects = ebuild.GetSourcePath(directory_src)[0]
        if changed_projects.intersection(projects):
          ebuild_projects[ebuild] = projects
    return ebuild_projects

  @classmethod
  def UpdateCommitHashesForChanges(cls, changes, buildroot):
    """Updates the commit hashes for the EBuilds uprevved in changes.

    Args:
      changes: Changes from Gerrit that are being pushed.
      buildroot: Path to root of build directory.
    """
    manifest = git.ManifestCheckout.Cached(buildroot)
    project_sha1s = {}
    overlay_list = FindOverlays(constants.BOTH_OVERLAYS, buildroot=buildroot)
    ebuild_projects = cls._GetEBuildProjects(buildroot, overlay_list, changes)
    for ebuild, projects in ebuild_projects.iteritems():
      for project in set(projects).difference(project_sha1s):
        project_sha1s[project] = cls._GetSHA1ForProject(manifest, project)
      sha1s = [project_sha1s[project] for project in projects]
      logging.info('Updating ebuild for project %s with commit hashes %r',
                   ebuild.package, sha1s)
      updates = dict(CROS_WORKON_COMMIT=cls.FormatBashArray(sha1s))
      EBuild.UpdateEBuild(ebuild.ebuild_path, updates)

    # Commit any changes to all overlays.
    for overlay in overlay_list:
      if EBuild.GitRepoHasChanges(overlay):
        EBuild.CommitChange('Updating commit hashes in ebuilds '
                            'to match remote repository.', overlay=overlay)


def BestEBuild(ebuilds):
  """Returns the newest EBuild from a list of EBuild objects."""
  from portage.versions import vercmp
  winner = ebuilds[0]
  for ebuild in ebuilds[1:]:
    if vercmp(winner.version, ebuild.version) < 0:
      winner = ebuild
  return winner


def _FindUprevCandidates(files, blacklist):
  """Return the uprev candidate ebuild from a specified list of files.

  Usually an uprev candidate is a the stable ebuild in a cros_workon
  directory.  However, if no such stable ebuild exists (someone just
  checked in the 9999 ebuild), this is the unstable ebuild.

  If the package isn't a cros_workon package, return None.

  Args:
    files: List of files in a package directory.
  """
  stable_ebuilds = []
  unstable_ebuilds = []
  for path in files:
    if not path.endswith('.ebuild') or os.path.islink(path):
      continue
    ebuild = EBuild(path)
    if not ebuild.is_workon or blacklist.IsPackageBlackListed(path):
      continue
    if ebuild.is_stable:
      if ebuild.version == '9999':
        cros_build_lib.Die('KEYWORDS in 9999 ebuild should not be stable %s'
                           % path)
      stable_ebuilds.append(ebuild)
    else:
      unstable_ebuilds.append(ebuild)

  # If both ebuild lists are empty, the passed in file list was for
  # a non-workon package.
  if not unstable_ebuilds:
    if stable_ebuilds:
      path = os.path.dirname(stable_ebuilds[0].ebuild_path)
      cros_build_lib.Die('Missing 9999 ebuild in %s' % path)
    return None

  path = os.path.dirname(unstable_ebuilds[0].ebuild_path)
  if len(unstable_ebuilds) > 1:
    cros_build_lib.Die('Found multiple unstable ebuilds in %s' % path)

  if not stable_ebuilds:
    cros_build_lib.Warning('Missing stable ebuild in %s' % path)
    return unstable_ebuilds[0]

  if len(stable_ebuilds) == 1:
    return stable_ebuilds[0]

  stable_versions = set(ebuild.version_no_rev for ebuild in stable_ebuilds)
  if len(stable_versions) > 1:
    package = stable_ebuilds[0].package
    message = 'Found multiple stable ebuild versions in %s:' % path
    for version in stable_versions:
      message += '\n    %s-%s' % (package, version)
    cros_build_lib.Die(message)

  uprev_ebuild = max(stable_ebuilds, key=lambda eb: eb.current_revision)
  for ebuild in stable_ebuilds:
    if ebuild != uprev_ebuild:
      cros_build_lib.Warning('Ignoring stable ebuild revision %s in %s' %
                             (ebuild.version, path))
  return uprev_ebuild


def BuildEBuildDictionary(overlays, use_all, packages):
  """Build a dictionary of the ebuilds in the specified overlays.

  overlays: A map which maps overlay directories to arrays of stable EBuilds
    inside said directories.
  use_all: Whether to include all ebuilds in the specified directories.
    If true, then we gather all packages in the directories regardless
    of whether they are in our set of packages.
  packages: A set of the packages we want to gather.  If use_all is
    True, this argument is ignored, and should be None.
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


def RegenCache(overlay):
  """Regenerate the cache of the specified overlay.

  overlay: The tree to regenerate the cache for.
  """
  repo_name = GetOverlayName(overlay)
  if not repo_name:
    return

  layout = cros_build_lib.LoadKeyValueFile('%s/metadata/layout.conf' % overlay,
                                           ignore_missing=True)
  if layout.get('cache-format') != 'md5-dict':
    return

  # Regen for the whole repo.
  cros_build_lib.RunCommand(['egencache', '--update', '--repo', repo_name,
                             '--jobs', str(multiprocessing.cpu_count())])
  # If there was nothing new generated, then let's just bail.
  result = cros_build_lib.RunCommand(['git', 'status', '-s', 'metadata/'],
                                     cwd=overlay, redirect_stdout=True)
  if not result.output:
    return
  # Explicitly add any new files to the index.
  cros_build_lib.RunCommand(['git', 'add', 'metadata/'], cwd=overlay)
  # Explicitly tell git to also include rm-ed files.
  cros_build_lib.RunCommand(['git', 'commit', '-m', 'regen cache',
                             'metadata/'], cwd=overlay)


def ParseBashArray(value):
  """Parse a valid bash array into python list."""
  # The syntax for bash arrays is nontrivial, so let's use bash to do the
  # heavy lifting for us.
  sep = ','
  # Because %s may contain bash comments (#), put a clever newline in the way.
  cmd = 'ARR=%s\nIFS=%s; echo -n "${ARR[*]}"' % (value, sep)
  return cros_build_lib.RunCommandCaptureOutput(
      cmd, print_cmd=False, shell=True).output.split(sep)


def GetWorkonProjectMap(overlay, subdirectories):
  """Get the project -> ebuild mapping for cros_workon ebuilds.

  Args:
    overlay: Overlay to look at.
    subdirectories: List of subdirectories to look in on the overlay.

  Returns:
    A list of (filename, projects) tuples for cros-workon ebuilds in the
    given overlay under the given subdirectories.
  """
  # Search ebuilds for project names, ignoring non-existent directories.
  cmd = ['grep', '^CROS_WORKON_PROJECT=', '--include', '*-9999.ebuild',
         '-Hsr'] + list(subdirectories)
  result = cros_build_lib.RunCommandCaptureOutput(
      cmd, cwd=overlay, error_code_ok=True, print_cmd=False)
  for grep_line in result.output.splitlines():
    filename, _, line = grep_line.partition(':')
    value = line.partition('=')[2]
    projects = ParseBashArray(value)
    yield filename, projects


def SplitEbuildPath(path):
  """Split an ebuild path into its components.

  Given a specified ebuild filename, returns $CATEGORY, $PN, $P. It does not
  perform any check on ebuild name elements or their validity, merely splits
  a filename, absolute or relative, and returns the last 3 components.

  Example: For /any/path/chromeos-base/power_manager/power_manager-9999.ebuild,
  returns ('chromeos-base', 'power_manager', 'power_manager-9999').

  Returns:
    $CATEGORY, $PN, $P
  """
  return os.path.splitext(path)[0].rsplit('/', 3)[-3:]


def SplitPV(pv):
  """Takes a PV value and splits it into individual components.

  Returns:
    A collection with named members:
      pv, package, version, version_no_rev, rev
  """
  m = _pvr_re.match(pv)
  if m is None:
    return None
  return PV(**m.groupdict())


def SplitCPV(cpv):
  """Splits a CPV value into components.

  Returns:
    A collection with named members:
      category, pv, package, version, version_no_rev, rev
  """
  (category, pv) = cpv.split('/', 1)
  m = SplitPV(pv)
  if m is None:
    return None
  # pylint: disable=W0212
  return CPV(category=category, **m._asdict())


def FindWorkonProjects(packages):
  """Find the projects associated with the specified cros_workon packages.

  Args:
    packages: List of cros_workon packages.

  Returns:
    The set of projects associated with the specified cros_workon packages.
  """
  all_projects = set()
  buildroot, both = constants.SOURCE_ROOT, constants.BOTH_OVERLAYS
  for overlay in FindOverlays(both, buildroot=buildroot):
    for _, projects in GetWorkonProjectMap(overlay, packages):
      all_projects.update(projects)
  return all_projects


def ListInstalledPackages(sysroot):
  """Lists all portage packages in a given portage-managed root.

  Assumes the existence of a /var/db/pkg package database.

  Args:
    sysroot: The root being inspected.

  Returns:
    A list of (cp,v) tuples in the given sysroot.
  """
  vdb_path = os.path.join(sysroot, 'var/db/pkg')
  ebuild_pattern = os.path.join(vdb_path, '*/*/*.ebuild')
  packages = []
  for path in glob.glob(ebuild_pattern):
    category, package, packagecheck = SplitEbuildPath(path)
    pv = SplitPV(package)
    if package == packagecheck and pv is not None:
      packages.append(('%s/%s' % (category, pv.package), pv.version))
  return packages


def BestVisible(atom, board=None, buildroot=constants.SOURCE_ROOT):
  """Get the best visible ebuild CPV for the given atom.

  Args:
    atom: Portage atom.
    board: Board to look at. By default, look in chroot.
    root: Directory

  Returns:
    A CPV object.
  """
  portageq = 'portageq' if board is None else 'portageq-%s' % board
  root = '/' if board is None else '/build/%s' % board
  cmd = [portageq, 'best_visible', root, 'ebuild', atom]
  result = cros_build_lib.RunCommandCaptureOutput(
      cmd, cwd=buildroot, enter_chroot=True, debug_level=logging.DEBUG)
  return SplitCPV(result.output.strip())
