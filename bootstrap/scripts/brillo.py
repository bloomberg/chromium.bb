# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Bootstrap wrapper for 'brillo' command.

For most commands of the form "brillo XYZ", we reinvoke
REPO_DIR/chromite/bin/brillo XYZ, after detecting REPO_DIR based on the CWD.

For the "brillo sdk" command, we reinvoke "../bin/brillo sdk" from the current
git repository. This allows the SDK command to be run, even if there is no repo
checkout.
"""

from __future__ import print_function

import os

from chromite.lib import bootstrap_lib
from chromite.lib import cros_build_lib
from chromite.lib import git
from chromite.lib import workspace_lib


def LocateBrilloCommand(args):
  bootstrap_path = bootstrap_lib.FindBootstrapPath(save_to_env=True)

  if len(args) >= 1 and args[0] == 'sdk':
    if not bootstrap_path:
      cros_build_lib.Die(
          'You are bootstrapping chromite from a repo checkout.\n'
          'You must use a git clone. (brbug.com/580: link docs)')

    # Run 'brillo sdk' from the repository containing this command.
    return os.path.join(bootstrap_path, 'bin', 'brillo')

  # If we are in a workspace, and the workspace has an associated SDK, use it.
  workspace_path = workspace_lib.WorkspacePath()
  if workspace_path:
    sdk_path = bootstrap_lib.GetActiveSdkPath(bootstrap_path, workspace_path)
    if not sdk_path:
      cros_build_lib.Die(
          'The current workspace has no valid SDK.\n'
          'Please run "brillo sdk --update" (brbug.com/580: link docs)')

    # Use SDK associated with workspace, or nothing.
    return os.path.join(sdk_path, 'chromite', 'bin', 'brillo')

  # Run all other commands from 'brillo' wrapper in repo detected via CWD.
  repo_path = git.FindRepoCheckoutRoot(os.getcwd())
  if repo_path:
    return os.path.join(repo_path, 'chromite', 'bin', 'brillo')

  # Couldn't find the real brillo command to run.
  cros_build_lib.Die('Unable to detect which SDK you want to use.')

def main(args):
  bin_cmd = LocateBrilloCommand(args)
  os.execv(bin_cmd, [bin_cmd] + args)
