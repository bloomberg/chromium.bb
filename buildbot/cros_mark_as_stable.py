#!/usr/bin/python

# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module uprevs a given package's ebuild to the next revision."""

import constants
import filecmp
import fileinput
import optparse
import os
import re
import shutil
import subprocess
import sys

if __name__ == '__main__':
  sys.path.append(constants.SOURCE_ROOT)

from chromite.buildbot import cbuildbot_commands as commands
from chromite.lib import cros_build_lib



# TODO(sosa): Remove during OO refactor.
VERBOSE = False

# Takes two strings, package_name and commit_id.
_GIT_COMMIT_MESSAGE = 'Marking 9999 ebuild for %s with commit %s as stable.'

# Dictionary of valid commands with usage information.
COMMAND_DICTIONARY = {
                        'clean':
                          'Cleans up previous calls to either commit or push',
                        'commit':
                          'Marks given ebuilds as stable locally',
                        'push':
                          'Pushes previous marking of ebuilds to remote repo',
                      }

# Name used for stabilizing branch.
STABLE_BRANCH_NAME = 'stabilizing_branch'


def BestEBuild(ebuilds):
  """Returns the newest EBuild from a list of EBuild objects."""
  from portage.versions import vercmp
  winner = ebuilds[0]
  for ebuild in ebuilds[1:]:
    if vercmp(winner.version, ebuild.version) < 0:
      winner = ebuild
  return winner

# ======================= Global Helper Functions ========================


def _Print(message):
  """Verbose print function."""
  if VERBOSE:
    cros_build_lib.Info(message)


def CleanStalePackages(board, package_atoms):
  """Cleans up stale package info from a previous build.
  Args:
    board: Board to clean the packages from.
    package_atoms: The actual package atom to unmerge.
  """
  if package_atoms:
    cros_build_lib.Info('Cleaning up stale packages %s.' % package_atoms)
    unmerge_board_cmd = ['emerge-%s' % board, '--unmerge']
    unmerge_board_cmd.extend(package_atoms)
    cros_build_lib.RunCommand(unmerge_board_cmd)

    unmerge_host_cmd = ['sudo', 'emerge', '--unmerge']
    unmerge_host_cmd.extend(package_atoms)
    cros_build_lib.RunCommand(unmerge_host_cmd)

  cros_build_lib.RunCommand(['eclean-%s' % board, '-d', 'packages'],
                            redirect_stderr=True)
  cros_build_lib.RunCommand(['sudo', 'eclean', '-d', 'packages'],
                            redirect_stderr=True)


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

    for re in self.black_list_re_array:
      if re.match(path_to_ebuild):
        return True

    return False


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


def _BuildEBuildDictionary(overlays, all, packages, blacklist):
  """Build a dictionary of the ebuilds in the specified overlays.

  overlays: A map which maps overlay directories to arrays of stable EBuilds
    inside said directories.
  all: Whether to include all ebuilds in the specified directories. If true,
    then we gather all packages in the directories regardless of whether
    they are in our set of packages.
  packages: A set of the packages we want to gather.
  """
  for overlay in overlays:
    for package_dir, unused_dirs, files in os.walk(overlay):
      # Add stable ebuilds to overlays[overlay].
      paths = [os.path.join(package_dir, path) for path in files]
      ebuild = _FindUprevCandidates(paths, blacklist)

      # If the --all option isn't used, we only want to update packages that
      # are in packages.
      if ebuild and (all or ebuild.package in packages):
        overlays[overlay].append(ebuild)


def _DoWeHaveLocalCommits(stable_branch, tracking_branch):
  """Returns true if there are local commits."""
  current_branch = _SimpleRunCommand('git branch | grep \*').split()[1]
  if current_branch == stable_branch:
    current_commit_id = _SimpleRunCommand('git rev-parse HEAD')
    tracking_commit_id = _SimpleRunCommand('git rev-parse %s' % tracking_branch)
    return current_commit_id != tracking_commit_id
  else:
    return False


def _CheckSaneArguments(package_list, command, options):
  """Checks to make sure the flags are sane.  Dies if arguments are not sane."""
  if not command in COMMAND_DICTIONARY.keys():
    _PrintUsageAndDie('%s is not a valid command' % command)
  if not options.packages and command == 'commit' and not options.all:
    _PrintUsageAndDie('Please specify at least one package')
  if not options.board and command == 'commit':
    _PrintUsageAndDie('Please specify a board')
  if not os.path.isdir(options.srcroot):
    _PrintUsageAndDie('srcroot is not a valid path')
  options.srcroot = os.path.abspath(options.srcroot)


def _PrintUsageAndDie(error_message=''):
  """Prints optional error_message the usage and returns an error exit code."""
  command_usage = 'Commands: \n'
  # Add keys and usage information from dictionary.
  commands = sorted(COMMAND_DICTIONARY.keys())
  for command in commands:
    command_usage += '  %s: %s\n' % (command, COMMAND_DICTIONARY[command])
  commands_str = '|'.join(commands)
  cros_build_lib.Warning('Usage: %s FLAGS [%s]\n\n%s' % (
      sys.argv[0], commands_str, command_usage))
  if error_message:
    cros_build_lib.Die(error_message)
  else:
    sys.exit(1)


def _SimpleRunCommand(command):
  """Runs a shell command and returns stdout back to caller."""
  _Print('  + %s' % command)
  proc_handle = subprocess.Popen(command, stdout=subprocess.PIPE, shell=True)
  stdout = proc_handle.communicate()[0]
  retcode = proc_handle.wait()
  if retcode != 0:
    _Print(stdout)
    raise subprocess.CalledProcessError(retcode, command)
  return stdout


# ======================= End Global Helper Functions ========================


def Clean(tracking_branch):
  """Cleans up uncommitted changes.

  Args:
    tracking_branch:  The tracking branch we want to return to after the call.
  """
  # Safety case in case we got into a bad state with a previous build.
  try:
    _SimpleRunCommand('git rebase --abort')
  except:
    pass

  _SimpleRunCommand('git reset HEAD --hard')
  branch = GitBranch(STABLE_BRANCH_NAME, tracking_branch)
  if branch.Exists():
    GitBranch.Checkout(branch)
    branch.Delete()


def PushChange(stable_branch, tracking_branch, dryrun):
  """Pushes commits in the stable_branch to the remote git repository.

  Pushes locals commits from calls to CommitChange to the remote git
  repository specified by current working directory.

  Args:
    stable_branch: The local branch with commits we want to push.
    tracking_branch: The tracking branch of the local branch.
    dryrun: Use git push --dryrun to emulate a push.
  Raises:
      OSError: Error occurred while pushing.
  """
  # Sanity check to make sure we're on a stabilizing branch before pushing.
  if not _DoWeHaveLocalCommits(stable_branch, tracking_branch):
    cros_build_lib.Info('Not work found to push.  Exiting')
    return

  description = _SimpleRunCommand('git log --format=format:%s%n%n%b ' +
                                  tracking_branch + '..')
  description = 'Marking set of ebuilds as stable\n\n%s' % description
  cros_build_lib.Info('Using description %s' % description)
  merge_branch_name = 'merge_branch'
  merge_branch = GitBranch(merge_branch_name, tracking_branch)
  if merge_branch.Exists():
    merge_branch.Delete()
  _SimpleRunCommand('repo sync .')
  merge_branch.CreateBranch()
  if not merge_branch.Exists():
    cros_build_lib.Die('Unable to create merge branch.')
  _SimpleRunCommand('git merge --squash %s' % stable_branch)
  _SimpleRunCommand('git commit -m "%s"' % description)
  _SimpleRunCommand('git config push.default tracking')
  cros_build_lib.GitPushWithRetry(merge_branch_name, cwd='.', dryrun=dryrun)


class GitBranch(object):
  """Wrapper class for a git branch."""

  def __init__(self, branch_name, tracking_branch):
    """Sets up variables but does not create the branch."""
    self.branch_name = branch_name
    self.tracking_branch = tracking_branch

  def CreateBranch(self):
    GitBranch.Checkout(self)

  @classmethod
  def Checkout(cls, target):
    """Function used to check out to another GitBranch."""
    if target.branch_name == target.tracking_branch or target.Exists():
      git_cmd = 'git checkout %s -f' % target.branch_name
    else:
      git_cmd = 'git checkout -b %s %s -f' % (target.branch_name,
                                              target.tracking_branch)
    _SimpleRunCommand(git_cmd)

  def Exists(self):
    """Returns True if the branch exists."""
    branch_cmd = 'git branch'
    branches = _SimpleRunCommand(branch_cmd)
    return self.branch_name in branches.split()

  def Delete(self):
    """Deletes the branch and returns the user to the master branch.

    Returns True on success.
    """
    tracking_branch = GitBranch(self.tracking_branch, self.tracking_branch)
    GitBranch.Checkout(tracking_branch)
    delete_cmd = 'git branch -D %s' % self.branch_name
    _SimpleRunCommand(delete_cmd)


class EBuild(object):
  """Wrapper class for information about an ebuild."""

  def __init__(self, path):
    """Sets up data about an ebuild from its path."""
    from portage.versions import pkgsplit
    unused_path, self.category, self.pkgname, filename = path.rsplit('/', 3)
    unused_pkgname, self.version_no_rev, rev = pkgsplit(
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

  def GetCommitId(self, srcroot):
    """Get the commit id for this ebuild."""
    # Grab and evaluate CROS_WORKON variables from this ebuild.
    unstable_ebuild = '%s-9999.ebuild' % self.ebuild_path_no_version
    cmd = ('export CROS_WORKON_LOCALNAME="%s" CROS_WORKON_PROJECT="%s"; '
           'eval $(grep -E "^CROS_WORKON" %s) && '
           'echo $CROS_WORKON_PROJECT '
           '$CROS_WORKON_LOCALNAME/$CROS_WORKON_SUBDIR'
           % (self.pkgname, self.pkgname, unstable_ebuild))
    project, subdir = _SimpleRunCommand(cmd).split()

    # Calculate srcdir.
    if self.category == 'chromeos-base':
      dir = 'platform'
    else:
      dir = 'third_party'

    srcdir = os.path.join(srcroot, dir, subdir)

    if not os.path.isdir(srcdir):
      cros_build_lib.Die('Cannot find commit id for %s' % self.ebuild_path)

    # Verify that we're grabbing the commit id from the right project name.
    # NOTE: chromeos-kernel has the wrong project name, so it fails this
    # check.
    # TODO(davidjames): Fix the project name in the chromeos-kernel ebuild.
    cmd = ('cd %s && ( git config --get remote.cros.projectname || '
           'git config --get remote.cros-internal.projectname )') % srcdir
    actual_project = _SimpleRunCommand(cmd).rstrip()
    if project not in (actual_project, 'chromeos-kernel'):
      cros_build_lib.Die('Project name mismatch for %s (%s != %s)' % (
          unstable_ebuild, project, actual_project))

    # Get commit id.
    output = _SimpleRunCommand('cd %s && git rev-parse HEAD' % srcdir)
    if not output:
      cros_build_lib.Die('Missing commit id for %s' % self.ebuild_path)
    return output.rstrip()


class EBuildStableMarker(object):
  """Class that revs the ebuild and commits locally or pushes the change."""

  def __init__(self, ebuild):
    assert ebuild
    self._ebuild = ebuild

  @classmethod
  def MarkAsStable(cls, unstable_ebuild_path, new_stable_ebuild_path,
                   commit_keyword, commit_value, redirect_file=None,
                   make_stable=True):
    """Static function that creates a revved stable ebuild.

    This function assumes you have already figured out the name of the new
    stable ebuild path and then creates that file from the given unstable
    ebuild and marks it as stable.  If the commit_value is set, it also
    set the commit_keyword=commit_value pair in the ebuild.

    Args:
      unstable_ebuild_path: The path to the unstable ebuild.
      new_stable_ebuild_path:  The path you want to use for the new stable
        ebuild.
      commit_keyword: Optional keyword to set in the ebuild to mark it as
        stable.
      commit_value: Value to set the above keyword to.
      redirect_file:  Optionally redirect output of new ebuild somewhere else.
      make_stable:  Actually make the ebuild stable.
    """
    shutil.copyfile(unstable_ebuild_path, new_stable_ebuild_path)
    for line in fileinput.input(new_stable_ebuild_path, inplace=1):
      # Has to be done here to get changes to sys.stdout from fileinput.input.
      if not redirect_file:
        redirect_file = sys.stdout
      if line.startswith('KEYWORDS'):
        # Actually mark this file as stable by removing ~'s.
        if make_stable:
          redirect_file.write(line.replace('~', ''))
        else:
          redirect_file.write(line)
      elif line.startswith('EAPI'):
        # Always add new commit_id after EAPI definition.
        redirect_file.write(line)
        if commit_keyword and commit_value:
          redirect_file.write('%s="%s"\n' % (commit_keyword, commit_value))
      elif not line.startswith(commit_keyword):
        # Skip old commit_keyword definition.
        redirect_file.write(line)
    fileinput.close()

  def RevWorkOnEBuild(self, commit_id, redirect_file=None):
    """Revs a workon ebuild given the git commit hash.

    By default this class overwrites a new ebuild given the normal
    ebuild rev'ing logic.  However, a user can specify a redirect_file
    to redirect the new stable ebuild to another file.

    Args:
        commit_id: String corresponding to the commit hash of the developer
          package to rev.
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
    if self._ebuild.is_stable:
      stable_version_no_rev = self._ebuild.version_no_rev
    else:
      # If given unstable ebuild, use 0.0.1 rather than 9999.
      stable_version_no_rev = '0.0.1'

    new_version = '%s-r%d' % (stable_version_no_rev,
                              self._ebuild.current_revision + 1)
    new_stable_ebuild_path = '%s-%s.ebuild' % (
        self._ebuild.ebuild_path_no_version, new_version)

    _Print('Creating new stable ebuild %s' % new_stable_ebuild_path)
    unstable_ebuild_path = ('%s-9999.ebuild' %
                            self._ebuild.ebuild_path_no_version)
    if not os.path.exists(unstable_ebuild_path):
      cros_build_lib.Die('Missing unstable ebuild: %s' % unstable_ebuild_path)

    self.MarkAsStable(unstable_ebuild_path, new_stable_ebuild_path,
                      'CROS_WORKON_COMMIT', commit_id, redirect_file)

    old_ebuild_path = self._ebuild.ebuild_path
    if filecmp.cmp(old_ebuild_path, new_stable_ebuild_path, shallow=False):
      os.unlink(new_stable_ebuild_path)
      return None
    else:
      _Print('Adding new stable ebuild to git')
      _SimpleRunCommand('git add %s' % new_stable_ebuild_path)

      if self._ebuild.is_stable:
        _Print('Removing old ebuild from git')
        _SimpleRunCommand('git rm %s' % old_ebuild_path)

      return '%s-%s' % (self._ebuild.package, new_version)

  @classmethod
  def CommitChange(cls, message):
    """Commits current changes in git locally with given commit message.

    Args:
        message: the commit string to write when committing to git.

    Raises:
        OSError: Error occurred while committing.
    """
    cros_build_lib.Info('Committing changes with commit message: %s' % message)
    git_commit_cmd = 'git commit -am "%s"' % message
    _SimpleRunCommand(git_commit_cmd)


def main(argv):
  parser = optparse.OptionParser('cros_mark_as_stable OPTIONS packages')
  parser.add_option('--all', action='store_true',
                    help='Mark all packages as stable.')
  parser.add_option('-b', '--board',
                    help='Board for which the package belongs.')
  parser.add_option('--drop_file',
                    help='File to list packages that were revved.')
  parser.add_option('--dryrun', action='store_true',
                    help='Passes dry-run to git push if pushing a change.')
  parser.add_option('-o', '--overlays',
                    help='Colon-separated list of overlays to modify.')
  parser.add_option('-p', '--packages',
                    help='Colon separated list of packages to rev.')
  parser.add_option('-r', '--srcroot',
                    default='%s/trunk/src' % os.environ['HOME'],
                    help='Path to root src directory.')
  parser.add_option('--verbose', action='store_true',
                    help='Prints out debug info.')
  (options, args) = parser.parse_args()

  global VERBOSE
  VERBOSE = options.verbose

  if len(args) != 1:
    _PrintUsageAndDie('Must specify a valid command [commit, clean, push]')

  command = args[0]
  package_list = None
  if options.packages:
    package_list = options.packages.split(':')

  _CheckSaneArguments(package_list, command, options)
  if options.overlays:
    overlays = {}
    for path in options.overlays.split(':'):
      if command != 'clean' and not os.path.isdir(path):
        cros_build_lib.Die('Cannot find overlay: %s' % path)
      overlays[path] = []
  else:
    cros_build_lib.Warning('Missing --overlays argument')
    overlays = {
      '%s/private-overlays/chromeos-overlay' % options.srcroot: [],
      '%s/third_party/chromiumos-overlay' % options.srcroot: []
    }

  if command == 'commit':
    blacklist = _BlackListManager()
    _BuildEBuildDictionary(overlays, options.all, package_list, blacklist)

  file_dir = os.path.dirname(os.path.realpath(__file__))
  tracking_branch = 'remotes/m/%s' % commands.GetManifestBranch(file_dir)

  for overlay, ebuilds in overlays.items():
    if not os.path.isdir(overlay):
      cros_build_lib.Warning("Skipping %s" % overlay)
      continue

    # TODO(davidjames): Currently, all code that interacts with git depends on
    # the cwd being set to the overlay directory. We should instead pass in
    # this parameter so that we don't need to modify the cwd globally.
    os.chdir(overlay)

    if command == 'clean':
      Clean(tracking_branch)
    elif command == 'push':
      PushChange(STABLE_BRANCH_NAME, tracking_branch, options.dryrun)
    elif command == 'commit' and ebuilds:
      work_branch = GitBranch(STABLE_BRANCH_NAME, tracking_branch)
      work_branch.CreateBranch()
      if not work_branch.Exists():
        cros_build_lib.Die('Unable to create stabilizing branch in %s' %
                           overlay)

      # Contains the array of packages we actually revved.
      revved_packages = []
      new_package_atoms = []
      for ebuild in ebuilds:
        try:
          _Print('Working on %s' % ebuild.package)
          worker = EBuildStableMarker(ebuild)
          commit_id = ebuild.GetCommitId(options.srcroot)
          new_package = worker.RevWorkOnEBuild(commit_id)
          if new_package:
            message = _GIT_COMMIT_MESSAGE % (ebuild.package, commit_id)
            worker.CommitChange(message)
            revved_packages.append(ebuild.package)
            new_package_atoms.append('=%s' % new_package)
        except (OSError, IOError):
          cros_build_lib.Warning('Cannot rev %s\n' % ebuild.package,
                  'Note you will have to go into %s '
                  'and reset the git repo yourself.' % overlay)
          raise

      CleanStalePackages(options.board, new_package_atoms)
      if options.drop_file:
        fh = open(options.drop_file, 'w')
        fh.write(' '.join(revved_packages))
        fh.close()


if __name__ == '__main__':
  main(sys.argv)
