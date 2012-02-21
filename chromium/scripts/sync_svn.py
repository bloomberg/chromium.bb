#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A script for syncing a Git repository to a Subversion repository.

See help text below for usage.
"""

import filecmp
import os
import subprocess
import sys


def SyncPath(git_base, svn_base, diff):
  """Synchronizes the Git repository to the SVN repository using the diff.

  Given two paths and a filecmp.dircmp object of the diff, mirrors the contents
  of |git_base| to |svn_base|.  WARNING: This includes deleting all files and
  directories in |svn_base| that are not in |git_base|.

  The difference between this and 'rsync -a --delete' is this method will run
  the appropriate 'svn add' and 'svn rm' commands necessary to commit.

  Args:
    git_base: Source folder, usually a Git repository.
    svn_base: Destination folder to sync to, must be an SVN repository.
    diff: A filecmp.dircmp diff object of git_base and svn_base.
  """
  # Delete files and folders only in the SVN repository.
  for f in diff.right_only:
    f = os.path.abspath(os.path.join(svn_base, f))
    print 'Removing %s' % f
    subprocess.check_call(['svn', 'rm', '--force', '--quiet', f])
    if os.path.isdir(f):
      subprocess.check_call(['rm', '-rf', f])

  # Copy files which were only in Git or different in SVN.
  for f in diff.left_only + diff.diff_files:
    dest = os.path.join(svn_base, f)
    src = os.path.abspath(os.path.join(git_base, f))
    print 'Copying %s' % src
    subprocess.check_call(['cp', '-fpr', src, dest])
    subprocess.check_call(['svn', 'add', '--force', '--quiet', dest])

  # For each directory in both, run SyncPath again on the diff.
  for dir_base, dir_diff in diff.subdirs.iteritems():
    SyncPath(os.path.join(git_base, dir_base),
             os.path.join(svn_base, dir_base), dir_diff)


def Main():
  print '\n'.join([
      '\n    Usage: %s <source git path> <dest svn path>\n' % sys.argv[0],
      '%s syncs a Git repository to a Subversion one.  Given a source' % (
          os.path.basename(sys.argv[0])),
      'Git repository and a destination SVN repository, it does:\n',
      '    1. cd <source git>; git clean -qfdx',
      '    2. Recursively removes all files and folders from SVN which are',
      '       in SVN but not in Git.',
      '    3. Recursively copies all files and folders from Git to SVN which',
      '       are in Git but not in SVN.',
      '    4. Recursively copies all files and folders from Git to SVN which',
      '       are different in Git than SVN.\n',
      '!!WARNING!!: This is a destructive process, both the source Git and ',
      'destination SVN repositories will be modified.  The git clean -qfdx',
      'command will remove all files and folders not under source control from',
      'the Git repository.  While the sync process will remove all files',
      'and folders from the SVN repository not in the Git repository.\n'])

  if len(sys.argv) != 3:
    sys.exit(1)

  git_path = os.path.abspath(sys.argv[1])
  svn_path = os.path.abspath(sys.argv[2])
  if not os.path.isdir(git_path):
    print 'ERROR: The specified Git path is not a directory.'
    sys.exit(1)
  if not os.path.isdir(svn_path):
    print 'ERROR: The specified Subversion path is not a directory.'
    sys.exit(1)

  prompt = raw_input('\n'.join([
      'You are about to sync the Git repository:\n'
      '    %s\n' % git_path,
      'To the Subversion repository:\n'
      '    %s\n' % svn_path,
      'This is destructive, are you sure you want to continue? [y/N]: ']))

  if not prompt.strip().upper() in ['Y', 'YES']:
    print 'Aborting...'
    sys.exit(1)

  # Cleanup the Git repository before syncing.
  subprocess.check_call(['git', 'clean', '-qfdx'], cwd=git_path)

  SyncPath(git_path, svn_path, filecmp.dircmp(
      git_path, svn_path, ignore=['.git', '.svn']))
  print 'Sync complete, you may now run "svn commit" from the SVN directory.'


if __name__ == '__main__':
  Main()
