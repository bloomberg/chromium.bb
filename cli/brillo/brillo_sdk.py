# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Manage Project SDK installation, NOT REALATED to 'cros_sdk'."""

from __future__ import print_function

import os
import tempfile

from chromite.cbuildbot import constants
from chromite.cbuildbot import repository
from chromite.cli import command
from chromite.lib import bootstrap_lib
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import git
from chromite.lib import gs
from chromite.lib import osutils
from chromite.lib import project_sdk
from chromite.lib import workspace_lib


_BRILLO_SDK_NO_UPDATE = 'BRILLO_SDK_NO_UPDATE'


def _ResolveLatest(gs_ctx):
  """Find the 'latest' SDK release."""
  return gs_ctx.Cat(constants.BRILLO_LATEST_RELEASE_URL).strip()


def _UpdateWorkspaceSdk(gs_ctx, bootstrap_path, workspace_path, version):
  """Install specified SDK, and associate the workspace with it.

  Args:
    gs_ctx: GS Context to use.
    bootstrap_path: Directory to the bootstrap.
    workspace_path: Directory that holds the workspace.
    version: Project SDK version to sync.
  """
  if project_sdk.FindRepoRoot(bootstrap_path):
    cros_build_lib.Die('brillo sdk must run from a git clone. brbug.com/580')

  sdk_path = bootstrap_lib.ComputeSdkPath(bootstrap_path, version)

  # If this version already exists, no need to reinstall it.
  if not os.path.exists(sdk_path) or version == 'tot':
    _UpdateSdk(gs_ctx, sdk_path, version)

  # Store the new version in the workspace.
  workspace_lib.SetActiveSdkVersion(workspace_path, version)


def _UpdateSdk(gs_ctx, sdk_dir, version):
  """Install the specified SDK at the specified location.

  Args:
    gs_ctx: GS Context to use.
    sdk_dir: Directory in which to create a repo.
    version: Project SDK version to sync.
  """
  # Create the SDK dir, if it doesn't already exist.
  osutils.SafeMakedirs(sdk_dir)

  logging.notice('Fetching files. This could take a few minutes...')
  # TOT is a special case, handle it first.
  if version.lower() == 'tot':
    # Init new repo.
    repo = repository.RepoRepository(
        constants.MANIFEST_URL, sdk_dir, groups='project_sdk')
    # Sync it.
    repo.Sync()
    return

  with tempfile.NamedTemporaryFile() as manifest:
    # Fetch manifest into temp file.
    manifest_url = os.path.join(constants.BRILLO_RELEASE_MANIFESTS_URL,
                                '%s.xml' % version)
    gs_ctx.Copy(manifest_url, manifest.name)

    with osutils.TempDir() as manifest_git_dir:
      # Convert manifest into a git repository for the repo command.
      repository.PrepManifestForRepo(manifest_git_dir, manifest.name)

      # Fetch the SDK.
      repo = repository.RepoRepository(manifest_git_dir, sdk_dir, depth=1)
      repo.Sync()

  # TODO(dgarrett): Embed this step into the manifest itself.
  # Write out the SDK Version.
  sdk_version_file = project_sdk.VersionFile(sdk_dir)
  osutils.WriteFile(sdk_version_file, version)


def _HandleUpdate(bootstrap_path, workspace_path, sdk_dir, version):
  """Handle an update request for a variety of conditions.

  Args:
    bootstrap_path: Path to bootstrap location (or None).
    workspace_path: Path to workspace location (or None).
    sdk_dir: Directory in which to create a repo (or None).
    version: Project SDK version to sync.
  """
  # We fetch versioned manifests from GS.
  gs_ctx = gs.GSContext()

  # Resolve 'latest' into a concrete version.
  if version.lower() == 'latest':
    version = _ResolveLatest(gs_ctx)

  if sdk_dir:
    # Install the SDK to an explicit location.
    _UpdateSdk(gs_ctx, sdk_dir, version)

  elif workspace_path:
    # If we are in a workspace, update the SDK for that workspace.
    _UpdateWorkspaceSdk(gs_ctx, bootstrap_path, workspace_path, version)


def _FindVersion(workspace_path, sdk_dir):
  """Find SDK version of an existing specific sdk_dir.

  Args:
    workspace_path: Path to workspace location (or None).
    sdk_dir: Directory in which to create a repo (or None).
  """
  if sdk_dir:
    # This will find a version, if it's an official release manifest checkout.
    version = project_sdk.FindVersion(sdk_dir)

    if version is None:
      # If it's not official, use a heuristic to see if it's an SDK at all.
      sdk_root = project_sdk.FindRepoRoot(sdk_dir)
      expected = ('chromite', 'src')
      if (sdk_root and
          all([os.path.exists(os.path.join(sdk_root, d)) for d in expected])):
        version = 'Unofficial SDK'

    return version

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

    # Must run outside the chroot.
    cros_build_lib.AssertOutsideChroot()

    workspace_path = workspace_lib.WorkspacePath()
    sdk_dir = self.options.sdk_dir

    if not sdk_dir and not workspace_path:
      cros_build_lib.Die('You must be in a workspace, or specifiy --sdk-dir.')

    # Perform the update.
    if self.options.update:
      bootstrap_path = bootstrap_lib.FindBootstrapPath()

      logging.info('Update bootstrap...')
      _SelfUpdate(bootstrap_path)

      logging.notice('Updating SDK...')
      _HandleUpdate(
          bootstrap_path, workspace_path, sdk_dir, self.options.update)

    # Find the version (possibly post-update). We re-detect as a
    # temp hack for discovering what version 'latest' resolved as.
    version = _FindVersion(workspace_path, sdk_dir)

    if version is None:
      cros_build_lib.Die('No valid SDK found.')

    logging.notice('Version: %s', version)
