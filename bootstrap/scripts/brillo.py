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
from chromite.lib import git
from chromite.lib import workspace_lib


def LocateBrilloCommand(args):
  relative_cmd = os.path.join('chromite', 'bin', 'brillo')
  bootstrap_path = bootstrap_lib.FindBootstrapPath()

  if len(args) >= 1 and args[0] == 'sdk':
    # Run 'brillo sdk' from the repository containing this command.
    return os.path.join(bootstrap_path, relative_cmd)

  # If we are in a workspace, and the workspace has an associated SDK, use it.
  workspace_path = workspace_lib.WorkspacePath()
  if workspace_path:
    sdk_path = bootstrap_lib.GetActiveSdkPath(bootstrap_path, workspace_path)
    # Use SDK associated with workspace, or nothing.
    return os.path.join(sdk_path, relative_cmd) if sdk_path else None

  # Run all other commands from 'brillo' wrapper in repo detected via CWD.
  repo_root = git.FindRepoCheckoutRoot(os.getcwd())
  if repo_root:
    return os.path.join(repo_root, relative_cmd)

  # Couldn't find the real brillo command to run.
  return None

def main(args):
  bin_cmd = LocateBrilloCommand(args)

  if bin_cmd is None:
    print('Unable to detect which SDK you want to use.')
    return 1

  os.execv(bin_cmd, [bin_cmd] + args)
