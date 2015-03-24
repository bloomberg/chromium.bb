# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Manage Project SDK installation, NOT REALATED to 'cros_sdk'."""

from __future__ import print_function

from chromite.cbuildbot import constants
from chromite.cbuildbot import repository
from chromite.cli import command
from chromite.lib import bootstrap_lib
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import osutils
from chromite.lib import project_sdk
from chromite.lib import workspace_lib


@command.CommandDecorator('sdk')
class SdkCommand(command.CliCommand):
  """Manage Project SDK installations."""

  @classmethod
  def _UpdateWorkspaceSdk(cls, workspace_path, version):
    """Install specified SDK, and associate the workspace with it."""
    bootstrap_path = bootstrap_lib.FindBootstrapPath()

    if project_sdk.FindSourceRoot(bootstrap_path):
      cros_build_lib.Die('brillo sdk must run from a git clone. brbug.com/580')

    sdk_path = bootstrap_lib.ComputeSdkPath(bootstrap_path, version)

    # If this version already exists, no need to reinstall it.
    if not project_sdk.FindSourceRoot(sdk_path):
      cls._UpdateSdk(sdk_path, version)

    # Store the new version in the workspace.
    workspace_lib.SetActiveSdkVersion(workspace_path, version)

  @classmethod
  def _UpdateSdk(cls, sdk_dir, version):
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

  @classmethod
  def _HandleUpdate(cls, workspace_path, sdk_dir, version):
    if sdk_dir:
      # Install the SDK to an explicit location.
      cls._UpdateSdk(sdk_dir, version)

    if workspace_path:
      # If we are in a workspace, update the SDK for that workspace.
      cls._UpdateWorkspaceSdk(workspace_path, version)

  @classmethod
  def _FindVersion(cls, workspace_path, sdk_dir):
    if sdk_dir:
      return project_sdk.FindVersion(sdk_dir)

    if workspace_path:
      return workspace_lib.GetActiveSdkVersion(workspace_path)

    return None

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
      logging.info('Updating...')
      self._HandleUpdate(workspace_path, sdk_dir, self.options.update)

    # Find the version (possibly post-update). We re-detect as a
    # temp hack for discovering what version 'latest' resolved as.
    version = self._FindVersion(workspace_path, sdk_dir)

    if version is None:
      cros_build_lib.Die('No valid SDK found.')

    logging.info('Version: %s', version)
