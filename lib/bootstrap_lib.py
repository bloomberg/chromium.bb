# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common utilities used by the chromium/bootstrap scripts."""

from __future__ import print_function

import os

from chromite.lib import workspace_lib


# This is the subdirectory of the bootstrap, where we store SDKs.
SDK_CHECKOUTS = 'sdk_checkouts'


# This env is used to remember the bootstrap path in child processes.
BOOTSTRAP_PATH_ENV = 'BRILLO_BOOTSTRAP_PATH'


def FindBootstrapPath(_save_to_env=False):
  """Find the bootstrap directory.

  This is only possible, if the process was initially launched from a bootstrap
  environment, and isn't inside a chroot.

  Args:
    save_to_env: If true, preserve the bootstrap path in an ENV for child
                 processes. Only intended for the brillo bootstrap wrapper.

  Returns:
   Path to root of bootstrap, or None.
  """
  return None


def ComputeSdkPath(bootstrap_path, version):
  """What directory should an SDK be in.

  Args:
    bootstrap_path: Bootstrap root directory, or None.
    version: Version of the SDK.

  Returns:
    Path in which SDK version should be stored, or None.
  """
  if bootstrap_path is None:
    return None

  return os.path.join(bootstrap_path, SDK_CHECKOUTS, version)


def GetActiveSdkPath(bootstrap_path, workspace_path):
  """Find the SDK Path associated with a given workspace.

  Most code should use constants.SOURCE_ROOT instead.

  Args:
    bootstrap_path: Path directory of the bootstrap dir (FindBootstrapPath()).
    workspace_path: Path directory of the workspace (FindWorkspacePath()).

  Returns:
    Path to root directory of SDK, if there is an active one, and it exists.
  """
  if bootstrap_path is None:
    return None

  version = workspace_lib.GetActiveSdkVersion(workspace_path)
  if version is None:
    return None

  sdk_root = ComputeSdkPath(bootstrap_path, version)

  # Returns None if there is no active SDK version, or if it's not installed.
  return sdk_root if os.path.isdir(sdk_root) else None
