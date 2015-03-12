# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common utilities used by the chromium/bootstrap scripts."""

from __future__ import print_function

import os

from chromite.lib import workspace_lib


# This is the subdirectory of the bootstrap, where we store SDKs.
SDK_CHECKOUTS = 'sdk_checkouts'


def FindBootstrapPath():
  """Find the bootstrap directory.

  This isn't possible, if you aren't running from the bootstrap.

  Returns:
   Path to root of bootstrap, or None.
  """
  # TODO(dgarrett): Come up with a real mechanism to detect if we are running
  # from the bootstrap repository.
  return os.path.realpath(os.path.join(
      os.path.abspath(__file__), '..', '..'))


def ComputeSdkPath(bootstrap_dir, version):
  """What directory should an SDK be in.

  Args:
    bootstrap_dir: Bootstrap root directory.
    version: Version of the SDK.

  Returns:
    Path in which SDK version should be stored.
  """
  return os.path.join(bootstrap_dir, SDK_CHECKOUTS, version)


def GetActiveSdkPath(bootstrap_root, workspace_root):
  """Find the SDK Path associated with a given workspace.

  Most code should use constants.SOURCE_ROOT instead.

  Args:
    bootstrap_root: Path directory of the bootstrap dir (FindBootstrapPath()).
    workspace_root: Path directory of the workspace (FindWorkspacePath()).

  Returns:
    Path to root directory of SDK, if there is an active one, and it exists.
  """
  version = workspace_lib.GetActiveSdkVersion(workspace_root)
  if version is None:
    return None

  sdk_root = ComputeSdkPath(bootstrap_root, version)

  # Returns None if there is no active SDK version, or if it's not installed.
  return sdk_root if os.path.isdir(sdk_root) else None
