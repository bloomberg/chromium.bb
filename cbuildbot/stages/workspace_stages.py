# -*- coding: utf-8 -*-
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Build stages related to a secondary workspace directory.

A workspace is a compelete ChromeOS checkout and may contain it's own chroot,
.cache directory, etc. Conceptually, cbuildbot_launch creates a workspace for
the intitial ChromeOS build, but these stages are for creating a secondary
build.

This might be useful if a build needs to work with more than one branch at a
time, or make changes to ChromeOS code without changing the code it is currently
running.

A secondary workspace may not be inside an existing ChromeOS repo checkout.
Also, the initial sync will usually take about 40 minutes, so performance should
be considered carefully.
"""

from __future__ import print_function

import os

from chromite.cbuildbot.stages import generic_stages
from chromite.cbuildbot import repository
from chromite.lib import config_lib
from chromite.lib import constants
from chromite.lib import cros_logging as logging
from chromite.lib import cros_sdk_lib
from chromite.lib import failures_lib
from chromite.lib import osutils


class InvalidWorkspace(failures_lib.StepFailure):
  """Raised when a workspace isn't usable."""


class WorkspaceStageBase(generic_stages.BuilderStage):
  """Base class for Workspace stages."""
  def __init__(self, builder_run, workspace_dir, **kwargs):
    """Initializer.

    Args:
      builder_run: BuilderRun object.
      workspace_dir: Fully qualified path to use as a string.
    """
    super(WorkspaceStageBase, self).__init__(builder_run, **kwargs)
    self.workspace_dir = workspace_dir
    if not self.workspace_dir:
      logging.error('No worksapce_dir: %s', self.workspace_dir)
      raise InvalidWorkspace('No worksapce_dir.')


class WorkspaceCleanStage(WorkspaceStageBase):
  """Clean a working directory checkout."""

  def PerformStage(self):
    """Clean stuff!."""
    logging.info('Cleaning: %s', self.workspace_dir)

    site_config = config_lib.GetConfig()
    manifest_url = site_config.params['MANIFEST_INT_URL']
    repo = repository.RepoRepository(manifest_url, self.workspace_dir)

    #
    # TODO: This logic is copied from cbuildbot_launch, need to share.
    #

    logging.info('Remove Chroot.')
    chroot_dir = os.path.join(repo.directory, constants.DEFAULT_CHROOT_DIR)
    if os.path.exists(chroot_dir) or os.path.exists(chroot_dir + '.img'):
      cros_sdk_lib.CleanupChrootMount(chroot_dir, delete=True)

    logging.info('Remove Chrome checkout.')
    osutils.RmDir(os.path.join(repo.directory, '.cache', 'distfiles'),
                  ignore_missing=True, sudo=True)

    try:
      # If there is any failure doing the cleanup, wipe everything.
      # The previous run might have been killed in the middle leaving stale git
      # locks. Clean those up, first.
      repo.CleanStaleLocks()
      repo.BuildRootGitCleanup(prune_all=True)
    except Exception:
      logging.info('Checkout cleanup failed, wiping buildroot:', exc_info=True)
      repository.ClearBuildRoot(repo.directory)


class WorkspaceSyncStage(WorkspaceStageBase):
  """Clean a working directory checkout."""

  def __init__(self, builder_run, workspace_dir, workspace_branch, **kwargs):
    """Initializer.

    Args:
      builder_run: BuilderRun object.
      workspace_dir: Fully qualified path to use as a string.
      workspace_branch: branch to sync into the workspace.
    """
    super(WorkspaceSyncStage, self).__init__(
        builder_run, workspace_dir, **kwargs)
    self.workspace_branch = workspace_branch

  def PerformStage(self):
    """Sync stuff!."""
    logging.info('Syncing %s branch into %s',
                 self.workspace_branch, self.workspace_dir)

    #
    # TODO: This logic is copied from cbuildbot_launch, need to share.
    #

    site_config = config_lib.GetConfig()
    manifest_url = site_config.params['MANIFEST_INT_URL']
    repo = repository.RepoRepository(
        manifest_url, self.workspace_dir,
        branch=self.workspace_branch,
        git_cache_dir=self._run.options.git_cache_dir)

    repo.Sync(detach=True)
