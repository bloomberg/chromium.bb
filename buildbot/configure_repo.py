# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Adjust a repo checkout's configuration, fixing/extending as needed"""

import constants
import os

from chromite.lib import cros_build_lib
from chromite.lib import osutils

def FixExternalRepoPushUrls(buildroot):
  """Set up SSH push for cros remote."""

  shell_code = """
[ "${REPO_REMOTE}" = "cros" ] || exit 0;
git config "remote.${REPO_REMOTE}.pushurl" "%s/${REPO_PROJECT}";
""" % (constants.GERRIT_SSH_URL,)

  cros_build_lib.RunCommand(['repo', '--time', 'forall', '-c', shell_code],
                            cwd=buildroot)


def FixBrokenExistingRepos(buildroot):
  """Ensure all git configurations are at least syncable and sane."""

  # Wipe pre-auto-gc hook; see chromium-os:31271
  cros_build_lib.RunCommand(
      ['repo', '--time', 'forall', '-c',
      'rm -f .git/hooks/pre-auto-gc; '
      'git config --remove-section "url.%s" 2> /dev/null' %
      constants.GERRIT_SSH_URL], cwd=buildroot, error_ok=True)

  osutils.SafeUnlink(os.path.join(buildroot, 'manifests.git/hooks/pre-auto-gc'))


def SetupGerritRemote(buildroot):
  """Set up gerrit remote with SSH push.

  This is used by buildbots. If a pushurl is present on the cros remote is
  present, it will be removed, for ensuring that all consumers in the buildbot
  have moved to use the cros remote.
  """

  urls = dict(gerrit_url=constants.GERRIT_SSH_URL,
              gerrit_int_url=constants.GERRIT_INT_SSH_URL)
  shell_code = """
if ! git config remote.gerrit.url > /dev/null; then
  if [ "${REPO_REMOTE}" = "cros" ]; then
    git config --unset-all "remote.${REPO_REMOTE}.pushurl" 2> /dev/null;
    git remote add gerrit "%(gerrit_url)s/${REPO_PROJECT}" || exit 1
  else
    git remote add gerrit "%(gerrit_int_url)s/${REPO_PROJECT}" || exit 1
  fi
fi
""" % urls

  cros_build_lib.RunCommand(['repo', '--time', 'forall', '-c', shell_code],
                            cwd=buildroot)
