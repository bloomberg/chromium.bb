# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Manage Project SDK installation, NOT REALATED to 'cros_sdk'."""

from __future__ import print_function

import os

from chromite.cbuildbot import constants
from chromite.cbuildbot import repository
from chromite.cli import command
from chromite.lib import bootstrap_lib
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import git
from chromite.lib import osutils
from chromite.lib import project_sdk
from chromite.lib import workspace_lib


_BRILLO_SDK_NO_UPDATE = 'BRILLO_SDK_NO_UPDATE'


def _UpdateWorkspaceSdk(bootstrap_path, workspace_path, version):
  """Install specified SDK, and associate the workspace with it."""
  if project_sdk.FindRepoRoot(bootstrap_path):
    cros_build_lib.Die('brillo sdk must run from a git clone. brbug.com/580')

  sdk_path = bootstrap_lib.ComputeSdkPath(bootstrap_path, version)

  # If this version already exists, no need to reinstall it.
  if not project_sdk.FindRepoRoot(sdk_path):
    _UpdateSdk(sdk_path, version)

  # Store the new version in the workspace.
  workspace_lib.SetActiveSdkVersion(workspace_path, version)


def _UpdateSdk(sdk_dir, version):
  """Install the specified SDK at the specified location.

  Args:
    sdk_dir: Directory in which to create a repo.
    version: Project SDK version to sync.
  """
  # Create the SDK dir, if it doesn't already exist.
  osutils.SafeMakedirs(sdk_dir)

  # Figure out what manifest to sync into repo.
  if version.lower() == 'tot':
    manifest_url = constants.MANIFEST_URL
    manifest_path = constants.PROJECT_MANIFEST
    depth = None
  else:
    manifest_url = constants.MANIFEST_VERSIONS_GOB_URL
    manifest_path = 'project-sdk/%s.xml' % version
    depth = 1

  # Init new repo.
  repo = repository.RepoRepository(
      manifest_url, sdk_dir, manifest=manifest_path, depth=depth)

  # Sync it.
  repo.Sync()


def _HandleUpdate(bootstrap_path, workspace_path, sdk_dir, version):
  """Handle an update request.

  Args:
    bootstrap_path: Path to bootstrap location (or None).
    workspace_path: Path to workspace location (or None).
    sdk_dir: Directory in which to create a repo (or None).
    version: Project SDK version to sync.
  """
  if sdk_dir:
    # Install the SDK to an explicit location.
    _UpdateSdk(sdk_dir, version)

  if workspace_path:
    # If we are in a workspace, update the SDK for that workspace.
    _UpdateWorkspaceSdk(bootstrap_path, workspace_path, version)


def _FindVersion(workspace_path, sdk_dir):
  """Find SDK version of an existing specific sdk_dir.

  Args:
    workspace_path: Path to workspace location (or None).
    sdk_dir: Directory in which to create a repo (or None).
  """
  if sdk_dir:
    return project_sdk.FindVersion(sdk_dir)

  if workspace_path:
    return workspace_lib.GetActiveSdkVersion(workspace_path)

  return None


def _SelfUpdate(bootstrap_path):
  """Update the bootstrap repository."""
  # If our bootstrap is part of a repository, we shouldn't update.
  if project_sdk.FindRepoRoot(bootstrap_path):
    return

  # If the 'skip update' variable is set, we shouldn't update.
  if os.environ.get(_BRILLO_SDK_NO_UPDATE):
    return

  # Perform the git pull to update our bootstrap.
  logging.info('Updating SDK bootstrap...')
  git.RunGit(bootstrap_path, ['pull'])

  # Prevent updating again, after we restart.
  logging.debug('Re-exec...')
  os.environ[_BRILLO_SDK_NO_UPDATE] = "1"
  commandline.ReExec()


@command.CommandDecorator('sdk')
class SdkCommand(command.CliCommand):
  """Manage Project SDK installations."""

  @classmethod
  def AddParser(cls, parser):
    super(cls, SdkCommand).AddParser(parser)

    parser.add_argument(
        '--sdk-dir', help='Force install to specific directory.')
    parser.add_argument(
        '--update', help='Update the SDK to version 1.2.3, tot, latest')

  def Run(self):
    """Run brillo sdk."""
    self.options.Freeze()

    workspace_path = workspace_lib.WorkspacePath()
    sdk_dir = self.options.sdk_dir

    if not sdk_dir and not workspace_path:
      cros_build_lib.Die('You must be in a workspace, or specifiy --sdk-dir.')

    # Perform the update.
    if self.options.update:
      bootstrap_path = bootstrap_lib.FindBootstrapPath()

      logging.info('Update bootstrap...')
      _SelfUpdate(bootstrap_path)

      logging.info('Updating SDK...')
      _HandleUpdate(
          bootstrap_path, workspace_path, sdk_dir, self.options.update)

    # Find the version (possibly post-update). We re-detect as a
    # temp hack for discovering what version 'latest' resolved as.
    version = _FindVersion(workspace_path, sdk_dir)

    if version is None:
      cros_build_lib.Die('No valid SDK found.')

    logging.info('Version: %s', version)
