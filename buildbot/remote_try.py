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
from chromite.lib import cros_build_lib as cros_lib

class RemoteTryJob(object):
  """Remote Tryjob that is submitted through a Git repo."""
  SSH_URL = os.path.join(constants.GERRIT_SSH_URL,
                         'chromiumos/tryjobs')
  TRYJOB_DESCRIPTION_VERSION = 1

  def __init__(self, options, bots, local_patches):
    """Construct the object.

    Args:
      options: The parsed options passed into cbuildbot.
      bots: A list of configs to run tryjobs for.
      local_patches: A list of LocalGitRepoPatch objects.
    """
    self.options = options
    self.user = getpass.getuser()
    cwd = os.path.dirname(os.path.realpath(__file__))
    self.user_email = cros_lib.GetProjectUserEmail(cwd)
    # Name of the job that appears on the waterfall.
    patch_list = options.gerrit_patches + options.local_patches
    self.name = ','.join(patch_list)
    self.bots = bots[:]
    self.description = ('name: %s\n patches: %s\nbots: %s' %
                        (self.name, patch_list, self.bots))
    self.buildroot = options.buildroot
    self.extra_args = []
    if options.gerrit_patches:
      self.extra_args.append('--gerrit-patches=%s'
                             % ' '.join(options.gerrit_patches))
    self.tryjob_repo = None
    self.local_patches = local_patches

  @property
  def values(self):
    return {
        'user' : self.user,
        'email' : [self.user_email],
        'name' : self.name,
        'bot' : self.bots,
        'extra_args' : self.extra_args,
        'version' : self.TRYJOB_DESCRIPTION_VERSION,}

  def _Submit(self, dryrun):
    """Internal submission function.  See Submit() for arg description."""
    current_time = str(int(time.time()))
    ref_base = os.path.join('refs/tryjobs', self.user, current_time)
    for patch in self.local_patches:
      sha1 = patch.Sha1Hash()[:8]
      local_branch = os.path.basename(patch.ref)
      ref_final = os.path.join(ref_base, local_branch, sha1)
      patch.Upload(ref_final, dryrun=dryrun)
      self.extra_args.append('--remote-patches=%s:%s:%s:%s'
                             % (patch.project, local_branch, ref_final,
                                patch.tracking_branch))

    # TODO(rcui): convert to shallow clone when that's available.
    repository.CloneGitRepo(self.tryjob_repo, self.SSH_URL)
    push_branch = manifest_version.PUSH_BRANCH
    cros_lib.CreatePushBranch(push_branch, self.tryjob_repo, sync=False)

    file_name = '%s.%s' % (self.user,
                           current_time)
    user_dir = os.path.join(self.tryjob_repo, self.user)
    if not os.path.isdir(user_dir):
      os.mkdir(user_dir)

    fullpath = os.path.join(user_dir, file_name)
    with open(fullpath, 'w+') as job_desc_file:
      json.dump(self.values, job_desc_file)

    cros_lib.RunCommand(['git', 'add', fullpath], cwd=self.tryjob_repo)
    extra_env = {
      # The committer field makes sure the creds match what the remote
      # gerrit instance expects while the author field allows lookup
      # on the console to work.  http://crosbug.com/27939
      'GIT_COMMITTER_EMAIL' : self.user_email,
      'GIT_AUTHOR_EMAIL'    : self.user_email,
    }
    cros_lib.RunCommand(['git', 'commit', '-m', self.description],
                        cwd=self.tryjob_repo, extra_env=extra_env)

    try:
      cros_lib.GitPushWithRetry(push_branch, self.tryjob_repo, dryrun=dryrun)
    except cros_lib.GitPushFailed:
      cros_lib.Error('Failed to submit tryjob.  This could be due to too many '
                     'submission requests by users.  Please try again.')
      raise

  def Submit(self, workdir=None, dryrun=False):
    """Submit the tryjob through Git.

    Args:
      workdir: The directory to clone tryjob repo into.  If you pass this
               in, you are responsible for deleting the directory.  Used for
               testing.
      dryrun: Setting to true will run everything except the final submit step.
    """
    self.tryjob_repo = workdir
    if self.tryjob_repo is None:
      self.tryjob_repo = tempfile.mkdtemp()

    try:
      self._Submit(dryrun)
    finally:
      if workdir is None:
        shutil.rmtree(self.tryjob_repo)

  def GetTrybotConsoleLink(self):
    """Get link to the console for the user."""
    return ('http://chromegw/p/tryserver.chromiumos/console?name=%s'
            % self.user_email)
