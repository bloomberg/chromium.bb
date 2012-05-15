# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Routines and classes for working with Portage overlays and ebuilds."""

import filecmp
import fileinput
import logging
import multiprocessing
import os
import re
import shutil
import sys

from chromite.lib import cros_build_lib
from chromite.buildbot import gerrit_helper

_CACHE_OVERLAY = '%(build_root)s/src/third_party/portage-stable'
_PUBLIC_OVERLAY = '%(build_root)s/src/third_party/chromiumos-overlay'
_OVERLAY_LIST_CMD = '%(build_root)s/src/platform/dev/host/cros_overlay_list'

# Takes two strings, package_name and commit_id.
_GIT_COMMIT_MESSAGE = 'Marking 9999 ebuild for %s with commit %s as stable.'


def FindOverlays(srcroot, overlay_type):
  """Return the list of overlays to use for a given buildbot.

  Args:
    overlay_type: A string describing which overlays you want.
              'private': Just the private overlay.
              'public': Just the public overlay.
              'both': Both the public and private overlays.
  """
  # we use a dictionary to allow tests to override _OVERLAY_LIST_CMD;
  # see the cbuildbot_stages and portage_utilities unit tests.
  format_args = { 'build_root' : srcroot }
  cmd = _OVERLAY_LIST_CMD % format_args
  # Check in case we haven't checked out the source yet.
  if not os.path.exists(cmd):
    return []

  cmd_argv = [cmd, '--all_boards']
  if overlay_type == 'private':
    cmd_argv.append('--nopublic')
  elif overlay_type == 'public':
    cmd_argv.append('--noprivate')
  elif overlay_type != 'both':
    return []

  overlays = cros_build_lib.RunCommand(
      cmd_argv, redirect_stdout=True, print_cmd=False).output.split()
  if overlay_type != 'private':
    # TODO(davidjames): cros_overlay_list should include chromiumos-overlay in
    #                   its list of public overlays. But it doesn't yet...
    overlays.append(_PUBLIC_OVERLAY % format_args)
    overlays.append(_CACHE_OVERLAY % format_args)
  return overlays


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

      # Always add variables at the top of the ebuild, before any un-commented
      # lines.
      if not written and not line.startswith('#'):
        for key, value in sorted(variables.items()):
          assert key is not None and value is not None
          redirect_file.write('%s="%s"\n' % (key, value))
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

  # TODO: Modify cros_mark_as_stable to no longer require chdir's and remove the
  # default '.' behavior from this method.
  @classmethod
  def CommitChange(cls, message, overlay='.'):
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
    _path, self._category, self._pkgname, filename = path.rsplit('/', 3)
    m = self._PACKAGE_VERSION_PATTERN.match(filename)
    if not m:
      raise EBuildVersionFormatException(filename)
    self.version, self._version_no_rev, revision = m.groups()
    if revision is not None:
      self.current_revision = int(revision.replace('-r', ''))
    else:
      self.current_revision = 0
    self.package = '%s/%s' % (self._category, self._pkgname)

    self._ebuild_path_no_version = os.path.join(
        os.path.dirname(path), self._pkgname)
    self.ebuild_path_no_revision = '%s-%s' % (
        self._ebuild_path_no_version, self._version_no_rev)
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
      elif (line.startswith('KEYWORDS=') and '~' not in line and
            ('amd64' in line or 'x86' in line or 'arm' in line)):
        self.is_stable = True
    fileinput.close()

  def GetSourcePath(self, srcroot):
    """Get the project and path for this ebuild.

    The path is guaranteed to exist, be a directory, and be absolute.
    """
    # Grab and evaluate CROS_WORKON variables from this ebuild.
    cmd = ('export CROS_WORKON_LOCALNAME="%s" CROS_WORKON_PROJECT="%s"; '
           'eval $(grep -E "^CROS_WORKON" %s) && '
           'echo $CROS_WORKON_PROJECT '
           '$CROS_WORKON_LOCALNAME/$CROS_WORKON_SUBDIR'
           % (self._pkgname, self._pkgname, self._unstable_ebuild_path))
    project, subdir = self._RunCommand(cmd, shell=True).split()

    # Calculate srcdir.
    if self._category == 'chromeos-base':
      dir_ = 'platform'
    else:
      dir_ = 'third_party'

    subdir_path = os.path.realpath(os.path.join(srcroot, dir_, subdir))

    if not os.path.isdir(subdir_path):
      cros_build_lib.Die('Source repository %s '
                         'for project %s does not exist.' % (
                            subdir_path, self._pkgname))

    # Verify that we're grabbing the commit id from the right project name.
    cmd = ('git config --get remote.cros.projectname || '
           'git config --get remote.cros-internal.projectname')
    real_project = self._RunCommand(cmd, cwd=subdir_path, shell=True).rstrip()
    if project != real_project:
      cros_build_lib.Die('Project name mismatch for %s '
                         '(found %s, expected %s)' % (
                             subdir_path, real_project, project))

    return project, subdir_path

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

    _project, srcdir = self.GetSourcePath(srcroot)

    # The chromeos-version script will output a usable raw version number,
    # or nothing in case of error or no available version
    output = self._RunCommand([vers_script, srcdir]).strip()

    if not output:
      cros_build_lib.Die('Package %s has a chromeos-version.sh script but '
                         'it returned no valid version for "%s"' %
                         (self._pkgname, srcdir))

    return output

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
      stable_version_no_rev = self.GetVersion(srcroot, self._version_no_rev)
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

    srcdir = self.GetSourcePath(srcroot)[1]
    commit_id = self.GetCommitId(srcdir)
    variables = dict(CROS_WORKON_COMMIT=commit_id,
                     CROS_WORKON_TREE=self.GetTreeId(srcdir))
    self.MarkAsStable(self._unstable_ebuild_path, new_stable_ebuild_path,
                      variables, redirect_file)

    old_ebuild_path = self.ebuild_path
    if filecmp.cmp(old_ebuild_path, new_stable_ebuild_path, shallow=False):
      os.unlink(new_stable_ebuild_path)
      return None
    else:
      self._Print('Adding new stable ebuild to git')
      self._RunCommand(['git', 'add', new_stable_ebuild_path])

      if self.is_stable:
        self._Print('Removing old ebuild from git')
        self._RunCommand(['git', 'rm', old_ebuild_path])

      message = _GIT_COMMIT_MESSAGE % (self.package, commit_id)
      self.CommitChange(message)
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

  @classmethod
  def UpdateCommitHashesForChanges(cls, changes, buildroot):
    """Updates the commit hashes for the EBuilds uprevved in changes."""
    overlay_list = FindOverlays(buildroot, 'both')
    directory_src = os.path.join(buildroot, 'src')
    overlay_dict = dict((o, []) for o in overlay_list)
    BuildEBuildDictionary(overlay_dict, True, None)

    # Generate a project->ebuild map for only projects affected by given
    # changes.
    project_ebuild_map = dict((c.project, []) for c in changes)
    modified_overlays = set()
    for overlay, ebuilds in overlay_dict.items():
      for ebuild in ebuilds:
        project = ebuild.GetSourcePath(directory_src)[0]
        project_ebuilds = project_ebuild_map.get(project)
        # This is not None if there already is an entry in project_ebuilds
        # for the given project.
        if project_ebuilds is not None:
          project_ebuilds.append(ebuild)
          modified_overlays.add(overlay)

    # Now go through each change and update the ebuilds they affect.
    for change in changes:
      ebuilds_for_change = project_ebuild_map[change.project]
      if len(ebuilds_for_change) == 0:
        continue

      helper = gerrit_helper.GetGerritHelperForChange(change)
      latest_sha1 = helper.GetLatestSHA1ForBranch(change.project,
                                                  change.tracking_branch)
      for ebuild in ebuilds_for_change:
        logging.info('Updating ebuild for project %s with commit hash %s',
                     ebuild.package, latest_sha1)
        EBuild.UpdateEBuild(ebuild.ebuild_path,
                            dict(CROS_WORKON_COMMIT=latest_sha1))

    for overlay in modified_overlays:
      # For commits to repos that merge, there won't actually be any changes.
      # Filter these out.
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

  stable_versions = set(ebuild._version_no_rev for ebuild in stable_ebuilds)
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

  cache_config = ['cache-format', '=', 'md5-dict']

  layout_conf = '%s/metadata/layout.conf' % overlay
  if not os.path.exists(layout_conf):
    return

  gencache = False
  with open(layout_conf) as f:
    for line in f:
      line = line.split('#')[0].split()
      if line == cache_config:
        gencache = True
        break
  if not gencache:
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
