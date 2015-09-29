#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""git drover: A tool for merging changes to release branches."""

import argparse
import functools
import logging
import os
import shutil
import subprocess
import sys
import tempfile

import git_common


class Error(Exception):
  pass


class _Drover(object):

  def __init__(self, branch, revision, parent_repo, dry_run):
    self._branch = branch
    self._branch_ref = 'refs/remotes/branch-heads/%s' % branch
    self._revision = revision
    self._parent_repo = os.path.abspath(parent_repo)
    self._dry_run = dry_run
    self._workdir = None
    self._branch_name = None
    self._dev_null_file = open(os.devnull, 'w')

  def run(self):
    """Runs this Drover instance.

    Raises:
      Error: An error occurred while attempting to cherry-pick this change.
    """
    try:
      self._run_internal()
    finally:
      self._cleanup()

  def _run_internal(self):
    self._check_inputs()
    if not self._confirm('Going to cherry-pick\n"""\n%s"""\nto %s.' % (
        self._run_git_command(['show', '-s', self._revision]), self._branch)):
      return
    self._create_checkout()
    self._prepare_cherry_pick()
    if self._dry_run:
      logging.info('--dry_run enabled; not landing.')
      return

    self._run_git_command(['cl', 'upload'],
                          error_message='Upload failed',
                          interactive=True)

    if not self._confirm('About to land on %s.' % self._branch):
      return
    self._run_git_command(['cl', 'land', '--bypass-hooks'], interactive=True)

  def _cleanup(self):
    if self._branch_name:
      try:
        self._run_git_command(['cherry-pick', '--abort'])
      except Error:
        pass
      self._run_git_command(['checkout', '--detach'])
      self._run_git_command(['branch', '-D', self._branch_name])
    if self._workdir:
      logging.debug('Deleting %s', self._workdir)
      shutil.rmtree(self._workdir)
    self._dev_null_file.close()

  @staticmethod
  def _confirm(message):
    """Show a confirmation prompt with the given message.

    Returns:
      A bool representing whether the user wishes to continue.
    """
    result = ''
    while result not in ('y', 'n'):
      try:
        result = raw_input('%s Continue (y/n)? ' % message)
      except EOFError:
        result = 'n'
    return result == 'y'

  def _check_inputs(self):
    """Check the input arguments and ensure the parent repo is up to date."""

    if not os.path.isdir(self._parent_repo):
      raise Error('Invalid parent repo path %r' % self._parent_repo)
    if not hasattr(os, 'symlink'):
      raise Error('Symlink support is required')

    self._run_git_command(['--help'], error_message='Unable to run git')
    self._run_git_command(['status'],
                          error_message='%r is not a valid git repo' %
                          os.path.abspath(self._parent_repo))
    self._run_git_command(['fetch', 'origin'],
                          error_message='Failed to fetch origin')
    self._run_git_command(
        ['rev-parse', '%s^{commit}' % self._branch_ref],
        error_message='Branch %s not found' % self._branch_ref)
    self._run_git_command(
        ['rev-parse', '%s^{commit}' % self._revision],
        error_message='Revision "%s" not found' % self._revision)

  FILES_TO_LINK = [
      'refs',
      'logs/refs',
      'info/refs',
      'info/exclude',
      'objects',
      'hooks',
      'packed-refs',
      'remotes',
      'rr-cache',
      'svn',
  ]
  FILES_TO_COPY = ['config', 'HEAD']

  def _create_checkout(self):
    """Creates a checkout to use for cherry-picking.

    This creates a checkout similarly to git-new-workdir. Most of the .git
    directory is shared with the |self._parent_repo| using symlinks. This
    differs from git-new-workdir in that the config is forked instead of shared.
    This is so the new workdir can be a sparse checkout without affecting
    |self._parent_repo|.
    """
    parent_git_dir = os.path.abspath(self._run_git_command(
        ['rev-parse', '--git-dir']).strip())
    self._workdir = tempfile.mkdtemp(prefix='drover_%s_' % self._branch)
    logging.debug('Creating checkout in %s', self._workdir)
    git_dir = os.path.join(self._workdir, '.git')
    git_common.make_workdir_common(parent_git_dir, git_dir, self.FILES_TO_LINK,
                                   self.FILES_TO_COPY)
    self._run_git_command(['config', 'core.sparsecheckout', 'true'])
    with open(os.path.join(git_dir, 'info', 'sparse-checkout'), 'w') as f:
      f.write('codereview.settings')

    branch_name = os.path.split(self._workdir)[-1]
    self._run_git_command(['checkout', '-b', branch_name, self._branch_ref])
    self._branch_name = branch_name

  def _prepare_cherry_pick(self):
    self._run_git_command(['cherry-pick', '-x', self._revision],
                          error_message='Patch failed to apply')
    self._run_git_command(['reset', '--hard'])

  def _run_git_command(self, args, error_message=None, interactive=False):
    """Runs a git command.

    Args:
      args: A list of strings containing the args to pass to git.
      interactive:
      error_message: A string containing the error message to report if the
          command fails.

    Raises:
      Error: The command failed to complete successfully.
    """
    cwd = self._workdir if self._workdir else self._parent_repo
    logging.debug('Running git %s (cwd %r)', ' '.join('%s' % arg
                                                      for arg in args), cwd)

    run = subprocess.check_call if interactive else subprocess.check_output

    try:
      return run(['git'] + args,
                 shell=False,
                 cwd=cwd,
                 stderr=self._dev_null_file)
    except (OSError, subprocess.CalledProcessError) as e:
      if error_message:
        raise Error(error_message)
      else:
        raise Error('Command %r failed: %s' % (' '.join(args), e))


def cherry_pick_change(branch, revision, parent_repo, dry_run):
  """Cherry-picks a change into a branch.

  Args:
    branch: A string containing the release branch number to which to
        cherry-pick.
    revision: A string containing the revision to cherry-pick. It can be any
        string that git-rev-parse can identify as referring to a single
        revision.
    parent_repo: A string containing the path to the parent repo to use for this
        cherry-pick.
    dry_run: A boolean containing whether to stop before uploading the
        cherry-pick cl.

  Raises:
    Error: An error occurred while attempting to cherry-pick |cl| to |branch|.
  """
  drover = _Drover(branch, revision, parent_repo, dry_run)
  drover.run()


def main():
  parser = argparse.ArgumentParser(
      description='Cherry-pick a change into a release branch.')
  parser.add_argument(
      '--branch',
      type=str,
      required=True,
      metavar='<branch>',
      help='the name of the branch to which to cherry-pick; e.g. 1234')
  parser.add_argument('--cherry-pick',
                      type=str,
                      required=True,
                      metavar='<change>',
                      help=('the change to cherry-pick; this can be any string '
                            'that unambiguously refers to a revision'))
  parser.add_argument(
      '--parent_checkout',
      type=str,
      default=os.path.abspath('.'),
      metavar='<path_to_parent_checkout>',
      help=('the path to the chromium checkout to use as the source for a '
            'creating git-new-workdir workdir to use for cherry-picking; '
            'if unspecified, the current directory is used'))
  parser.add_argument(
      '--dry-run',
      action='store_true',
      default=False,
      help=("don't actually upload and land; "
            "just check that cherry-picking would succeed"))
  parser.add_argument('-v',
                      '--verbose',
                      action='store_true',
                      default=False,
                      help='show verbose logging')
  options = parser.parse_args()
  if options.verbose:
    logging.getLogger().setLevel(logging.DEBUG)
  try:
    cherry_pick_change(options.branch, options.cherry_pick,
                       options.parent_checkout, options.dry_run)
  except Error as e:
    logging.error(e.message)
    sys.exit(128)


if __name__ == '__main__':
  main()
