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

from chromite.lib import git


def main(args):
  if len(args) >= 1 and args[0] == 'sdk':
    # Run 'brillo sdk' from the repository containing this command.
    bootstrap_dir = os.path.dirname(os.path.abspath(__file__))
    bin_cmd = os.path.join(bootstrap_dir, '..', '..', 'bin', 'brillo')
  else:
    # Run all other commands from 'brillo' wrapper in repo detected via CWD.
    repo_root = git.FindRepoCheckoutRoot(os.getcwd())
    if repo_root is None:
      print('Unable to detect which SDK you want to use.')
      return 1
    bin_cmd = os.path.join(repo_root, 'chromite', 'bin', 'brillo')

  os.execv(bin_cmd, [bin_cmd] + args)
