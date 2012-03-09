#!/usr/bin/python

# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module uprevs a given package's ebuild to the next revision."""

import optparse
import os
import subprocess
import sys

import constants
if __name__ == '__main__':
  sys.path.insert(0, constants.SOURCE_ROOT)

from chromite.buildbot import portage_utilities, repository
from chromite.lib import cros_build_lib


# TODO(sosa): Remove during OO refactor.
VERBOSE = False

# Dictionary of valid commands with usage information.
COMMAND_DICTIONARY = {
                        'commit':
                          'Marks given ebuilds as stable locally',
                        'push':
                          'Pushes previous marking of ebuilds to remote repo',
                      }


# ======================= Global Helper Functions ========================


def _Print(message):
  """Verbose print function."""
  if VERBOSE:
    cros_build_lib.Info(message)


def CleanStalePackages(boards, package_atoms):
  """Cleans up stale package info from a previous build.
  Args:
    boards: Boards to clean the packages from.
    package_atoms: The actual package atom to unmerge.
  """
  if package_atoms:
    cros_build_lib.Info('Cleaning up stale packages %s.' % package_atoms)
    for board in boards:
      unmerge_board_cmd = ['emerge-%s' % board, '--unmerge']
      unmerge_board_cmd.extend(package_atoms)
      cros_build_lib.RunCommand(unmerge_board_cmd)

    unmerge_host_cmd = ['emerge', '--unmerge']
    unmerge_host_cmd.extend(package_atoms)
    cros_build_lib.SudoRunCommand(unmerge_host_cmd)

  for board in boards:
    cros_build_lib.RunCommand(['eclean-%s' % board, '-d', 'packages'],
                              redirect_stderr=True)
  cros_build_lib.SudoRunCommand(['eclean', '-d', 'packages'],
                            redirect_stderr=True)


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
  if not options.boards and command == 'commit':
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


def PushChange(stable_branch, tracking_branch, dryrun):
  """Pushes commits in the stable_branch to the remote git repository.

  Pushes local commits from calls to CommitChange to the remote git
  repository specified by current working directory.

  Args:
    stable_branch: The local branch with commits we want to push.
    tracking_branch: The tracking branch of the local branch.
    dryrun: Use git push --dryrun to emulate a push.
  Raises:
      OSError: Error occurred while pushing.
  """
  # For the commit queue, our local branch may contain commits that were just
  # tested and pushed during the CommitQueueCompletion stage. After syncing,
  # our local branch should be rebased so that these changes are no longer
  # present.
  repository.RepoSyncUsingSSH('.')

  if not _DoWeHaveLocalCommits(stable_branch, tracking_branch):
    cros_build_lib.Info('No work found to push.  Exiting')
    return

  description = _SimpleRunCommand('git log --format=format:%s%n%n%b ' +
                                  tracking_branch + '..')
  description = 'Marking set of ebuilds as stable\n\n%s' % description
  cros_build_lib.Info('Using description %s' % description)
  merge_branch = GitBranch(constants.MERGE_BRANCH, tracking_branch)
  if merge_branch.Exists():
    merge_branch.Delete()
  merge_branch.CreateBranch()
  if not merge_branch.Exists():
    cros_build_lib.Die('Unable to create merge branch.')
  _SimpleRunCommand('git merge --squash %s' % stable_branch)
  cros_build_lib.RunCommand(['git', 'commit', '-m', description])
  _SimpleRunCommand('git config push.default tracking')
  cros_build_lib.GitPushWithRetry(constants.MERGE_BRANCH, cwd='.',
                                  dryrun=dryrun)


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
      git_cmd = 'repo start %s .' % target.branch_name
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
    delete_cmd = 'repo abandon %s' % self.branch_name
    _SimpleRunCommand(delete_cmd)


def main():
  parser = optparse.OptionParser('cros_mark_as_stable OPTIONS packages')
  parser.add_option('--all', action='store_true',
                    help='Mark all packages as stable.')
  parser.add_option('-b', '--boards',
                    help='Colon-separated list of boards')
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
  portage_utilities.EBuild.VERBOSE = options.verbose

  if len(args) != 1:
    _PrintUsageAndDie('Must specify a valid command [commit, push]')

  command = args[0]
  package_list = None
  if options.packages:
    package_list = options.packages.split(':')

  _CheckSaneArguments(package_list, command, options)
  if options.overlays:
    overlays = {}
    for path in options.overlays.split(':'):
      if not os.path.isdir(path):
        cros_build_lib.Die('Cannot find overlay: %s' % path)
      overlays[path] = []
  else:
    cros_build_lib.Warning('Missing --overlays argument')
    overlays = {
      '%s/private-overlays/chromeos-overlay' % options.srcroot: [],
      '%s/third_party/chromiumos-overlay' % options.srcroot: []
    }

  if command == 'commit':
    portage_utilities.BuildEBuildDictionary(
      overlays, options.all, package_list)

  tracking_branch = cros_build_lib.GetManifestDefaultBranch(options.srcroot)
  tracking_branch = 'remotes/m/' + tracking_branch

  # Contains the array of packages we actually revved.
  revved_packages = []
  new_package_atoms = []

  for overlay, ebuilds in overlays.items():
    if not os.path.isdir(overlay):
      cros_build_lib.Warning("Skipping %s" % overlay)
      continue

    # TODO(davidjames): Currently, all code that interacts with git depends on
    # the cwd being set to the overlay directory. We should instead pass in
    # this parameter so that we don't need to modify the cwd globally.
    os.chdir(overlay)

    if command == 'push':
      PushChange(constants.STABLE_EBUILD_BRANCH, tracking_branch,
                 options.dryrun)
    elif command == 'commit' and ebuilds:
      existing_branch = cros_build_lib.GetCurrentBranch('.')
      work_branch = GitBranch(constants.STABLE_EBUILD_BRANCH, tracking_branch)
      work_branch.CreateBranch()
      if not work_branch.Exists():
        cros_build_lib.Die('Unable to create stabilizing branch in %s' %
                           overlay)

      # In the case of uprevving overlays that have patches applied to them,
      # include the patched changes in the stabilizing branch.
      if existing_branch:
        _SimpleRunCommand('git rebase %s' % existing_branch)

      for ebuild in ebuilds:
        try:
          _Print('Working on %s' % ebuild.package)
          new_package = ebuild.RevWorkOnEBuild(options.srcroot)
          if new_package:
            revved_packages.append(ebuild.package)
            new_package_atoms.append('=%s' % new_package)
        except (OSError, IOError):
          cros_build_lib.Warning('Cannot rev %s\n' % ebuild.package +
                  'Note you will have to go into %s '
                  'and reset the git repo yourself.' % overlay)
          raise

  if command == 'commit':
    CleanStalePackages(options.boards.split(':'), new_package_atoms)
    if options.drop_file:
      fh = open(options.drop_file, 'w')
      fh.write(' '.join(revved_packages))
      fh.close()


if __name__ == '__main__':
  main()
