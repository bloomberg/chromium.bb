# -*- coding: utf-8 -*-
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing the sync stages."""

from __future__ import print_function

import collections
import contextlib
import os
import re
import sys
from xml.etree import ElementTree
from xml.dom import minidom

from chromite.cbuildbot import commands
from chromite.cbuildbot import lkgm_manager
from chromite.cbuildbot import manifest_version
from chromite.cbuildbot import patch_series
from chromite.cbuildbot import trybot_patch_pool
from chromite.cbuildbot.stages import generic_stages
from chromite.cbuildbot.stages import build_stages
from chromite.lib import config_lib
from chromite.lib import constants
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import failures_lib
from chromite.lib import git
from chromite.lib import osutils
from chromite.lib import patch as cros_patch
from chromite.lib import timeout_util
from chromite.scripts import cros_mark_android_as_stable
from chromite.scripts import cros_mark_chrome_as_stable


class PatchChangesStage(generic_stages.BuilderStage):
  """Stage that patches a set of Gerrit changes to the buildroot source tree."""

  category = constants.CI_INFRA_STAGE

  def __init__(self, builder_run, buildstore, patch_pool, **kwargs):
    """Construct a PatchChangesStage.

    Args:
      builder_run: BuilderRun object.
      buildstore: BuildStore instance to make DB calls with.
      patch_pool: A TrybotPatchPool object containing the different types of
                  patches to apply.
    """
    super(PatchChangesStage, self).__init__(builder_run, buildstore, **kwargs)
    self.patch_pool = patch_pool

  @staticmethod
  def _CheckForDuplicatePatches(_series, changes):
    conflicts = {}
    duplicates = []
    for change in changes:
      if change.id is None:
        logging.warning(
            'Change %s lacks a usable ChangeId; duplicate checking cannot '
            'be done for this change.  If cherry-picking fails, this is a '
            'potential cause.', change)
        continue
      conflicts.setdefault(change.id, []).append(change)

    duplicates = [x for x in conflicts.values() if len(x) > 1]
    if not duplicates:
      return changes

    for conflict in duplicates:
      logging.error(
          'Changes %s conflict with each other- they have same id %s., '.join(
              str(x) for x in conflict), conflict[0].id)

    cros_build_lib.Die('Duplicate patches were encountered: %s', duplicates)

  def _PatchSeriesFilter(self, series, changes):
    return self._CheckForDuplicatePatches(series, changes)

  def _ApplyPatchSeries(self, series, patch_pool, **kwargs):
    """Applies a patch pool using a patch series."""
    kwargs.setdefault('frozen', False)
    # Honor the given ordering, so that if a gerrit/remote patch
    # conflicts w/ a local patch, the gerrit/remote patch are
    # blamed rather than local (patch ordering is typically
    # local, gerrit, then remote).
    kwargs.setdefault('honor_ordering', True)
    kwargs['changes_filter'] = self._PatchSeriesFilter

    _applied, failed_tot, failed_inflight = series.Apply(
        list(patch_pool), **kwargs)

    failures = failed_tot + failed_inflight
    if failures:
      self.HandleApplyFailures(failures)

  def HandleApplyFailures(self, failures):
    cros_build_lib.Die('Failed applying patches: %s', '\n'.join(
        str(x) for x in failures))

  def PerformStage(self):

    class NoisyPatchSeries(patch_series.PatchSeries):
      """Custom PatchSeries that adds links to buildbot logs for remote trys."""

      def ApplyChange(self, change):
        if isinstance(change, cros_patch.GerritPatch):
          logging.PrintBuildbotLink(str(change), change.url)
        elif isinstance(change, cros_patch.UploadedLocalPatch):
          logging.PrintBuildbotStepText(str(change))

        return patch_series.PatchSeries.ApplyChange(self, change)

    # If we're an external builder, ignore internal patches.
    helper_pool = patch_series.HelperPool.SimpleCreate(
        cros_internal=self._run.config.internal, cros=True)

    # Limit our resolution to non-manifest patches.
    patches = NoisyPatchSeries(
        self._build_root,
        helper_pool=helper_pool,
        deps_filter_fn=lambda p: not trybot_patch_pool.ManifestFilter(p))

    self._ApplyPatchSeries(patches, self.patch_pool)


class BootstrapStage(PatchChangesStage):
  """Stage that patches a chromite repo and re-executes inside it.

  Attributes:
    returncode - the returncode of the cbuildbot re-execution.  Valid after
                 calling stage.Run().
  """
  option_name = 'bootstrap'
  category = constants.CI_INFRA_STAGE

  def __init__(self, builder_run, buildstore, patch_pool, **kwargs):
    super(BootstrapStage, self).__init__(builder_run, buildstore,
                                         trybot_patch_pool.TrybotPatchPool(),
                                         **kwargs)

    self.patch_pool = patch_pool
    self.returncode = None
    self.tempdir = None

  def _ApplyManifestPatches(self, patch_pool):
    """Apply a pool of manifest patches to a temp manifest checkout.

    Args:
      patch_pool: The pool to apply.

    Returns:
      The path to the patched manifest checkout.

    Raises:
      Exception, if the new patched manifest cannot be parsed.
    """
    checkout_dir = os.path.join(self.tempdir, 'manfest-checkout')
    git.Clone(checkout_dir, self._run.config.manifest_repo_url)

    patches = patch_series.PatchSeries.WorkOnSingleRepo(
        checkout_dir, tracking_branch=self._run.manifest_branch)
    self._ApplyPatchSeries(patches, patch_pool)

    # Verify that the patched manifest loads properly. Propagate any errors as
    # exceptions.
    manifest = os.path.join(checkout_dir, self._run.config.manifest)
    git.Manifest.Cached(manifest, manifest_include_dir=checkout_dir)
    return checkout_dir

  @staticmethod
  def _FilterArgsForApi(parsed_args, api_minor):
    """Remove arguments that are introduced after an api version."""

    def filter_fn(passed_arg):
      return passed_arg.opt_inst.api_version <= api_minor

    accepted, removed = commandline.FilteringParser.FilterArgs(
        parsed_args, filter_fn)

    if removed:
      logging.warning("The following arguments were removed due to api: '%s'",
                      ' '.join(removed))
    return accepted

  @classmethod
  def FilterArgsForTargetCbuildbot(cls, buildroot, cbuildbot_path, options):
    _, minor = commands.GetTargetChromiteApiVersion(buildroot)
    args = [cbuildbot_path]
    args.append(options.build_config_name)
    args.extend(cls._FilterArgsForApi(options.parsed_args, minor))

    # Only pass down --cache-dir if it was specified. By default, we want
    # the cache dir to live in the root of each checkout, so this means that
    # each instance of cbuildbot needs to calculate the default separately.
    if minor >= 2 and options.cache_dir_specified:
      args += ['--cache-dir', options.cache_dir]

    if minor >= constants.REEXEC_API_TSMON_TASK_NUM:
      # Increment the ts-mon task_num so the metrics don't collide.
      args.extend(['--ts-mon-task-num', str(options.ts_mon_task_num + 1)])

    return args

  @classmethod
  def BootstrapPatchesNeeded(cls, builder_run, patch_pool):
    """See if bootstrapping is needed for any of the given patches.

    Does NOT determine if they have already been applied.

    Args:
      builder_run: BuilderRun object for this build.
      patch_pool: All patches to be applied this run.

    Returns:
      boolean True if bootstrapping is needed.
    """
    chromite_pool = patch_pool.Filter(project=constants.CHROMITE_PROJECT)
    if builder_run.config.internal:
      manifest_pool = patch_pool.FilterIntManifest()
    else:
      manifest_pool = patch_pool.FilterExtManifest()

    return bool(chromite_pool or manifest_pool)

  def HandleApplyFailures(self, failures):
    """Handle the case where patches fail to apply."""
    if self._run.config.pre_cq:
      # Let the PreCQSync stage handle this failure. The PreCQSync stage will
      # comment on CLs with the appropriate message when they fail to apply.
      #
      # WARNING: For manifest patches, the Pre-CQ attempts to apply external
      # patches to the internal manifest, and this means we may flag a conflict
      # here even if the patch applies cleanly. TODO(davidjames): Fix this.
      logging.PrintBuildbotStepWarnings()
      logging.error('Failed applying patches: %s\n'.join(
          str(x) for x in failures))
    else:
      PatchChangesStage.HandleApplyFailures(self, failures)

  def _PerformStageInTempDir(self):
    # The plan for the builders is to use master branch to bootstrap other
    # branches. Now, if we wanted to test patches for both the bootstrap code
    # (on master) and the branched chromite (say, R20), we need to filter the
    # patches by branch.
    filter_branch = self._run.manifest_branch
    if self._run.options.test_bootstrap:
      filter_branch = 'master'

    # Filter all requested patches for the branch.
    branch_pool = self.patch_pool.FilterBranch(filter_branch)

    # Checkout the new version of chromite, and patch it.
    chromite_dir = os.path.join(self.tempdir, 'chromite')
    reference_repo = os.path.join(constants.CHROMITE_DIR, '.git')
    git.Clone(chromite_dir, constants.CHROMITE_URL, reference=reference_repo)
    git.RunGit(chromite_dir, ['checkout', filter_branch])

    chromite_pool = branch_pool.Filter(project=constants.CHROMITE_PROJECT)
    if chromite_pool:
      patches = patch_series.PatchSeries.WorkOnSingleRepo(
          chromite_dir, filter_branch)
      self._ApplyPatchSeries(patches, chromite_pool)

    # Re-exec into new instance of cbuildbot, with proper command line args.
    cbuildbot_path = constants.PATH_TO_CBUILDBOT
    if not os.path.exists(os.path.join(self.tempdir, cbuildbot_path)):
      cbuildbot_path = 'chromite/cbuildbot/cbuildbot'
    cmd = self.FilterArgsForTargetCbuildbot(self.tempdir, cbuildbot_path,
                                            self._run.options)

    extra_params = ['--sourceroot', self._run.options.sourceroot]
    extra_params.extend(self._run.options.bootstrap_args)
    if self._run.options.test_bootstrap:
      # We don't want re-executed instance to see this.
      cmd = [a for a in cmd if a != '--test-bootstrap']
    else:
      # If we've already done the desired number of bootstraps, disable
      # bootstrapping for the next execution.  Also pass in the patched manifest
      # repository.
      extra_params.append('--nobootstrap')
      if self._run.config.internal:
        manifest_pool = branch_pool.FilterIntManifest()
      else:
        manifest_pool = branch_pool.FilterExtManifest()

      if manifest_pool:
        manifest_dir = self._ApplyManifestPatches(manifest_pool)
        extra_params.extend(['--manifest-repo-url', manifest_dir])

    cmd += extra_params
    result_obj = cros_build_lib.run(
        cmd, cwd=self.tempdir, kill_timeout=30, error_code_ok=True)
    self.returncode = result_obj.returncode

  def PerformStage(self):
    with osutils.TempDir(base_dir=self._run.options.bootstrap_dir) as tempdir:
      self.tempdir = tempdir
      self._PerformStageInTempDir()
    self.tempdir = None


class SyncStage(generic_stages.BuilderStage):
  """Stage that performs syncing for the builder."""

  option_name = 'sync'
  output_manifest_sha1 = True
  category = constants.CI_INFRA_STAGE

  def __init__(self, builder_run, buildstore, **kwargs):
    super(SyncStage, self).__init__(builder_run, buildstore, **kwargs)
    self.repo = None
    self.skip_sync = False

    # TODO(mtennant): Why keep a duplicate copy of this config value
    # at self.internal when it can always be retrieved from config?
    self.internal = self._run.config.internal
    self.buildbucket_client = self.GetBuildbucketClient()

  def _GetManifestVersionsRepoUrl(self, internal=None, test=False):
    if internal is None:
      internal = self._run.config.internal

    site_params = config_lib.GetSiteParams()
    if internal:
      if test:
        return site_params.MANIFEST_VERSIONS_INT_GOB_URL_TEST
      else:
        return site_params.MANIFEST_VERSIONS_INT_GOB_URL
    else:
      if test:
        return site_params.MANIFEST_VERSIONS_GOB_URL_TEST
      else:
        return site_params.MANIFEST_VERSIONS_GOB_URL

  def Initialize(self):
    self._InitializeRepo()

  def _InitializeRepo(self):
    """Set up the RepoRepository object."""
    self.repo = self.GetRepoRepository()

  def GetNextManifest(self):
    """Returns the manifest to use."""
    return self._run.config.manifest

  def ManifestCheckout(self, next_manifest, fetch_all=False):
    """Checks out the repository to the given manifest."""
    self._Print('\n'.join([
        'BUILDROOT: %s' % self.repo.directory,
        'TRACKING BRANCH: %s' % self.repo.branch,
        'NEXT MANIFEST: %s' % next_manifest
    ]))

    if not self.skip_sync:
      self.repo.Sync(next_manifest)

    print(
        self.repo.ExportManifest(mark_revision=self.output_manifest_sha1),
        file=sys.stderr)

    if fetch_all:
      # Perform git fetch all on projects to resolve any git corruption
      # that may occur due to flake.
      # http://crbug/921407
      self.repo.FetchAll()

  def RunPrePatchBuild(self):
    """Run through a pre-patch build to prepare for incremental build.

    This function runs though the InitSDKStage, SetupBoardStage, and
    BuildPackagesStage. It is intended to be called before applying
    any patches under test, to prepare the chroot and sysroot in a state
    corresponding to ToT prior to an incremental build.

    Returns:
      True if all stages were successful, False if any of them failed.
    """
    suffix = ' (pre-Patch)'
    try:
      build_stages.InitSDKStage(
          self._run, self.buildstore, chroot_replace=True, suffix=suffix).Run()
      for builder_run in self._run.GetUngroupedBuilderRuns():
        for board in builder_run.config.boards:
          build_stages.SetupBoardStage(
              builder_run, self.buildstore, board=board, suffix=suffix).Run()
          build_stages.BuildPackagesStage(
              builder_run, self.buildstore, board=board, suffix=suffix).Run()
    except failures_lib.StepFailure:
      return False

    return True

  @failures_lib.SetFailureType(failures_lib.InfrastructureFailure)
  def PerformStage(self):
    self.Initialize()
    with osutils.TempDir() as tempdir:
      # Save off the last manifest.
      fresh_sync = True
      if os.path.exists(self.repo.directory) and not self._run.options.clobber:
        old_filename = os.path.join(tempdir, 'old.xml')
        try:
          old_contents = self.repo.ExportManifest()
        except cros_build_lib.RunCommandError as e:
          logging.warning(str(e))
        else:
          osutils.WriteFile(old_filename, old_contents)
          fresh_sync = False

      # Sync.
      self.ManifestCheckout(self.GetNextManifest())

      # Print the blamelist.
      if fresh_sync:
        logging.PrintBuildbotStepText('(From scratch)')
      elif self._run.options.buildbot:
        lkgm_manager.GenerateBlameList(self.repo, old_filename)

      # Incremental builds request an additional build before patching changes.
      if self._run.config.build_before_patching:
        pre_build_passed = self.RunPrePatchBuild()
        if not pre_build_passed:
          logging.PrintBuildbotStepText('Pre-patch build failed.')


class ManifestVersionedSyncStage(SyncStage):
  """Stage that generates a unique manifest file, and sync's to it."""

  # TODO(mtennant): Make this into a builder run value.
  output_manifest_sha1 = False
  category = constants.CI_INFRA_STAGE

  def __init__(self, builder_run, buildstore, **kwargs):
    # Perform the sync at the end of the stage to the given manifest.
    super(ManifestVersionedSyncStage, self).__init__(builder_run, buildstore,
                                                     **kwargs)
    self.repo = None
    self.manifest_manager = None

    # If a builder pushes changes (even with dryrun mode), we need a writable
    # repository. Otherwise, the push will be rejected by the server.
    self.manifest_repo = self._GetManifestVersionsRepoUrl()

    # 1. Our current logic for calculating whether to re-run a build assumes
    #    that if the build is green, then it doesn't need to be re-run. This
    #    isn't true for canary masters, because the canary master ignores the
    #    status of its slaves and is green even if they fail. So set
    #    force=True in this case.
    # 2. If we're running with --debug, we should always run through to
    #    completion, so as to ensure a complete test.
    self._force = self._run.config.master or self._run.options.debug

  def HandleSkip(self):
    """Initializes a manifest manager to the specified version if skipped."""
    super(ManifestVersionedSyncStage, self).HandleSkip()
    if self._run.options.force_version:
      self.Initialize()
      self.ForceVersion(self._run.options.force_version)

  def ForceVersion(self, version):
    """Creates a manifest manager from given version and returns manifest."""
    logging.PrintBuildbotStepText(version)
    return self.manifest_manager.BootstrapFromVersion(version)

  def VersionIncrementType(self):
    """Return which part of the version number should be incremented."""
    if self._run.manifest_branch == 'master':
      return 'build'

    return 'branch'

  def RegisterManifestManager(self, manifest_manager):
    """Save the given manifest manager for later use in this run.

    Args:
      manifest_manager: Expected to be a BuildSpecsManager.
    """
    self._run.attrs.manifest_manager = self.manifest_manager = manifest_manager

  def Initialize(self):
    """Initializes a manager that manages manifests for associated stages."""

    dry_run = self._run.options.debug

    self._InitializeRepo()

    # If chrome_rev is somehow set, fail.
    assert not self._chrome_rev, \
        'chrome_rev is unsupported on release builders.'

    self.RegisterManifestManager(
        manifest_version.BuildSpecsManager(
            source_repo=self.repo,
            manifest_repo=self.manifest_repo,
            manifest=self._run.config.manifest,
            build_names=self._run.GetBuilderIds(),
            incr_type=self.VersionIncrementType(),
            force=self._force,
            branch=self._run.manifest_branch,
            dry_run=dry_run,
            config=self._run.config,
            metadata=self._run.attrs.metadata,
            buildstore=self.buildstore,
            buildbucket_client=self.buildbucket_client))

  def _SetAndroidVersionIfApplicable(self, manifest):
    """If 'android' is in |manifest|, write version to the BuilderRun object.

    Args:
      manifest: Path to the manifest.
    """
    manifest_dom = minidom.parse(manifest)
    elements = manifest_dom.getElementsByTagName(lkgm_manager.ANDROID_ELEMENT)

    if elements:
      android_version = elements[0].getAttribute(
          lkgm_manager.ANDROID_VERSION_ATTR)
      logging.info('Android version was found in the manifest: %s',
                   android_version)
      # Update the metadata dictionary. This is necessary because the
      # metadata dictionary is preserved through re-executions, so
      # UprevAndroidStage can read the version from the dictionary
      # later. This is easier than parsing the manifest again after
      # the re-execution.
      self._run.attrs.metadata.UpdateKeyDictWithDict(
          'version', {'android': android_version})

  def _SetChromeVersionIfApplicable(self, manifest):
    """If 'chrome' is in |manifest|, write the version to the BuilderRun object.

    Args:
      manifest: Path to the manifest.
    """
    manifest_dom = minidom.parse(manifest)
    elements = manifest_dom.getElementsByTagName(lkgm_manager.CHROME_ELEMENT)

    if elements:
      chrome_version = elements[0].getAttribute(
          lkgm_manager.CHROME_VERSION_ATTR)
      logging.info('Chrome version was found in the manifest: %s',
                   chrome_version)
      # Update the metadata dictionary. This is necessary because the
      # metadata dictionary is preserved through re-executions, so
      # SyncChromeStage can read the version from the dictionary
      # later. This is easier than parsing the manifest again after
      # the re-execution.
      self._run.attrs.metadata.UpdateKeyDictWithDict('version',
                                                     {'chrome': chrome_version})

  def GetNextManifest(self):
    """Uses the initialized manifest manager to get the next manifest."""
    assert self.manifest_manager, \
        'Must run GetStageManager before checkout out build.'

    build_id = self._run.attrs.metadata.GetDict().get('build_id')

    to_return = self.manifest_manager.GetNextBuildSpec(build_id=build_id)
    logging.info('Found next version to build: %s', to_return)
    previous_version = self.manifest_manager.GetLatestPassingSpec()
    target_version = self.manifest_manager.current_version

    # Print the Blamelist here.
    url_prefix = 'https://crosland.corp.google.com/log/'
    url = url_prefix + '%s..%s' % (previous_version, target_version)
    logging.PrintBuildbotLink('Blamelist', url)
    # The testManifestVersionedSyncOnePartBranch interacts badly with this
    # function.  It doesn't fully initialize self.manifest_manager which
    # causes target_version to be None.  Since there isn't a clean fix in
    # either direction, just throw this through str().  In the normal case,
    # it's already a string anyways.
    logging.PrintBuildbotStepText(str(target_version))

    return to_return

  @contextlib.contextmanager
  def LocalizeManifest(self, manifest, filter_cros=False):
    """Remove restricted checkouts from the manifest if needed.

    Args:
      manifest: The manifest to localize.
      filter_cros: If set, then only checkouts with a remote of 'cros' or
        'cros-internal' are kept, and the rest are filtered out.
    """
    if filter_cros:
      with osutils.TempDir() as tempdir:
        filtered_manifest = os.path.join(tempdir, 'filtered.xml')
        doc = ElementTree.parse(manifest)
        root = doc.getroot()
        for node in root.findall('project'):
          remote = node.attrib.get('remote')
          if remote and remote not in config_lib.GetSiteParams().GIT_REMOTES:
            root.remove(node)
        doc.write(filtered_manifest)
        yield filtered_manifest
    else:
      yield manifest

  def _GetMasterVersion(self, master_id, timeout=5 * 60):
    """Get the platform version associated with the master_build_id.

    Args:
      master_id: Our master buildbucket id.
      timeout: How long to wait for the platform version to show up
        in the database. This is needed because the slave builders are
        triggered slightly before the platform version is written. Default
        is 5 minutes.
    """

    # TODO(davidjames): Remove the wait loop here once we've updated slave
    # builders to only get triggered after the platform version is written.
    def _PrintRemainingTime(remaining):
      logging.info('%s until timeout...', remaining)

    def _GetPlatformVersion():
      status = self.buildstore.GetBuildStatuses(buildbucket_ids=[master_id])[0]
      return status['platform_version']

    # Retry until non-None version is returned.
    def _ShouldRetry(x):
      return not x

    return timeout_util.WaitForSuccess(
        _ShouldRetry,
        _GetPlatformVersion,
        timeout,
        period=constants.SLEEP_TIMEOUT,
        side_effect_func=_PrintRemainingTime)

  def _VerifyMasterId(self, master_id):
    """Verify that our master id is current and valid.

    Args:
      master_id: Our master buildbucket id.
    """
    if self.buildstore.AreClientsReady() and master_id:
      assert not self._run.options.force_version
      master_build_status = self.buildstore.GetBuildStatuses(
          buildbucket_ids=[master_id])[0]
      latest = self.buildstore.GetBuildHistory(
          master_build_status['build_config'],
          1,
          branch=self._run.options.branch)
      if latest and str(latest[0]['buildbucket_id']) != str(master_id):
        raise failures_lib.MasterSlaveVersionMismatchFailure(
            "This slave's master (id=%s) has been supplanted by a newer "
            'master (id=%s). Aborting.' % (master_id, latest[0]['id']))

  @failures_lib.SetFailureType(failures_lib.InfrastructureFailure)
  def PerformStage(self):
    self.Initialize()

    self._VerifyMasterId(self._run.options.master_buildbucket_id)
    version = self._run.options.force_version
    if self._run.options.master_buildbucket_id:
      version = self._GetMasterVersion(self._run.options.master_buildbucket_id)

    next_manifest = None
    if version:
      next_manifest = self.ForceVersion(version)
    else:
      self.skip_sync = True
      next_manifest = self.GetNextManifest()

    if not next_manifest:
      logging.info('Found no work to do.')
      if self._run.attrs.manifest_manager.DidLastBuildFail():
        raise failures_lib.StepFailure('The previous build failed.')
      else:
        raise failures_lib.ExitEarlyException(
            'ManifestVersionedSyncStage finished and exited early.')

    # Log this early on for the release team to grep out before we finish.
    if self.manifest_manager:
      self._Print(
          '\nRELEASETAG: %s\n' % (self.manifest_manager.current_version))

    self._SetAndroidVersionIfApplicable(next_manifest)
    self._SetChromeVersionIfApplicable(next_manifest)
    # To keep local trybots working, remove restricted checkouts from the
    # official manifest we get from manifest-versions.
    with self.LocalizeManifest(
        next_manifest, filter_cros=self._run.options.local) as new_manifest:
      self.ManifestCheckout(new_manifest)


class MasterSlaveLKGMSyncStage(ManifestVersionedSyncStage):
  """Stage that generates a unique manifest file candidate, and sync's to it.

  This stage uses an LKGM manifest manager that handles LKGM
  candidates and their states.
  """
  # If we are using an internal manifest, but need to be able to create an
  # external manifest, we create a second manager for that manifest.
  external_manager = None
  MAX_BUILD_HISTORY_LENGTH = 10
  MilestoneVersion = collections.namedtuple('MilestoneVersion',
                                            ['milestone', 'platform'])
  category = constants.CI_INFRA_STAGE

  def __init__(self, builder_run, buildstore, **kwargs):
    super(MasterSlaveLKGMSyncStage, self).__init__(builder_run, buildstore,
                                                   **kwargs)
    # lkgm_manager deals with making sure we're synced to whatever manifest
    # we get back in GetNextManifest so syncing again is redundant.
    self._android_version = None
    self._chrome_version = None

  def _GetInitializedManager(self, internal):
    """Returns an initialized lkgm manager.

    Args:
      internal: Boolean.  True if this is using an internal manifest.

    Returns:
      lkgm_manager.LKGMManager.
    """
    increment = self.VersionIncrementType()
    return lkgm_manager.LKGMManager(
        source_repo=self.repo,
        manifest_repo=self._GetManifestVersionsRepoUrl(internal=internal),
        manifest=self._run.config.manifest,
        build_names=self._run.GetBuilderIds(),
        build_type=self._run.config.build_type,
        incr_type=increment,
        force=self._force,
        branch=self._run.manifest_branch,
        dry_run=self._run.options.debug,
        config=self._run.config,
        metadata=self._run.attrs.metadata,
        buildstore=self.buildstore,
        buildbucket_client=self.buildbucket_client)

  def Initialize(self):
    """Override: Creates an LKGMManager rather than a ManifestManager."""
    self._InitializeRepo()
    self.RegisterManifestManager(self._GetInitializedManager(self.internal))
    if self._run.config.master and self._GetSlaveConfigs():
      assert self.internal, 'Unified masters must use an internal checkout.'
      MasterSlaveLKGMSyncStage.external_manager = \
          self._GetInitializedManager(False)

  def ForceVersion(self, version):
    manifest = super(MasterSlaveLKGMSyncStage, self).ForceVersion(version)
    if MasterSlaveLKGMSyncStage.external_manager:
      MasterSlaveLKGMSyncStage.external_manager.BootstrapFromVersion(version)

    return manifest

  def _VerifyMasterId(self, master_id):
    """Verify that our master id is current and valid."""
    super(MasterSlaveLKGMSyncStage, self)._VerifyMasterId(master_id)
    if not self._run.config.master and not master_id:
      raise failures_lib.StepFailure(
          'Cannot start build without a master_build_id. Did you hit force '
          'build on a slave? Please hit force build on the master instead.')

  def GetNextManifest(self):
    """Gets the next manifest using LKGM logic."""
    assert self.manifest_manager, \
        'Must run Initialize before we can get a manifest.'
    assert isinstance(self.manifest_manager, lkgm_manager.LKGMManager), \
        'Manifest manager instantiated with wrong class.'
    assert self._run.config.master

    build_id = self._run.attrs.metadata.GetDict().get('build_id')
    logging.info(
        'Creating new candidate manifest, including chrome version '
        '%s.', self._chrome_version)
    if self._android_version:
      logging.info('Adding Android version to new candidate manifest %s.',
                   self._android_version)
    manifest = self.manifest_manager.CreateNewCandidate(
        android_version=self._android_version,
        chrome_version=self._chrome_version,
        build_id=build_id)
    if MasterSlaveLKGMSyncStage.external_manager:
      MasterSlaveLKGMSyncStage.external_manager.CreateFromManifest(
          manifest, build_id=build_id)

    return manifest

  def GetLatestAndroidVersion(self):
    """Returns the version of Android to uprev."""
    return cros_mark_android_as_stable.GetLatestBuild(
        constants.ANDROID_BUCKET_URL, self._run.config.android_import_branch,
        cros_mark_android_as_stable.MakeBuildTargetDict(
            self._run.config.android_package,
            self._run.config.android_import_branch))[0]

  def GetLatestChromeVersion(self):
    """Returns the version of Chrome to uprev."""
    return cros_mark_chrome_as_stable.GetLatestRelease(
        constants.CHROMIUM_GOB_URL)

  def GetLastChromeOSVersion(self):
    """Fetching ChromeOS version from the last run.

    Fetching the chromeos version from the last run that published a manifest
    by querying CIDB. Master builds that failed before publishing a manifest
    will be ignored.

    Returns:
      A namedtuple MilestoneVersion,
      e.g. MilestoneVersion(milestone='44', platform='7072.0.0-rc4')
      or None if failed to retrieve milestone and platform versions.
    """
    build_identifier, _ = self._run.GetCIDBHandle()
    buildbucket_id = build_identifier.buildbucket_id

    if not self.buildstore.AreClientsReady():
      return None

    builds = self.buildstore.GetBuildHistory(
        build_config=self._run.config.name,
        num_results=self.MAX_BUILD_HISTORY_LENGTH,
        ignore_build_id=buildbucket_id)
    full_versions = [b.get('full_version') for b in builds]
    old_versions = [x for x in full_versions if x]
    if old_versions:
      old_version = old_versions[0]
      pattern = r'^R(\d+)-(\d+.\d+.\d+(-rc\d+)*)'
      m = re.match(pattern, old_version)
      if m:
        milestone = m.group(1)
        platform = m.group(2)
      return self.MilestoneVersion(milestone=milestone, platform=platform)
    return None

  @failures_lib.SetFailureType(failures_lib.InfrastructureFailure)
  def PerformStage(self):
    """Performs the stage."""
    if self._android_rev and self._run.config.master:
      self._android_version = self.GetLatestAndroidVersion()
      logging.info('Latest Android version is: %s', self._android_version)

    if (self._chrome_rev == constants.CHROME_REV_LATEST and
        self._run.config.master):
      # PFQ master needs to determine what version of Chrome to build
      # for all slaves.
      logging.info('I am a master running with CHROME_REV_LATEST, '
                   'therefore getting latest chrome version.')
      self._chrome_version = self.GetLatestChromeVersion()
      logging.info('Latest chrome version is: %s', self._chrome_version)

    ManifestVersionedSyncStage.PerformStage(self)

    # Generate blamelist
    cros_version = self.GetLastChromeOSVersion()
    if cros_version:
      old_filename = self.manifest_manager.GetBuildSpecFilePath(
          cros_version.milestone, cros_version.platform)
      if not os.path.exists(old_filename):
        logging.error(
            'Could not generate blamelist, '
            'manifest file does not exist: %s', old_filename)
      else:
        logging.debug('Generate blamelist against: %s', old_filename)
        lkgm_manager.GenerateBlameList(self.repo, old_filename)
