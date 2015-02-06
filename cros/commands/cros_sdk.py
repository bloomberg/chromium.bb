# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Manage Project SDK installation, NOT REALATED to 'cros_sdk'."""

from __future__ import print_function

import logging

from chromite import cros
from chromite.cbuildbot import cbuildbot_config
from chromite.cbuildbot import repository
from chromite.lib import cros_build_lib
from chromite.lib import osutils
from chromite.lib import project_sdk


@cros.CommandDecorator('sdk')
class SdkCommand(cros.CrosCommand):
  """Manage Project SDK installations."""

  def _UpdateProjectSdk(self, sdk_dir, version):
    """Switch the current SDK to the specified Project SDK Manfiest and sync.

    Args:
      sdk_dir: Directory in which to create a repo, or subdir of existing repo.
      version: Project SDK version to sync.
    """
    # If the specified directory is inside an existing repo, move to the root.
    src_root = project_sdk.FindSourceRoot(sdk_dir)
    if src_root:
      sdk_dir = src_root
      # TODO(brbug.com/164): Blow away chroot, if it exists.

    # Create the SDK dir, if it doesn't already exist.
    osutils.SafeMakedirs(sdk_dir)

    # Figure out what manifest to sync into repo.
    manifest_url = cbuildbot_config.GetManifestVersionsRepoUrl(
        False, read_only=True)
    manifest_path = 'project-sdk/%s.xml' % version

    # Init new repo.
    repo = repository.RepoRepository(
        manifest_url, sdk_dir, manifest=manifest_path)

    # Sync it.
    repo.Sync(all_branches=False)

  @classmethod
  def AddParser(cls, parser):
    super(cls, SdkCommand).AddParser(parser)

    parser.add_argument('--sdk-dir', default='.',
                        help='Directory to install/update the Project SDK in.')
    parser.add_argument('--update', default=None,
                        help='Update/create Project SDK at given version.')

  def Run(self):
    """Run cros sdk."""
    self.options.Freeze()

    if self.options.update is None:
      version = project_sdk.FindVersion(self.options.sdk_dir)
      if version is None:
        cros_build_lib.Die('No valid SDK found.')
      logging.info('Version: %s', version)
    else:
      logging.info('Updating to: %s', self.options.update)
      # If we updating an existing SDK, move from nested to root dir.
      self._UpdateProjectSdk(self.options.sdk_dir, self.options.update)
