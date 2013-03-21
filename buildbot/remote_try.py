#!/usr/bin/python

# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import constants
import getpass
import json
import os
import shutil
import sys
import tempfile
import time

if __name__ == '__main__':
  sys.path.insert(0, constants.SOURCE_ROOT)

from chromite.buildbot import repository
from chromite.buildbot import manifest_version
from chromite.lib import cros_build_lib
from chromite.lib import git


class ChromiteUpgradeNeeded(Exception):
  """Exception thrown when it's detected that we need to upgrade chromite."""

  def __init__(self, version=None):
    Exception.__init__(self)
    self.version = version
    self.args = (version,)

  def __str__(self):
    version_str = ''
    if self.version:
      version_str = "  Need format version %r support." % (self.version,)
    return (
        "Your version of cbuildbot is too old; please resync it, "
        "and then retry your submission.%s" % (version_str,))


class RemoteTryJob(object):
  """Remote Tryjob that is submitted through a Git repo."""
  EXT_SSH_URL = os.path.join(constants.GERRIT_SSH_URL,
                             'chromiumos/tryjobs')
  INT_SSH_URL = os.path.join(constants.GERRIT_INT_SSH_URL,
                             'chromeos/tryjobs')

  # In version 3, remote patches have an extra field.
  # In version 4, cherry-picking is the norm, thus multiple patches are
  # generated.
  TRYJOB_FORMAT_VERSION = 4
  TRYSERVER_URL = 'https://uberchromegw.corp.google.com/i/chromiumos.tryserver'
  TRYJOB_FORMAT_FILE = '.tryjob_minimal_format_version'

  def __init__(self, options, bots, local_patches):
    """Construct the object.

    Args:
      options: The parsed options passed into cbuildbot.
      bots: A list of configs to run tryjobs for.
      local_patches: A list of LocalPatch objects.
    """
    self.options = options
    self.user = getpass.getuser()
    cwd = os.path.dirname(os.path.realpath(__file__))
    self.user_email = git.GetProjectUserEmail(cwd)
    cros_build_lib.Info('Using email:%s', self.user_email)
    # Name of the job that appears on the waterfall.
    patch_list = options.gerrit_patches + options.local_patches
    self.name = options.remote_description
    if self.name is None:
      self.name = ''
      if options.branch != 'master':
        self.name = '[%s] ' % options.branch
      self.name += ','.join(patch_list)
    self.bots = bots[:]
    self.slaves_request = options.slaves
    self.description = ('name: %s\n patches: %s\nbots: %s' %
                        (self.name, patch_list, self.bots))
    self.extra_args = options.pass_through_args
    if '--buildbot' not in self.extra_args:
      self.extra_args.append('--remote-trybot')

    self.extra_args.append('--remote-version=%s'
                           % (self.TRYJOB_FORMAT_VERSION,))
    self.tryjob_repo = None
    self.local_patches = local_patches
    self.ssh_url = self.EXT_SSH_URL
    self.manifest = None
    if repository.IsARepoRoot(options.sourceroot):
      self.manifest = git.ManifestCheckout.Cached(options.sourceroot)
      if repository.IsInternalRepoCheckout(options.sourceroot):
        self.ssh_url = self.INT_SSH_URL

  @property
  def values(self):
    return {
        'bot' : self.bots,
        'email' : [self.user_email],
        'extra_args' : self.extra_args,
        'name' : self.name,
        'slaves_request' : self.slaves_request,
        'user' : self.user,
        'version' : self.TRYJOB_FORMAT_VERSION,
        }

  def _Submit(self, testjob, dryrun):
    """Internal submission function.  See Submit() for arg description."""
    # TODO(rcui): convert to shallow clone when that's available.
    current_time = str(int(time.time()))
    repository.CloneGitRepo(self.tryjob_repo, self.ssh_url)
    version_path = os.path.join(self.tryjob_repo,
                                self.TRYJOB_FORMAT_FILE)
    with open(version_path, 'r') as f:
      try:
        val = int(f.read().strip())
      except ValueError:
        raise ChromiteUpgradeNeeded()
      if val > self.TRYJOB_FORMAT_VERSION:
        raise ChromiteUpgradeNeeded(val)

    ref_base = os.path.join('refs/tryjobs', self.user, current_time)
    for patch in self.local_patches:
      # Isolate the name; if it's a tag or a remote, let through.
      # Else if it's a branch, get the full branch name minus refs/heads.
      local_branch = git.StripRefsHeads(patch.ref, False)
      ref_final = os.path.join(ref_base, local_branch, patch.sha1)

      self.manifest.AssertProjectIsPushable(patch.project)
      data = self.manifest.projects[patch.project]
      patch.Upload(data['push_url'], ref_final, dryrun=dryrun)

      # TODO(rcui): Pass in the remote instead of tag. http://crosbug.com/33937.
      tag = constants.EXTERNAL_PATCH_TAG
      if data['remote'] == constants.INTERNAL_REMOTE:
        tag = constants.INTERNAL_PATCH_TAG

      self.extra_args.append('--remote-patches=%s:%s:%s:%s:%s'
                             % (patch.project, local_branch, ref_final,
                                patch.tracking_branch, tag))

    push_branch = manifest_version.PUSH_BRANCH
    remote_branch = ('origin', 'refs/remotes/origin/test') if testjob else None
    git.CreatePushBranch(push_branch, self.tryjob_repo, sync=False,
                         remote_push_branch=remote_branch)

    file_name = '%s.%s' % (self.user,
                           current_time)
    user_dir = os.path.join(self.tryjob_repo, self.user)
    if not os.path.isdir(user_dir):
      os.mkdir(user_dir)

    fullpath = os.path.join(user_dir, file_name)
    with open(fullpath, 'w+') as job_desc_file:
      json.dump(self.values, job_desc_file)

    cros_build_lib.RunCommand(['git', 'add', fullpath], cwd=self.tryjob_repo)
    extra_env = {
      # The committer field makes sure the creds match what the remote
      # gerrit instance expects while the author field allows lookup
      # on the console to work.  http://crosbug.com/27939
      'GIT_COMMITTER_EMAIL' : self.user_email,
      'GIT_AUTHOR_EMAIL'    : self.user_email,
    }
    cros_build_lib.RunCommand(['git', 'commit', '-m', self.description],
                              cwd=self.tryjob_repo, extra_env=extra_env)

    try:
      git.PushWithRetry(
          push_branch, self.tryjob_repo, retries=3, dryrun=dryrun)
    except cros_build_lib.RunCommandError:
      cros_build_lib.Error(
          'Failed to submit tryjob.  This could be due to too many '
          'submission requests by users.  Please try again.')
      raise

  def Submit(self, workdir=None, testjob=False, dryrun=False):
    """Submit the tryjob through Git.

    Args:
      workdir: The directory to clone tryjob repo into.  If you pass this
               in, you are responsible for deleting the directory.  Used for
               testing.
      testjob: Submit job to the test branch of the tryjob repo.  The tryjob
               will be ignored by production master.
      dryrun: Setting to true will run everything except the final submit step.
    """
    self.tryjob_repo = workdir
    if self.tryjob_repo is None:
      self.tryjob_repo = tempfile.mkdtemp()

    try:
      self._Submit(testjob, dryrun)
    finally:
      if workdir is None:
        shutil.rmtree(self.tryjob_repo)

  def GetTrybotConsoleLink(self):
    """Get link to the console for the user."""
    return ('%s/console?name=%s' % (self.TRYSERVER_URL, self.user_email))

  def GetTrybotWaterfallLink(self):
    """Get link to the waterfall for the user."""
    # Note that this will only show the jobs submitted by the user in the last
    # 24 hours.
    return ('%s/waterfall?committer=%s' % (self.TRYSERVER_URL, self.user_email))
