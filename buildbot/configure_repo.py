# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Adjust a repo checkout's configuration, fixing/extending as needed"""

import os
import constants

from chromite.lib import cros_build_lib


def FixExternalRepoPushUrls(buildroot):
  """Set up SSH push for cros remote."""

  shell_code = """
[ "${REPO_REMOTE}" = "cros" ] || exit 0;
git config "remote.${REPO_REMOTE}.pushurl" "%s/${REPO_PROJECT}";
""" % (constants.GERRIT_SSH_URL,)

  cros_build_lib.RunCommand(['repo', '--time', 'forall', '-c', shell_code],
                            cwd=buildroot)


def FixBrokenExistingRepos(buildroot):
  """Ensure all git configurations are at least syncable."""

  cros_build_lib.RunCommand(
      ['repo', '--time', 'forall', '-c',
      'git config --remove-section "url.%s" 2> /dev/null' %
      constants.GERRIT_SSH_URL], cwd=buildroot, error_ok=True)
