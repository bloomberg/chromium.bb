# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common utilities used by the chromium/bootstrap scripts."""

from __future__ import print_function

import os

from chromite.lib import cros_build_lib
from chromite.lib import project_sdk
from chromite.lib import workspace_lib


# This is the subdirectory of the bootstrap, where we store SDKs.
SDK_CHECKOUTS = 'sdk_checkouts'


# This env is used to remember the bootstrap path in child processes.
BOOTSTRAP_PATH_ENV = 'BRILLO_BOOTSTRAP_PATH'


def FindBootstrapPath(save_to_env=False):
  """Find the bootstrap directory.

  This is only possible, if the process was initially launched from a bootstrap
  environment, and isn't inside a chroot.

  Args:
    save_to_env: If true, preserve the bootstrap path in an ENV for child
                 processes. Only intended for the brillo bootstrap wrapper.

  Returns:
   Path to root of bootstrap, or None.
  """
  # We never have access to bootstrap if we are inside the chroot.
  if cros_build_lib.IsInsideChroot():
    return None

  # See if the path has already been determined, especially in a parent wrapper
  # process.
  env_path = os.environ.get(BOOTSTRAP_PATH_ENV)
  if env_path:
    return env_path

  # Base the bootstrap location on our current location, and remember it.
  new_path = os.path.realpath(os.path.join(
      os.path.abspath(__file__), '..', '..'))

  # No repo checkout is a valid bootstrap environment, because the bootstrap
  # environment holds repo checkouts inside SDK_CHECKOUTS, and repos cannot
  # exist inside other repos.
  if project_sdk.FindRepoRoot(new_path):
    return None

  if save_to_env:
    os.environ[BOOTSTRAP_PATH_ENV] = new_path

  return new_path


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
