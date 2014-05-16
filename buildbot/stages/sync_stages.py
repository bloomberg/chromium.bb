# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing the sync stages."""

import contextlib
import datetime
import logging
import os
import sys
from xml.etree import ElementTree

from chromite.buildbot import cbuildbot_config
from chromite.buildbot import cbuildbot_failures as failures_lib
from chromite.buildbot import constants
from chromite.buildbot import lkgm_manager
from chromite.buildbot import manifest_version
from chromite.buildbot import repository
from chromite.buildbot import trybot_patch_pool
from chromite.buildbot import validation_pool
from chromite.buildbot.stages import generic_stages
from chromite.buildbot.stages import build_stages
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import git
from chromite.lib import osutils
from chromite.lib import patch as cros_patch
from chromite.lib import timeout_util


PRE_CQ = validation_pool.PRE_CQ


class PatchChangesStage(generic_stages.BuilderStage):
  """Stage that patches a set of Gerrit changes to the buildroot source tree."""

  def __init__(self, builder_run, patch_pool, **kwargs):
    """Construct a PatchChangesStage.

    Args:
      builder_run: BuilderRun object.
      patch_pool: A TrybotPatchPool object containing the different types of
                  patches to apply.
    """
    super(PatchChangesStage, self).__init__(builder_run, **kwargs)
    self.patch_pool = patch_pool

  @staticmethod
  def _CheckForDuplicatePatches(_series, changes):
    conflicts = {}
    duplicates = []
    for change in changes:
      if change.id is None:
        cros_build_lib.Warning(
            "Change %s lacks a usable ChangeId; duplicate checking cannot "
            "be done for this change.  If cherry-picking fails, this is a "
            "potential cause.", change)
        continue
      conflicts.setdefault(change.id, []).append(change)

    duplicates = [x for x in conflicts.itervalues() if len(x) > 1]
    if not duplicates:
      return changes

    for conflict in duplicates:
      cros_build_lib.Error(
          "Changes %s conflict with each other- they have same id %s.",
          ', '.join(map(str, conflict)), conflict[0].id)

    cros_build_lib.Die("Duplicate patches were encountered: %s", duplicates)

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
    cros_build_lib.Die("Failed applying patches: %s",
                       "\n".join(map(str, failures)))

  def PerformStage(self):
    class NoisyPatchSeries(validation_pool.PatchSeries):
      """Custom PatchSeries that adds links to buildbot logs for remote trys."""

      def ApplyChange(self, change, dryrun=False):
        if isinstance(change, cros_patch.GerritPatch):
          cros_build_lib.PrintBuildbotLink(str(change), change.url)
        elif isinstance(change, cros_patch.UploadedLocalPatch):
          cros_build_lib.PrintBuildbotStepText(str(change))

        return validation_pool.PatchSeries.ApplyChange(self, change,
                                                       dryrun=dryrun)

    # If we're an external builder, ignore internal patches.
    helper_pool = validation_pool.HelperPool.SimpleCreate(
        cros_internal=self._run.config.internal, cros=True)

    # Limit our resolution to non-manifest patches.
    patch_series = NoisyPatchSeries(
        self._build_root,
        force_content_merging=True,
        helper_pool=helper_pool,
        deps_filter_fn=lambda p: not trybot_patch_pool.ManifestFilter(p))

    self._ApplyPatchSeries(patch_series, self.patch_pool)


class BootstrapStage(PatchChangesStage):
  """Stage that patches a chromite repo and re-executes inside it.

  Attributes:
    returncode - the returncode of the cbuildbot re-execution.  Valid after
                 calling stage.Run().
  """
  option_name = 'bootstrap'

  def __init__(self, builder_run, chromite_patch_pool,
               manifest_patch_pool=None, **kwargs):
    super(BootstrapStage, self).__init__(
        builder_run, trybot_patch_pool.TrybotPatchPool(), **kwargs)
    self.chromite_patch_pool = chromite_patch_pool
    self.manifest_patch_pool = manifest_patch_pool
    self.returncode = None

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
    repository.CloneGitRepo(checkout_dir,
                            self._run.config.manifest_repo_url)

    patch_series = validation_pool.PatchSeries.WorkOnSingleRepo(
        checkout_dir, tracking_branch=self._run.manifest_branch)

    self._ApplyPatchSeries(patch_series, patch_pool)
    # Create the branch that 'repo init -b <target_branch> -u <patched_repo>'
    # will look for.
    cmd = ['branch', '-f', self._run.manifest_branch,
           constants.PATCH_BRANCH]
    git.RunGit(checkout_dir, cmd)

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
      cros_build_lib.Warning('The following arguments were removed due to api: '
                             "'%s'" % ' '.join(removed))
    return accepted

  @classmethod
  def FilterArgsForTargetCbuildbot(cls, buildroot, cbuildbot_path, options):
    _, minor = cros_build_lib.GetTargetChromiteApiVersion(buildroot)
    args = [cbuildbot_path]
    args.extend(options.build_targets)
    args.extend(cls._FilterArgsForApi(options.parsed_args, minor))

    # Only pass down --cache-dir if it was specified. By default, we want
    # the cache dir to live in the root of each checkout, so this means that
    # each instance of cbuildbot needs to calculate the default separately.
    if minor >= 2 and options.cache_dir_specified:
      args += ['--cache-dir', options.cache_dir]

    return args

  def HandleApplyFailures(self, failures):
    """Handle the case where patches fail to apply."""
    if self._run.options.pre_cq or self._run.config.pre_cq:
      # Let the PreCQSync stage handle this failure. The PreCQSync stage will
      # comment on CLs with the appropriate message when they fail to apply.
      #
      # WARNING: For manifest patches, the Pre-CQ attempts to apply external
      # patches to the internal manifest, and this means we may flag a conflict
      # here even if the patch applies cleanly. TODO(davidjames): Fix this.
      cros_build_lib.PrintBuildbotStepWarnings()
      cros_build_lib.Error('Failed applying patches: %s',
                           '\n'.join(map(str, failures)))
    else:
      PatchChangesStage.HandleApplyFailures(self, failures)

  #pylint: disable=E1101
  @osutils.TempDirDecorator
  def PerformStage(self):
    # The plan for the builders is to use master branch to bootstrap other
    # branches. Now, if we wanted to test patches for both the bootstrap code
    # (on master) and the branched chromite (say, R20), we need to filter the
    # patches by branch.
    filter_branch = self._run.manifest_branch
    if self._run.options.test_bootstrap:
      filter_branch = 'master'

    chromite_dir = os.path.join(self.tempdir, 'chromite')
    reference_repo = os.path.join(constants.SOURCE_ROOT, 'chromite', '.git')
    repository.CloneGitRepo(chromite_dir, constants.CHROMITE_URL,
                            reference=reference_repo)
    git.RunGit(chromite_dir, ['checkout', filter_branch])

    def BranchAndChromiteFilter(patch):
      return (trybot_patch_pool.BranchFilter(filter_branch, patch) and
              trybot_patch_pool.ChromiteFilter(patch))

    patch_series = validation_pool.PatchSeries.WorkOnSingleRepo(
        chromite_dir, filter_branch,
        deps_filter_fn=BranchAndChromiteFilter)

    filtered_pool = self.chromite_patch_pool.FilterBranch(filter_branch)
    if filtered_pool:
      self._ApplyPatchSeries(patch_series, filtered_pool)

    cbuildbot_path = constants.PATH_TO_CBUILDBOT
    if not os.path.exists(os.path.join(self.tempdir, cbuildbot_path)):
      cbuildbot_path = 'chromite/buildbot/cbuildbot'
    # pylint: disable=W0212
    cmd = self.FilterArgsForTargetCbuildbot(self.tempdir, cbuildbot_path,
                                            self._run.options)

    extra_params = ['--sourceroot=%s' % self._run.options.sourceroot]
    extra_params.extend(self._run.options.bootstrap_args)
    if self._run.options.test_bootstrap:
      # We don't want re-executed instance to see this.
      cmd = [a for a in cmd if a != '--test-bootstrap']
    else:
      # If we've already done the desired number of bootstraps, disable
      # bootstrapping for the next execution.  Also pass in the patched manifest
      # repository.
      extra_params.append('--nobootstrap')
      if self.manifest_patch_pool:
        manifest_dir = self._ApplyManifestPatches(self.manifest_patch_pool)
        extra_params.extend(['--manifest-repo-url', manifest_dir])

    cmd += extra_params
    result_obj = cros_build_lib.RunCommand(
        cmd, cwd=self.tempdir, kill_timeout=30, error_code_ok=True)
    self.returncode = result_obj.returncode


class SyncStage(generic_stages.BuilderStage):
  """Stage that performs syncing for the builder."""

  option_name = 'sync'
  output_manifest_sha1 = True

  def __init__(self, builder_run, **kwargs):
    super(SyncStage, self).__init__(builder_run, **kwargs)
    self.repo = None
    self.skip_sync = False

    # TODO(mtennant): Why keep a duplicate copy of this config value
    # at self.internal when it can always be retrieved from config?
    self.internal = self._run.config.internal

  def _GetManifestVersionsRepoUrl(self, read_only=False):
    return cbuildbot_config.GetManifestVersionsRepoUrl(
        self.internal,
        read_only=read_only)

  def Initialize(self):
    self._InitializeRepo()

  def _InitializeRepo(self):
    """Set up the RepoRepository object."""
    self.repo = self.GetRepoRepository()

  def GetNextManifest(self):
    """Returns the manifest to use."""
    return self._run.config.manifest

  def ManifestCheckout(self, next_manifest):
    """Checks out the repository to the given manifest."""
    self._Print('\n'.join(['BUILDROOT: %s' % self.repo.directory,
                           'TRACKING BRANCH: %s' % self.repo.branch,
                           'NEXT MANIFEST: %s' % next_manifest]))

    if not self.skip_sync:
      self.repo.Sync(next_manifest)

    print >> sys.stderr, self.repo.ExportManifest(
        mark_revision=self.output_manifest_sha1)

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
          cros_build_lib.Warning(str(e))
        else:
          osutils.WriteFile(old_filename, old_contents)
          fresh_sync = False

      # Sync.
      self.ManifestCheckout(self.GetNextManifest())

      # Print the blamelist.
      if fresh_sync:
        cros_build_lib.PrintBuildbotStepText('(From scratch)')
      elif self._run.options.buildbot:
        lkgm_manager.GenerateBlameList(self.repo, old_filename)


class LKGMSyncStage(SyncStage):
  """Stage that syncs to the last known good manifest blessed by builders."""

  output_manifest_sha1 = False

  def GetNextManifest(self):
    """Override: Gets the LKGM."""
    # TODO(sosa):  Should really use an initialized manager here.
    if self.internal:
      mv_dir = 'manifest-versions-internal'
    else:
      mv_dir = 'manifest-versions'

    manifest_path = os.path.join(self._build_root, mv_dir)
    manifest_repo = self._GetManifestVersionsRepoUrl(read_only=True)
    manifest_version.RefreshManifestCheckout(manifest_path, manifest_repo)
    return os.path.join(manifest_path, lkgm_manager.LKGMManager.LKGM_PATH)


class ManifestVersionedSyncStage(SyncStage):
  """Stage that generates a unique manifest file, and sync's to it."""

  # TODO(mtennant): Make this into a builder run value.
  output_manifest_sha1 = False

  def __init__(self, builder_run, **kwargs):
    # Perform the sync at the end of the stage to the given manifest.
    super(ManifestVersionedSyncStage, self).__init__(builder_run, **kwargs)
    self.repo = None
    self.manifest_manager = None

    # If a builder pushes changes (even with dryrun mode), we need a writable
    # repository. Otherwise, the push will be rejected by the server.
    self.manifest_repo = self._GetManifestVersionsRepoUrl(read_only=False)

    # 1. If we're uprevving Chrome, Chrome might have changed even if the
    #    manifest has not, so we should force a build to double check. This
    #    means that we'll create a new manifest, even if there are no changes.
    # 2. If we're running with --debug, we should always run through to
    #    completion, so as to ensure a complete test.
    self._force = self._chrome_rev or self._run.options.debug

  def HandleSkip(self):
    """Initializes a manifest manager to the specified version if skipped."""
    super(ManifestVersionedSyncStage, self).HandleSkip()
    if self._run.options.force_version:
      self.Initialize()
      self.ForceVersion(self._run.options.force_version)

  def ForceVersion(self, version):
    """Creates a manifest manager from given version and returns manifest."""
    cros_build_lib.PrintBuildbotStepText(version)
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

    self.RegisterManifestManager(manifest_version.BuildSpecsManager(
        source_repo=self.repo,
        manifest_repo=self.manifest_repo,
        manifest=self._run.config.manifest,
        build_names=self._run.GetBuilderIds(),
        incr_type=self.VersionIncrementType(),
        force=self._force,
        branch=self._run.manifest_branch,
        dry_run=dry_run,
        master=self._run.config.master))

  def GetNextManifest(self):
    """Uses the initialized manifest manager to get the next manifest."""
    assert self.manifest_manager, \
        'Must run GetStageManager before checkout out build.'

    to_return = self.manifest_manager.GetNextBuildSpec(
        dashboard_url=self.ConstructDashboardURL())
    previous_version = self.manifest_manager.GetLatestPassingSpec()
    target_version = self.manifest_manager.current_version

    # Print the Blamelist here.
    url_prefix = 'http://chromeos-images.corp.google.com/diff/report?'
    url = url_prefix + 'from=%s&to=%s' % (previous_version, target_version)
    cros_build_lib.PrintBuildbotLink('Blamelist', url)
    # The testManifestVersionedSyncOnePartBranch interacts badly with this
    # function.  It doesn't fully initialize self.manifest_manager which
    # causes target_version to be None.  Since there isn't a clean fix in
    # either direction, just throw this through str().  In the normal case,
    # it's already a string anyways.
    cros_build_lib.PrintBuildbotStepText(str(target_version))

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
          if remote and remote not in constants.GIT_REMOTES:
            root.remove(node)
        doc.write(filtered_manifest)
        yield filtered_manifest
    else:
      yield manifest

  def PerformStage(self):
    self.Initialize()
    if self._run.options.force_version:
      next_manifest = self.ForceVersion(self._run.options.force_version)
    else:
      next_manifest = self.GetNextManifest()

    if not next_manifest:
      cros_build_lib.Info('Found no work to do.')
      if self._run.attrs.manifest_manager.DidLastBuildFail():
        raise failures_lib.StepFailure('The previous build failed.')
      else:
        sys.exit(0)

    # Log this early on for the release team to grep out before we finish.
    if self.manifest_manager:
      self._Print('\nRELEASETAG: %s\n' % (
          self.manifest_manager.current_version))

    # To keep local trybots working, remove restricted checkouts from the
    # official manifest we get from manifest-versions.
    with self.LocalizeManifest(
        next_manifest, filter_cros=self._run.options.local) as new_manifest:
      self.ManifestCheckout(new_manifest)


class MasterSlaveSyncStage(ManifestVersionedSyncStage):
  """Stage that generates a unique manifest file candidate, and sync's to it."""

  # TODO(mtennant): Turn this into self._run.attrs.sub_manager or similar.
  # An instance of lkgm_manager.LKGMManager for slave builds.
  sub_manager = None

  def __init__(self, builder_run, **kwargs):
    super(MasterSlaveSyncStage, self).__init__(builder_run, **kwargs)
    # lkgm_manager deals with making sure we're synced to whatever manifest
    # we get back in GetNextManifest so syncing again is redundant.
    self.skip_sync = True

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
        manifest_repo=cbuildbot_config.GetManifestVersionsRepoUrl(
            internal, read_only=False),
        manifest=self._run.config.manifest,
        build_names=self._run.GetBuilderIds(),
        build_type=self._run.config.build_type,
        incr_type=increment,
        force=self._force,
        branch=self._run.manifest_branch,
        dry_run=self._run.options.debug,
        master=self._run.config.master)

  def Initialize(self):
    """Override: Creates an LKGMManager rather than a ManifestManager."""
    self._InitializeRepo()
    self.RegisterManifestManager(self._GetInitializedManager(self.internal))
    if (self._run.config.master and self._GetSlaveConfigs()):
      assert self.internal, 'Unified masters must use an internal checkout.'
      MasterSlaveSyncStage.sub_manager = self._GetInitializedManager(False)

  def ForceVersion(self, version):
    manifest = super(MasterSlaveSyncStage, self).ForceVersion(version)
    if MasterSlaveSyncStage.sub_manager:
      MasterSlaveSyncStage.sub_manager.BootstrapFromVersion(version)

    return manifest

  def GetNextManifest(self):
    """Gets the next manifest using LKGM logic."""
    assert self.manifest_manager, \
        'Must run Initialize before we can get a manifest.'
    assert isinstance(self.manifest_manager, lkgm_manager.LKGMManager), \
        'Manifest manager instantiated with wrong class.'

    if self._run.config.master:
      manifest = self.manifest_manager.CreateNewCandidate()
      if MasterSlaveSyncStage.sub_manager:
        MasterSlaveSyncStage.sub_manager.CreateFromManifest(
            manifest, dashboard_url=self.ConstructDashboardURL())
      return manifest
    else:
      return self.manifest_manager.GetLatestCandidate(
          dashboard_url=self.ConstructDashboardURL())


class CommitQueueSyncStage(MasterSlaveSyncStage):
  """Commit Queue Sync stage that handles syncing and applying patches.

  This stage handles syncing to a manifest, passing around that manifest to
  other builders and finding the Gerrit Reviews ready to be committed and
  applying them into its own checkout.
  """

  def __init__(self, builder_run, **kwargs):
    super(CommitQueueSyncStage, self).__init__(builder_run, **kwargs)
    # Figure out the builder's name from the buildbot waterfall.
    builder_name = self._run.config.paladin_builder_name
    self.builder_name = builder_name if builder_name else self._run.config.name

    # The pool of patches to be picked up by the commit queue.
    # - For the master commit queue, it's initialized in GetNextManifest.
    # - For slave commit queues, it's initialized in _SetPoolFromManifest.
    #
    # In all cases, the pool is saved to disk, and refreshed after bootstrapping
    # by HandleSkip.
    self.pool = None

  def HandleSkip(self):
    """Handles skip and initializes validation pool from manifest."""
    super(CommitQueueSyncStage, self).HandleSkip()
    filename = self._run.options.validation_pool
    if filename:
      self.pool = validation_pool.ValidationPool.Load(filename,
          metadata=self._run.attrs.metadata)
    else:
      self._SetPoolFromManifest(self.manifest_manager.GetLocalManifest())

  def _ChangeFilter(self, pool, changes, non_manifest_changes):
    # First, look for changes that were tested by the Pre-CQ.
    changes_to_test = []
    for change in changes:
      status = pool.GetCLStatus(PRE_CQ, change)
      if status == manifest_version.BuilderStatus.STATUS_PASSED:
        changes_to_test.append(change)

    # If we only see changes that weren't verified by Pre-CQ, try all of the
    # changes. This ensures that the CQ continues to work even if the Pre-CQ is
    # down.
    if not changes_to_test:
      changes_to_test = changes

    return changes_to_test, non_manifest_changes

  def _SetPoolFromManifest(self, manifest):
    """Sets validation pool based on manifest path passed in."""
    # Note that GetNextManifest() calls GetLatestCandidate() in this case,
    # so the repo will already be sync'd appropriately. This means that
    # AcquirePoolFromManifest does not need to sync.
    self.pool = validation_pool.ValidationPool.AcquirePoolFromManifest(
        manifest, self._run.config.overlays, self.repo,
        self._run.buildnumber, self.builder_name,
        self._run.config.master, self._run.options.debug,
        metadata=self._run.attrs.metadata)

  def GetNextManifest(self):
    """Gets the next manifest using LKGM logic."""
    assert self.manifest_manager, \
        'Must run Initialize before we can get a manifest.'
    assert isinstance(self.manifest_manager, lkgm_manager.LKGMManager), \
        'Manifest manager instantiated with wrong class.'

    if self._run.config.master:
      try:
        # In order to acquire a pool, we need an initialized buildroot.
        if not git.FindRepoDir(self.repo.directory):
          self.repo.Initialize()
        self.pool = pool = validation_pool.ValidationPool.AcquirePool(
            self._run.config.overlays, self.repo,
            self._run.buildnumber, self.builder_name,
            self._run.options.debug,
            check_tree_open=not self._run.options.debug or
                            self._run.options.mock_tree_status,
            changes_query=self._run.options.cq_gerrit_override,
            change_filter=self._ChangeFilter, throttled_ok=True,
            metadata=self._run.attrs.metadata)

      except validation_pool.TreeIsClosedException as e:
        cros_build_lib.Warning(str(e))
        return None

      manifest = self.manifest_manager.CreateNewCandidate(validation_pool=pool)
      if MasterSlaveSyncStage.sub_manager:
        MasterSlaveSyncStage.sub_manager.CreateFromManifest(
            manifest, dashboard_url=self.ConstructDashboardURL())

      return manifest
    else:
      manifest = self.manifest_manager.GetLatestCandidate(
          dashboard_url=self.ConstructDashboardURL())
      if manifest:
        if self._run.config.build_before_patching:
          pre_build_passed = self._RunPrePatchBuild()
          self._RenameStep('CommitQueueSync : Apply Patches')
          if not pre_build_passed:
            cros_build_lib.PrintBuildbotStepText('Pre-patch build failed.')

        self._SetPoolFromManifest(manifest)
        self.pool.ApplyPoolIntoRepo()

      return manifest

  def _RunPrePatchBuild(self):
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
          self._run, chroot_replace=True, suffix=suffix).Run()
      for builder_run in self._run.GetUngroupedBuilderRuns():
        for board in builder_run.config.boards:
          build_stages.SetupBoardStage(
              builder_run, board=board, suffix=suffix).Run()
          build_stages.BuildPackagesStage(
              builder_run, board=board, suffix=suffix).Run()
    except failures_lib.StepFailure:
      return False

    return True

  def PerformStage(self):
    """Performs normal stage and prints blamelist at end."""
    if self._run.options.force_version:
      self.HandleSkip()
    else:
      ManifestVersionedSyncStage.PerformStage(self)


class PreCQSyncStage(SyncStage):
  """Sync and apply patches to test if they compile."""

  def __init__(self, builder_run, patches, **kwargs):
    super(PreCQSyncStage, self).__init__(builder_run, **kwargs)

    # The list of patches to test.
    self.patches = patches

    # The ValidationPool of patches to test. Initialized in PerformStage, and
    # refreshed after bootstrapping by HandleSkip.
    self.pool = None

  def HandleSkip(self):
    """Handles skip and loads validation pool from disk."""
    super(PreCQSyncStage, self).HandleSkip()
    filename = self._run.options.validation_pool
    if filename:
      self.pool = validation_pool.ValidationPool.Load(filename,
          metadata=self._run.attrs.metadata)

  def PerformStage(self):
    super(PreCQSyncStage, self).PerformStage()
    self.pool = validation_pool.ValidationPool.AcquirePreCQPool(
        self._run.config.overlays, self._build_root,
        self._run.buildnumber, self._run.config.name,
        dryrun=self._run.options.debug_forced, changes=self.patches,
        metadata=self._run.attrs.metadata)
    self.pool.ApplyPoolIntoRepo()


class PreCQLauncherStage(SyncStage):
  """Scans for CLs and automatically launches Pre-CQ jobs to test them."""

  STATUS_INFLIGHT = validation_pool.ValidationPool.STATUS_INFLIGHT
  STATUS_PASSED = validation_pool.ValidationPool.STATUS_PASSED
  STATUS_FAILED = validation_pool.ValidationPool.STATUS_FAILED
  STATUS_LAUNCHING = validation_pool.ValidationPool.STATUS_LAUNCHING
  STATUS_WAITING = validation_pool.ValidationPool.STATUS_WAITING

  # The number of minutes we allow before considering a launch attempt failed.
  # If this window isn't hit in a given launcher run, the window will start
  # again from scratch in the next run.
  LAUNCH_DELAY = 30

  # The number of minutes we allow before considering an in-flight
  # job failed. If this window isn't hit in a given launcher run, the window
  # will start again from scratch in the next run.
  INFLIGHT_DELAY = 90

  # The maximum number of patches we will allow in a given trybot run. This is
  # needed because our trybot infrastructure can only handle so many patches at
  # once.
  MAX_PATCHES_PER_TRYBOT_RUN = 50

  def __init__(self, builder_run, **kwargs):
    super(PreCQLauncherStage, self).__init__(builder_run, **kwargs)
    self.skip_sync = True
    # Mapping from launching changes to the first known time when they
    # were launching.
    self.launching = {}
    # Mapping from inflight changes to the first known time when they
    # were inflight.
    self.inflight = {}
    self.retried = set()

  def _HasLaunchTimedOut(self, change):
    """Check whether a given |change| has timed out on its trybot launch.

    Assumes that the change is in the middle of being launched.

    Returns:
      True if the change has timed out. False otherwise.
    """
    diff = datetime.timedelta(minutes=self.LAUNCH_DELAY)
    return datetime.datetime.now() - self.launching[change] > diff

  def _HasInflightTimedOut(self, change):
    """Check whether a given |change| has timed out while trybot inflight.

    Assumes that the change's trybot is inflight.

    Returns:
      True if the change has timed out. False otherwise.
    """
    diff = datetime.timedelta(minutes=self.INFLIGHT_DELAY)
    return datetime.datetime.now() - self.inflight[change] > diff

  def GetPreCQStatus(self, pool, changes):
    """Get the Pre-CQ status of a list of changes.

    Side effect: reject or retry changes that have timed out.

    Args:
      pool: The validation pool.
      changes: Changes to examine.

    Returns:
      busy: The set of CLs that are currently being tested.
      passed: The set of CLs that have been verified.
    """
    busy, passed = set(), set()

    for change in changes:
      status = pool.GetCLStatus(PRE_CQ, change)

      if status != self.STATUS_LAUNCHING:
        # The trybot is not launching, so we should remove it from our
        # launching timeout map.
        self.launching.pop(change, None)

      if status != self.STATUS_INFLIGHT:
        # The trybot is not inflight, so we should remove it from our
        # inflight timeout map.
        self.inflight.pop(change, None)

      if status == self.STATUS_LAUNCHING:
        # The trybot is in the process of launching.
        busy.add(change)
        if change not in self.launching:
          # Record the launch time of changes.
          self.launching[change] = datetime.datetime.now()
        elif self._HasLaunchTimedOut(change):
          if change in self.retried:
            msg = ('We were not able to launch a pre-cq trybot for your change.'
                   '\n\n'
                   'This problem can happen if the trybot waterfall is very '
                   'busy, or if there is an infrastructure issue. Please '
                   'notify the sheriff and mark your change as ready again. If '
                   'this problem occurs multiple times in a row, please file a '
                   'bug.')

            pool.SendNotification(change, '%(details)s', details=msg)
            pool.RemoveCommitReady(change)
            pool.UpdateCLStatus(PRE_CQ, change, self.STATUS_FAILED,
                                self._run.options.debug)
            self.retried.discard(change)
          else:
            # Try the change again.
            self.retried.add(change)
            pool.UpdateCLStatus(PRE_CQ, change, self.STATUS_WAITING,
                                self._run.options.debug)
      elif status == self.STATUS_INFLIGHT:
        # Once a Pre-CQ run actually starts, it'll set the status to
        # STATUS_INFLIGHT.
        busy.add(change)
        if change not in self.inflight:
          # Record the inflight start time.
          self.inflight[change] = datetime.datetime.now()
        elif self._HasInflightTimedOut(change):
          msg = ('The pre-cq trybot for your change timed out after %s minutes.'
                 '\n\n'
                 'This problem can happen if your change causes the builder '
                 'to hang, or if there is some infrastructure issue. If your '
                 'change is not at fault you may mark your change as ready '
                 'again. If this problem occurs multiple times please notify '
                 'the sheriff and file a bug.' % self.INFLIGHT_DELAY)

          pool.SendNotification(change, '%(details)s', details=msg)
          pool.RemoveCommitReady(change)
          pool.UpdateCLStatus(PRE_CQ, change, self.STATUS_FAILED,
                              self._run.options.debug)
      elif status == self.STATUS_FAILED:
        # The Pre-CQ run failed for this change. It's possible that we got
        # unlucky and this change was just marked as 'Not Ready' by a bot. To
        # test this, mark the CL as 'waiting' for now. If the CL is still marked
        # as 'Ready' next time we check, we'll know the CL is truly still ready.
        busy.add(change)
        pool.UpdateCLStatus(PRE_CQ, change, self.STATUS_WAITING,
                            self._run.options.debug)
      elif status == self.STATUS_PASSED:
        passed.add(change)

    return busy, passed

  def LaunchTrybot(self, pool, plan):
    """Launch a Pre-CQ run with the provided list of CLs.

    Args:
      pool: ValidationPool corresponding to |plan|.
      plan: The list of patches to test in the Pre-CQ run.
    """
    cmd = ['cbuildbot', '--remote', constants.PRE_CQ_BUILDER_NAME]
    if self._run.options.debug:
      cmd.append('--debug')
    for patch in plan:
      cmd += ['-g', cros_patch.AddPrefix(patch, patch.gerrit_number)]
    cros_build_lib.RunCommand(cmd, cwd=self._build_root)
    for patch in plan:
      if pool.GetCLStatus(PRE_CQ, patch) != self.STATUS_PASSED:
        pool.UpdateCLStatus(PRE_CQ, patch, self.STATUS_LAUNCHING,
                            self._run.options.debug)

  def GetDisjointTransactionsToTest(self, pool, changes):
    """Get the list of disjoint transactions to test.

    Side effect: reject or retry changes that have timed out.

    Returns:
      A list of disjoint transactions to test. Each transaction should be sent
      to a different Pre-CQ trybot.
    """
    busy, passed = self.GetPreCQStatus(pool, changes)

    # Create a list of disjoint transactions to test.
    manifest = git.ManifestCheckout.Cached(self._build_root)
    plans = pool.CreateDisjointTransactions(
        manifest, max_txn_length=self.MAX_PATCHES_PER_TRYBOT_RUN)
    for plan in plans:
      # If any of the CLs in the plan are currently "busy" being tested,
      # wait until they're done before launching our trybot run. This helps
      # avoid race conditions.
      #
      # Similarly, if all of the CLs in the plan have already been validated,
      # there's no need to launch a trybot run.
      plan = set(plan)
      if plan.issubset(passed):
        logging.info('CLs already verified: %r', ' '.join(map(str, plan)))
      elif plan.intersection(busy):
        logging.info('CLs currently being verified: %r',
                     ' '.join(map(str, plan.intersection(busy))))
        if plan.difference(busy):
          logging.info('CLs waiting on verification of dependencies: %r',
              ' '.join(map(str, plan.difference(busy))))
      else:
        yield plan

  def ProcessChanges(self, pool, changes, _non_manifest_changes):
    """Process a list of changes that were marked as Ready.

    From our list of changes that were marked as Ready, we create a
    list of disjoint transactions and send each one to a separate Pre-CQ
    trybot.

    Non-manifest changes are just submitted here because they don't need to be
    verified by either the Pre-CQ or CQ.
    """
    # Submit non-manifest changes if we can.
    if timeout_util.IsTreeOpen(
        validation_pool.ValidationPool.STATUS_URL):
      pool.SubmitNonManifestChanges(check_tree_open=False)

    # Launch trybots for manifest changes.
    for plan in self.GetDisjointTransactionsToTest(pool, changes):
      self.LaunchTrybot(pool, plan)

    # Tell ValidationPool to keep waiting for more changes until we hit
    # its internal timeout.
    return [], []

  def PerformStage(self):
    # Setup and initialize the repo.
    super(PreCQLauncherStage, self).PerformStage()

    # Loop through all of the changes until we hit a timeout.
    validation_pool.ValidationPool.AcquirePool(
        self._run.config.overlays, self.repo,
        self._run.buildnumber,
        constants.PRE_CQ_LAUNCHER_NAME,
        dryrun=self._run.options.debug,
        changes_query=self._run.options.cq_gerrit_override,
        check_tree_open=False, change_filter=self.ProcessChanges,
        metadata=self._run.attrs.metadata)
