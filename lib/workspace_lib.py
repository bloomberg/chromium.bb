# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilities for discovering the directories associated with workspaces.

Workspaces have a variety of important concepts:

* The bootstrap repository. BOOTSTRAP/chromite/bootstrap is expected to be in
the user's path. Most commands run from here redirect to the active SDK.

* The workspace directory. This directory (identified by presence of
WORKSPACE_CONFIG), contains code, and is associated with exactly one SDK
instance. It is normally discovered based on CWD.

* The SDK root. This directory contains a specific SDK version, and is stored in
BOOTSTRAP/sdk_checkouts/<version>.

This library contains helper methods for finding all of the relevant directories
here.
"""

from __future__ import print_function

import json
import os

from chromite.cbuildbot import constants
from chromite.lib import cros_build_lib
from chromite.lib import osutils

# The presence of this file signifies the root of a workspace.
WORKSPACE_CONFIG = 'workspace-config.json'
WORKSPACE_LOCAL_CONFIG = '.local.json'


def WorkspacePath(workspace_reference_dir=None):
  """Returns the path to the current workspace.

  This method works both inside and outside the chroot, though results will
  be different.

  Args:
    workspace_reference_dir: Any directory inside the workspace. If None,
      will use CWD (outside chroot), or bind mount location (inside chroot).
      You should normally use the default.

  Returns:
    Path to root directory of the workspace (if valid), or None.
  """
  if workspace_reference_dir is None:
    if cros_build_lib.IsInsideChroot():
      workspace_reference_dir = constants.CHROOT_WORKSPACE_ROOT
    else:
      workspace_reference_dir = os.getcwd()

  workspace_config = osutils.FindInPathParents(
      WORKSPACE_CONFIG,
      os.path.abspath(workspace_reference_dir))

  return os.path.dirname(workspace_config) if workspace_config else None


def GetActiveSdkVersion(workspace_root):
  """Find which SDK version a workspace is associated with.

  This SDK may or may not exist in the bootstrap cache. There may be no
  SDK version associated with a workspace.

  Args:
    workspace_root: Root directory of the workspace (WorkspacePath()).

  Returns:
    version string or None.
  """
  # Config should always return a dictionary.
  config = _ReadLocalConfig(workspace_root)

  # If version is present, use it, else return None.
  return config.get('version')


def SetActiveSdkVersion(workspace_root, version):
  """Set which SDK version a workspace is associated with.

  This method is NOT atomic.

  Args:
    workspace_root: Root directory of the workspace (WorkspacePath()).
    version: Version string of the SDK. (Eg. 1.2.3)
  """
  # Read the config, update its version, and write it.
  config = _ReadLocalConfig(workspace_root)
  config['version'] = version
  _WriteLocalConfig(workspace_root, config)


def _ReadLocalConfig(workspace_root):
  """Read a local config for a workspace.

  Args:
    workspace_root: Root directory of the workspace (WorkspacePath()).

  Returns:
    Local workspace config as a Python dictionary.
  """
  config_file = os.path.join(workspace_root, WORKSPACE_LOCAL_CONFIG)

  # If the file doesn't exist, it's an empty dictionary.
  if not os.path.exists(config_file):
    return {}

  with open(config_file, 'r') as config_fp:
    return json.load(config_fp)


def _WriteLocalConfig(workspace_root, config):
  """Save out a new local config for a workspace.

  Args:
    workspace_root: Root directory of the workspace (WorkspacePath()).
    config: New local workspace config contents as a Python dictionary.
  """
  config_file = os.path.join(workspace_root, WORKSPACE_LOCAL_CONFIG)

  # Overwrite the config file, with the new dictionary.
  with open(config_file, 'w') as config_fp:
    json.dump(config, config_fp)
