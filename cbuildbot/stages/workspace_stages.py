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

from chromite.cbuildbot import commands
from chromite.cbuildbot import manifest_version
from chromite.cbuildbot import repository
from chromite.cbuildbot.stages import generic_stages
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
  def __init__(self, builder_run, build_root, **kwargs):
    """Initializer.

    Properties for subclasses:
      self._build_root to access the workspace directory,
      self._tot_root to access the original buildroot.

    Args:
      builder_run: BuilderRun object.
      build_root: Fully qualified path to use as a string.
    """
    super(WorkspaceStageBase, self).__init__(
        builder_run, build_root=build_root, **kwargs)

    self._tot_root = builder_run.buildroot

  def GetWorkspaceVersionInfo(self):
    """WorkspaceVersion

    Only valid after the workspace has been synced.

    Returns:
      manifest-version.VersionInfo object based on the workspace checkout.
    """
    return manifest_version.VersionInfo.from_repo(self._build_root)


class WorkspaceCleanStage(WorkspaceStageBase):
  """Clean a working directory checkout."""

  def PerformStage(self):
    """Clean stuff!."""
    logging.info('Cleaning: %s', self._build_root)

    site_config = config_lib.GetConfig()
    manifest_url = site_config.params['MANIFEST_INT_URL']
    repo = repository.RepoRepository(manifest_url, self._build_root)

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

  def __init__(self, builder_run, build_root, workspace_branch, **kwargs):
    """Initializer.

    Args:
      builder_run: BuilderRun object.
      build_root: Fully qualified path to use as a string.
      workspace_branch: branch to sync into the workspace.
    """
    super(WorkspaceSyncStage, self).__init__(
        builder_run, build_root, **kwargs)
    self.workspace_branch = workspace_branch

  def PerformStage(self):
    """Sync stuff!."""
    logging.info('Syncing %s branch into %s',
                 self.workspace_branch, self._build_root)

    #
    # TODO: This logic is copied from cbuildbot_launch, need to share.
    #

    site_config = config_lib.GetConfig()
    manifest_url = site_config.params['MANIFEST_INT_URL']
    repo = repository.RepoRepository(
        manifest_url, self._build_root,
        branch=self.workspace_branch,
        git_cache_dir=self._run.options.git_cache_dir)

    repo.Sync(detach=True)


class WorkspaceInitSDKStage(WorkspaceStageBase):
  """Stage that is responsible for initializing the SDK."""

  def PerformStage(self):
    chroot_path = os.path.join(self._build_root,
                               constants.DEFAULT_CHROOT_DIR)

    # Worksapce chroots are always wiped by cleanup stage, no need to update.
    cmd = ['cros_sdk', '--create']
    commands.RunBuildScript(self._build_root, cmd, chromite_cmd=True)

    post_ver = cros_sdk_lib.GetChrootVersion(chroot=chroot_path)
    logging.PrintBuildbotStepText(post_ver)


class WorkspaceSetupBoardStage(generic_stages.BoardSpecificBuilderStage,
                               WorkspaceStageBase):
  """Stage that is responsible for building host pkgs and setting up a board."""

  def PerformStage(self):
    usepkg = self._run.config.usepkg_build_packages
    commands.SetupBoard(
        self._build_root, board=self._current_board, usepkg=usepkg,
        chrome_binhost_only=self._run.config.chrome_binhost_only,
        force=self._run.config.board_replace,
        extra_env=self._portage_extra_env, chroot_upgrade=True,
        profile=self._run.options.profile or self._run.config.profile)


class WorkspaceBuildPackagesStage(generic_stages.BoardSpecificBuilderStage):
  """Build Chromium OS packages."""

  def PerformStage(self):
    packages = self.GetListOfPackagesToBuild()
    commands.Build(self._build_root,
                   self._current_board,
                   build_autotest=False,
                   usepkg=self._run.config.usepkg_build_packages,
                   chrome_binhost_only=self._run.config.chrome_binhost_only,
                   packages=packages,
                   skip_chroot_upgrade=True,
                   chrome_root=self._run.options.chrome_root,
                   noretry=self._run.config.nobuildretry,
                   chroot_args=None,
                   extra_env=self._portage_extra_env)
