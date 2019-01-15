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
from chromite.cbuildbot import trybot_patch_pool
from chromite.cbuildbot.stages import generic_stages
from chromite.lib import config_lib
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import cros_sdk_lib
from chromite.lib import failures_lib
from chromite.lib import portage_util
from chromite.lib import request_build

BUILD_PACKAGES_PREBUILTS = '10774.0.0'
BUILD_PACKAGES_WITH_DEBUG_SYMBOLS = '6302.0.0'

class InvalidWorkspace(failures_lib.StepFailure):
  """Raised when a workspace isn't usable."""


class WorkspaceStageBase(generic_stages.BuilderStage):
  """Base class for Workspace stages."""
  def __init__(self, builder_run, buildstore, build_root, **kwargs):
    """Initializer.

    Properties for subclasses:
      self._build_root to access the workspace directory,
      self._orig_root to access the original buildroot.

    Args:
      builder_run: BuilderRun object.
      buildstore: BuildStore instance to make DB calls with.
      build_root: Fully qualified path to use as a string.
    """
    super(WorkspaceStageBase, self).__init__(
        builder_run, buildstore, build_root=build_root,
        **kwargs)

    self._orig_root = builder_run.buildroot

  def GetWorkspaceRepo(self):
    """Fetch a repo object for the workspace.

    Returns:
      repository.RepoRepository instance for the workspace.
    """
    # TODO: Properly select the manifest. Currently hard coded to internal
    # branch checkouts.
    manifest_url = config_lib.GetSiteParams().MANIFEST_INT_URL

    # Workspace repos use the workspace URL / branch.
    return self.GetRepoRepository(
        manifest_repo_url=manifest_url,
        branch=self._run.config.workspace_branch)

  def GetWorkspaceVersionInfo(self):
    """Fetch a VersionInfo for the workspace.

    Only valid after the workspace has been synced.

    Returns:
      manifest-version.VersionInfo object based on the workspace checkout.
    """
    return manifest_version.VersionInfo.from_repo(self._build_root)

  def AfterLimit(self, limit):
    """Is worksapce version newer than cutoff limit?

    Args:
      limit: String version of format '123.0.0'

    Returns:
      bool: True if workspace has newer version than limit.
    """
    version_info = self.GetWorkspaceVersionInfo()
    return version_info > manifest_version.VersionInfo(limit)

  # Standardize manifest_versions paths for workspaces.

  @property
  def int_manifest_versions_path(self):
    """Path to use for internal manifest_versions."""
    return os.path.join(
        self._orig_root,
        config_lib.GetSiteParams().INTERNAL_MANIFEST_VERSIONS_PATH)

  @property
  def ext_manifest_versions_path(self):
    """Path to use for external manifest_versions."""
    return os.path.join(
        self._orig_root,
        config_lib.GetSiteParams().EXTERNAL_MANIFEST_VERSIONS_PATH)

  def GetWorkspaceReleaseTag(self):
    workspace_version_info = self.GetWorkspaceVersionInfo()

    if self._run.options.debug:
      build_id, _ = self._run.GetCIDBHandle()
      return 'R%s-%s-b%s' % (
          workspace_version_info.chrome_branch,
          workspace_version_info.VersionString(),
          build_id)
    else:
      return 'R%s-%s' % (
          workspace_version_info.chrome_branch,
          workspace_version_info.VersionString())


class SyncStage(WorkspaceStageBase):
  """Perform a repo sync."""

  category = constants.CI_INFRA_STAGE

  def __init__(self, builder_run, buildstore, build_root,
               external=False,
               branch=None,
               version=None,
               patch_pool=None,
               **kwargs):
    """Initializer.

    Args:
      builder_run: BuilderRun object.
      buildstore: BuildStore instance to make DB calls with.
      build_root: Path to sync into.
      external: Boolean telling if this an internal or external checkout.
      branch: Branch to sync, with default to master.
      version: Version number to sync too.
      patch_pool: None or a list of lib.patch.GerritPatch objects.
    """
    super(SyncStage, self).__init__(
        builder_run, buildstore, build_root=build_root, **kwargs)

    self.external = external
    self.branch = branch
    self.version = version
    self.patch_pool = patch_pool

  def PerformStage(self):
    """Sync stuff!"""
    logging.info('SubWorkspaceSync')

    cmd = [
        os.path.join(constants.CHROMITE_DIR, 'scripts', 'repo_sync_manifest'),
        '--repo-root', self._build_root,
        '--manifest-versions-int', self.int_manifest_versions_path,
        '--manifest-versions-ext', self.ext_manifest_versions_path,
    ]

    if self.external:
      cmd += ['--external']

    if self.branch and not self.version:
      cmd += ['--branch', self.branch]

    if self.version:
      cmd += ['--version', self.version]

    if self.patch_pool:
      patch_options = []
      for patch in self.patch_pool:
        logging.PrintBuildbotLink(str(patch), patch.url)
        patch_options += ['--gerrit-patches', patch.gerrit_number_str]

      cmd += patch_options

    assert not (self.version and self.patch_pool), (
        'Can\'t cherry-pick "%s" into an official version "%s."' %
        (patch_options, self.version))

    cros_build_lib.RunCommand(cmd)


class WorkspaceSyncStage(WorkspaceStageBase):
  """Checkout both infra and workspace repos."""

  category = constants.CI_INFRA_STAGE

  def PerformStage(self):
    """Sync all the stuff!"""
    # Select changes to cherry-pick into the build, and filter them into
    # chromite versus branch changes.
    patch_pool = trybot_patch_pool.TrybotPatchPool.FromOptions(
        gerrit_patches=self._run.options.gerrit_patches)

    infra_pool = patch_pool.FilterFn(trybot_patch_pool.ChromiteFilter)
    branch_pool = patch_pool.FilterFn(trybot_patch_pool.ChromiteFilter,
                                      negate=True)

    SyncStage(
        self._run,
        self.buildstore,
        build_root=self._orig_root,
        external=True,
        branch='master',
        patch_pool=infra_pool,
        suffix=' [Infra]').Run()

    branch = self._run.config.workspace_branch

    SyncStage(
        self._run,
        self.buildstore,
        build_root=self._build_root,
        external=not self._run.config.internal,
        branch=branch,
        version=self._run.options.force_version,
        patch_pool=branch_pool,
        suffix=' [%s]' % branch).Run()


class WorkspaceSyncChromeStage(WorkspaceStageBase):
  """Stage that syncs Chrome sources if needed."""
  option_name = 'managed_chrome'
  category = constants.PRODUCT_CHROME_STAGE

  def DetermineChromeVersion(self):
    cpv = portage_util.PortageqBestVisible(constants.CHROME_CP,
                                           cwd=self._build_root)
    return cpv.version_no_rev.partition('_')[0]


  @failures_lib.SetFailureType(failures_lib.InfrastructureFailure)
  def PerformStage(self):
    chrome_version = self.DetermineChromeVersion()

    logging.PrintBuildbotStepText('tag %s' % chrome_version)

    useflags = self._run.config.useflags
    commands.SyncChrome(build_root=self._orig_root,
                        chrome_root=self._run.options.chrome_root,
                        useflags=useflags,
                        tag=chrome_version)


class WorkspaceUprevAndPublishStage(WorkspaceStageBase):
  """Uprev ebuilds, and immediately publish them.

  This stage updates ebuilds to top of branch with no verification, or prebuilt
  generation. This is generally intended only for branch builds.
  """
  config = 'push_overlays'

  def __init__(self, builder_run, buildstore, boards=None, **kwargs):
    super(WorkspaceUprevAndPublishStage, self).__init__(builder_run,
                                                        buildstore,
                                                        **kwargs)
    if boards is not None:
      self._boards = boards

  def PerformStage(self):
    """Perform the uprev and push."""
    commands.UprevPackages(self._orig_root, self._boards,
                           overlay_type=self._run.config.overlays,
                           workspace=self._build_root)

    logging.info('Pushing.')
    commands.UprevPush(self._orig_root,
                       overlay_type=self._run.config.push_overlays,
                       dryrun=self._run.options.debug,
                       workspace=self._build_root)


class WorkspacePublishBuildspecStage(WorkspaceStageBase):
  """Increment the ChromeOS version, and publish a buildspec."""

  def PerformStage(self):
    """Increment ChromeOS version, and publish buildpec."""
    repo = self.GetWorkspaceRepo()

    # TODO: Add 'patch' support somehow,
    if repo.branch == 'master':
      incr_type = 'build'
    else:
      incr_type = 'branch'

    build_spec_path = manifest_version.GenerateAndPublishOfficialBuildSpec(
        repo,
        incr_type,
        manifest_versions_int=self.int_manifest_versions_path,
        manifest_versions_ext=self.ext_manifest_versions_path,
        dryrun=self._run.options.debug)

    if self._run.options.debug:
      msg = 'DEBUG: Would have defined: %s' % build_spec_path
    else:
      msg = 'Defined: %s' % build_spec_path

    logging.PrintBuildbotStepText(msg)


class WorkspaceScheduleChildrenStage(WorkspaceStageBase):
  """Schedule child builds for this buildspec."""

  def PerformStage(self):
    """Schedule child builds for this buildspec."""
    build_id, _ = self._run.GetCIDBHandle()
    master_buildbucket_id = self._run.options.buildbucket_id
    version_info = self.GetWorkspaceVersionInfo()

    extra_args = [
        '--buildbot',
        '--version', version_info.VersionString(),
    ]

    if self._run.options.debug:
      extra_args.append('--debug')

    for child_name in self._run.config.slave_configs:
      child = request_build.RequestBuild(
          build_config=child_name,
          master_cidb_id=build_id,
          master_buildbucket_id=master_buildbucket_id,
          extra_args=extra_args,
      )
      result = child.Submit(dryrun=self._run.options.debug)

      logging.info(
          'Build_name %s buildbucket_id %s created_timestamp %s',
          result.build_config, result.buildbucket_id, result.created_ts)
      logging.PrintBuildbotLink(result.build_config, result.url)

class WorkspaceInitSDKStage(WorkspaceStageBase):
  """Stage that is responsible for initializing the SDK."""

  category = constants.CI_INFRA_STAGE

  def PerformStage(self):
    chroot_path = os.path.join(self._build_root,
                               constants.DEFAULT_CHROOT_DIR)

    # Worksapce chroots are always wiped by cleanup stage, no need to update.
    cmd = ['cros_sdk', '--create', '--cache-dir', self._run.options.cache_dir]
    commands.RunBuildScript(self._build_root, cmd, chromite_cmd=True)

    post_ver = cros_sdk_lib.GetChrootVersion(chroot_path)
    logging.PrintBuildbotStepText(post_ver)


class WorkspaceSetupBoardStage(generic_stages.BoardSpecificBuilderStage,
                               WorkspaceStageBase):
  """Stage that is responsible for building host pkgs and setting up a board."""
  category = constants.CI_INFRA_STAGE

  def PerformStage(self):
    usepkg = self._run.config.usepkg_build_packages
    commands.SetupBoard(
        self._build_root, board=self._current_board, usepkg=usepkg,
        force=self._run.config.board_replace,
        extra_env=self._portage_extra_env, chroot_upgrade=True,
        profile=self._run.options.profile or self._run.config.profile,
        chroot_args=['--cache-dir', self._run.options.cache_dir])


class WorkspaceBuildPackagesStage(generic_stages.BoardSpecificBuilderStage,
                                  WorkspaceStageBase):
  """Build Chromium OS packages."""

  category = constants.PRODUCT_OS_STAGE

  def PerformStage(self):
    usepkg = False
    if self.AfterLimit(BUILD_PACKAGES_PREBUILTS):
      usepkg = self._run.config.usepkg_build_packages

    packages = self.GetListOfPackagesToBuild()

    cmd = ['./build_packages', '--board=%s' % self._current_board,
           '--accept_licenses=@CHROMEOS', '--skip_chroot_upgrade']

    if not self._run.options.tests:
      cmd.append('--nowithautotest')

    if self.AfterLimit(BUILD_PACKAGES_WITH_DEBUG_SYMBOLS):
      cmd.append('--withdebugsymbols')

    if not usepkg:
      cmd.extend(commands.LOCAL_BUILD_FLAGS)

    if self._run.config.nobuildretry:
      cmd.append('--nobuildretry')

    chroot_args = ['--cache-dir', self._run.options.cache_dir]
    if self._run.options.chrome_root:
      chroot_args += ['--chrome_root', self._run.options.chrome_root]

    # TODO: Add event file handling, for build package performance tracking.
    #if self.AfterLimit(BUILD_PACKAGES_EVENTS):
    #  cmd.append('--withevents')
    #  cmd.append('--eventfile=%s' % event_file)

    cmd.extend(packages)

    commands.RunBuildScript(
        self._build_root, cmd, chroot_args=chroot_args, enter_chroot=True)
