# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Adjust a repo checkout's configuration, fixing/extending as needed"""

import os
import constants
if __name__ == '__main__':
  import sys
  sys.path.append(constants.SOURCE_ROOT)

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


if __name__ == '__main__':
  if len(sys.argv) == 2:
    path = os.path.abspath(sys.argv[1])
    FixBrokenExistingRepos(path)
    FixExternalRepoPushUrls(path)
    sys.exit(0)

  elif len(sys.argv) == 1:
    print "No arguments given: I need the pathway to the repo root."

  else:
    print "Wrong arguments given: I expect just the path to the repo root" \
          " however was given: %s" % ' '.join(map(repr, sys.argv[1:]))
  sys.exit(1)
