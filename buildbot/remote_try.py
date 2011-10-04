#!/usr/bin/python

# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import constants
import datetime
import getpass
import os
import sys

if __name__ == '__main__':
  sys.path.append(constants.SOURCE_ROOT)

from chromite.buildbot import repository
from chromite.buildbot import manifest_version
from chromite.lib import cros_build_lib as cros_lib

class RemoteTryJob(object):
  """Remote Tryjob that is submitted through a Git repo."""
  TRYJOB_REPO = '/tmp/tryjobs'
  SSH_URL = os.path.join(constants.GERRIT_SSH_URL,
                                  'chromiumos/tryjobs')

  def __init__(self, options, bots):
    values = {}
    self.user = values['user'] = getpass.getuser()
    values['email'] = '%s@google.com' % self.user
    # Name of the job that appears on the waterfall.
    values['name'] = ','.join(options.gerrit_patches)
    values['issue'] = ' '.join(options.gerrit_patches)
    self.bots = tuple(bots)
    values['bot'] = ','.join(bots)
    self.description = '\n'.join(
        "%s=%s" % (k, v) for (k, v) in values.iteritems())
    self.buildroot = options.buildroot
    self.tryjob_repo = os.path.join(self.buildroot, self.TRYJOB_REPO)

  def Submit(self):
    """Submit the tryjob through Git."""
    manifest_version._RemoveDirs(self.tryjob_repo)
    repository.CloneGitRepo(self.tryjob_repo, self.SSH_URL)
    manifest_version.PrepForChanges(self.tryjob_repo, False)

    current_time = str(datetime.datetime.utcnow()).replace(':', '.')
    current_time = current_time.replace(' ', '_')
    file_name = self.user + '.' + current_time
    fullpath = os.path.join(self.tryjob_repo, file_name)
    # Both commit description and contents of file contain tryjob specs.
    with open(fullpath, 'w+') as job_desc_file:
       job_desc_file.write(self.description)
       job_desc_file.flush()

    cros_lib.RunCommand(['git', 'add', file_name], cwd=self.tryjob_repo)
    cros_lib.RunCommand(['git', 'commit', '-am', self.description],
                        cwd=self.tryjob_repo)

    cros_lib.GitPushWithRetry(manifest_version.PUSH_BRANCH, self.tryjob_repo,
                              retries=10)
