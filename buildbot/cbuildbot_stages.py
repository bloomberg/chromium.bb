# Copyright (c) 2011-2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing the various stages that a builder runs."""

import cPickle
import datetime
import functools
import multiprocessing
import os
import Queue
import shutil
import sys
import tempfile

from chromite.buildbot import builderstage as bs
from chromite.buildbot import cbuildbot_background as background
from chromite.buildbot import cbuildbot_commands as commands
from chromite.buildbot import cbuildbot_config
from chromite.buildbot import configure_repo
from chromite.buildbot import constants
from chromite.buildbot import lkgm_manager
from chromite.buildbot import manifest_version
from chromite.buildbot import patch as cros_patch
from chromite.buildbot import portage_utilities
from chromite.buildbot import repository
from chromite.buildbot import validation_pool
from chromite.lib import cros_build_lib as cros_lib

_FULL_BINHOST = 'FULL_BINHOST'
_PORTAGE_BINHOST = 'PORTAGE_BINHOST'
_CROS_ARCHIVE_URL = 'CROS_ARCHIVE_URL'
_PRINT_INTERVAL = 1
BUILDBOT_ARCHIVE_PATH = '/b/archive'


class NonHaltingBuilderStage(bs.BuilderStage):
  """Build stage that fails a build but finishes the other steps."""
  def Run(self):
    try:
      super(NonHaltingBuilderStage, self).Run()
    except bs.NonBacktraceBuildException:
      pass


class ForgivingBuilderStage(NonHaltingBuilderStage):
  """Build stage that turns a build step red but not a build."""
  def _HandleStageException(self, exception):
    """Override and don't set status to FAIL but FORGIVEN instead."""
    return self._HandleExceptionAsWarning(exception)


class BoardSpecificBuilderStage(bs.BuilderStage):

  def __init__(self, options, build_config, board, suffix=None):
    super(BoardSpecificBuilderStage, self).__init__(options, build_config,
                                                    suffix)
    self._current_board = board

    if not isinstance(board, basestring):
      raise TypeError('Expected string, got %r' % (board,))

    # Add a board name suffix to differentiate between various boards (in case
    # more than one board is built on a single builder.)
    if len(self._boards) > 1 or build_config['grouped']:
      self.name = '%s [%s]' % (self.name, board)

  def GetImageDirSymlink(self, pointer='latest-cbuildbot'):
    """Get the location of the current image."""
    buildroot, board = self._options.buildroot, self._current_board
    return os.path.join(buildroot, 'src', 'build', 'images', board, pointer)


class CleanUpStage(bs.BuilderStage):
  """Stages that cleans up build artifacts from previous runs.

  This stage cleans up previous KVM state, temporary git commits,
  clobbers, and wipes tmp inside the chroot.
  """

  option_name = 'clean'

  def _CleanChroot(self):
    commands.CleanupChromeKeywordsFile(self._boards,
                                       self._build_root)
    chroot_tmpdir = os.path.join(self._build_root, 'chroot', 'tmp')
    if os.path.exists(chroot_tmpdir):
      cros_lib.SudoRunCommand(['rm', '-rf', chroot_tmpdir],
                          print_cmd=False)
      cros_lib.SudoRunCommand(['mkdir', '--mode', '1777', chroot_tmpdir],
                          print_cmd=False)

  def _DeleteChroot(self):
    chroot = os.path.join(self._build_root, 'chroot')
    if os.path.exists(chroot):
      cros_lib.RunCommand(['cros_sdk', '--delete', '--chroot=%s' % chroot],
                          self._build_root,
                          cwd=self._build_root)

  def _DeleteArchivedTrybotImages(self):
    """For trybots, clear all previus archive images to save space."""
    archive_root = ArchiveStage.GetTrybotArchiveRoot(self._build_root)
    shutil.rmtree(archive_root, ignore_errors=True)

  def _PerformStage(self):
    if not self._options.buildbot and self._options.clobber:
      if not commands.ValidateClobber(self._build_root):
        sys.exit(0)

    if self._options.clobber or not os.path.exists(
        os.path.join(self._build_root, '.repo')):
      self._DeleteChroot()
      repository.ClearBuildRoot(self._build_root, self._options.preserve_paths)
    else:
      # Clean mount points first to be safe about deleting.
      commands.CleanUpMountPoints(self._build_root)

      tasks = [functools.partial(commands.BuildRootGitCleanup,
                                 self._build_root),
               functools.partial(commands.WipeOldOutput, self._build_root),
               self._DeleteArchivedTrybotImages]
      if self._build_config['chroot_replace'] and self._options.build:
        tasks.append(self._DeleteChroot)
      else:
        tasks.append(self._CleanChroot)
      background.RunParallelSteps(tasks)


class PatchChangesStage(bs.BuilderStage):
  """Stage that patches a set of Gerrit changes to the buildroot source tree."""
  def __init__(self, options, build_config, gerrit_patches, local_patches):
    """Construct a PatchChangesStage.

    Args:
      options, build_config: See arguments to bs.BuilderStage.__init__()
      gerrit_patches: A list of cros_patch.GerritPatch objects to apply.
                      Cannot be None.
      local_patches: A list cros_patch.LocalPatch objects to apply. Cannot be
                     None.
    """
    bs.BuilderStage.__init__(self, options, build_config)
    self.gerrit_patches = gerrit_patches
    self.local_patches = local_patches

  def _PerformStage(self):
    for patch in self.local_patches:
      patch.Apply(self._build_root)

    for patch in self._options.remote_patches:
      # Construct the patch here because now we have buildroot.
      project, original_branch, ref, tracking_branch = patch.split(':')
      cros_lib.PrintBuildbotStepText('%s:%s' % (project, original_branch))

      push_url = constants.GERRIT_SSH_URL
      if cros_lib.IsProjectInternal(self._build_root, project):
        push_url = constants.GERRIT_INT_SSH_URL

      patch_object = cros_patch.GitRepoPatch(os.path.join(push_url, project),
                                             project, ref, tracking_branch)
      patch_object.Apply(self._build_root)

    for patch in self.gerrit_patches:
      cros_lib.PrintBuildbotLink(str(patch), patch.url)
      patch.Apply(self._build_root)


class SyncStage(bs.BuilderStage):
  """Stage that performs syncing for the builder."""

  option_name = 'sync'

  def __init__(self, options, build_config):
    super(SyncStage, self).__init__(options, build_config)
    self.repo = None
    self.skip_sync = False
    self.internal = self._build_config['internal']

  def _GetManifestVersionsRepoUrl(self, read_only=False):
    return cbuildbot_config.GetManifestVersionsRepoUrl(
        self.internal,
        read_only=read_only,
        test=self._build_config['unified_manifest_version'])

  def Initialize(self):
    self._InitializeRepo()

  def _InitializeRepo(self, git_url=None, build_root=None, **kwds):
    if build_root is None:
      build_root = self._build_root

    if git_url is None:
      git_url = self._build_config['git_url']

    kwds.setdefault('referenced_repo', self._options.reference_repo)
    kwds.setdefault('branch', self._target_manifest_branch)

    self.repo = repository.RepoRepository(git_url, build_root, **kwds)

  def GetNextManifest(self):
    """Returns the manifest to use."""
    return repository.RepoRepository.DEFAULT_MANIFEST

  def ManifestCheckout(self, next_manifest):
    """Checks out the repository to the given manifest."""
    print 'BUILDROOT: %s' % self.repo.directory
    print 'TRACKING BRANCH: %s' % self.repo.branch
    print 'NEXT MANIFEST: %s' % next_manifest

    if not self.skip_sync: self.repo.Sync(next_manifest)
    self.repo.ExportManifest('/dev/stderr')

  def _PerformStage(self):
    self.Initialize()
    self.ManifestCheckout(self.GetNextManifest())

  def HandleSkip(self):
    super(SyncStage, self).HandleSkip()
    # Ensure the gerrit remote is present for backwards compatibility.
    # TODO(davidjames): Remove this.
    configure_repo.SetupGerritRemote(self._build_root)


class LKGMSyncStage(SyncStage):
  """Stage that syncs to the last known good manifest blessed by builders."""

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

  manifest_manager = None

  def __init__(self, options, build_config):
    # Perform the sync at the end of the stage to the given manifest.
    super(ManifestVersionedSyncStage, self).__init__(options, build_config)
    self.repo = None

    # If a builder pushes changes (even with dryrun mode), we need a writable
    # repository. Otherwise, the push will be rejected by the server.
    self.manifest_repo = self._GetManifestVersionsRepoUrl(read_only=False)

    # 1. If we're uprevving Chrome, Chrome might have changed even if the
    #    manifest has not, so we should force a build to double check. This
    #    means that we'll create a new manifest, even if there are no changes.
    # 2. If we're running with --debug, we should always run through to
    #    completion, so as to ensure a complete test.
    self._force = self._chrome_rev or options.debug

  def HandleSkip(self):
    """Initializes a manifest manager to the specified version if skipped."""
    super(ManifestVersionedSyncStage, self).HandleSkip()
    if self._options.force_version:
      self.Initialize()
      self.ForceVersion(self._options.force_version)

  def ForceVersion(self, version):
    """Creates a manifest manager from given version and returns manifest."""
    return ManifestVersionedSyncStage.manifest_manager.BootstrapFromVersion(
        version)

  def Initialize(self):
    """Initializes a manager that manages manifests for associated stages."""
    increment = ('build' if self._target_manifest_branch == 'master'
                 else 'branch')

    dry_run = self._options.debug

    self._InitializeRepo()

    # If chrome_rev is somehow set, fail.
    assert not self._chrome_rev, \
        'chrome_rev is unsupported on release builders.'

    ManifestVersionedSyncStage.manifest_manager = \
        manifest_version.BuildSpecsManager(
            source_repo=self.repo,
            manifest_repo=self.manifest_repo,
            build_name=self._bot_id,
            incr_type=increment,
            force=self._force,
            dry_run=dry_run)

  def GetNextManifest(self):
    """Uses the initialized manifest manager to get the next manifest."""
    assert self.manifest_manager, \
        'Must run GetStageManager before checkout out build.'
    return self.manifest_manager.GetNextBuildSpec()

  def _PerformStage(self):
    self.Initialize()
    if self._options.force_version:
      next_manifest = self.ForceVersion(self._options.force_version)
    else:
      next_manifest = self.GetNextManifest()

    if not next_manifest:
      print 'Found no work to do.'
      if ManifestVersionedSyncStage.manifest_manager.DidLastBuildSucceed():
        sys.exit(0)
      else:
        cros_lib.Die('Last build status was non-passing.')

    # Log this early on for the release team to grep out before we finish.
    if ManifestVersionedSyncStage.manifest_manager:
      print '\nRELEASETAG: %s\n' % (
          ManifestVersionedSyncStage.manifest_manager.current_version)

    self.ManifestCheckout(next_manifest)


class LKGMCandidateSyncStage(ManifestVersionedSyncStage):
  """Stage that generates a unique manifest file candidate, and sync's to it."""

  sub_manager = None

  def __init__(self, options, build_config):
    super(LKGMCandidateSyncStage, self).__init__(options, build_config)
    # lkgm_manager deals with making sure we're synced to whatever manifest
    # we get back in GetNextManifest so syncing again is redundant.
    self.skip_sync = True

  def _GetInitializedManager(self, internal):
    """Returns an initialized lkgm manager."""
    increment = ('build' if self._target_manifest_branch == 'master'
                 else 'branch')
    return lkgm_manager.LKGMManager(
        source_repo=self.repo,
        manifest_repo=cbuildbot_config.GetManifestVersionsRepoUrl(
            internal, read_only=False,
            test=self._build_config['unified_manifest_version']),
        build_name=self._bot_id,
        build_type=self._build_config['build_type'],
        incr_type=increment,
        force=self._force,
        dry_run=self._options.debug)

  def Initialize(self):
    """Override: Creates an LKGMManager rather than a ManifestManager."""
    self._InitializeRepo()
    ManifestVersionedSyncStage.manifest_manager = self._GetInitializedManager(
        self.internal)
    if (self._build_config['unified_manifest_version'] and
        self._build_config['master']):
      assert self.internal, 'Unified masters must use an internal checkout.'
      LKGMCandidateSyncStage.sub_manager = self._GetInitializedManager(False)

  def ForceVersion(self, version):
    manifest = super(LKGMCandidateSyncStage, self).ForceVersion(version)
    if LKGMCandidateSyncStage.sub_manager:
      LKGMCandidateSyncStage.sub_manager.BootstrapFromVersion(version)

    return manifest

  def GetNextManifest(self):
    """Gets the next manifest using LKGM logic."""
    assert self.manifest_manager, \
        'Must run Initialize before we can get a manifest.'
    assert isinstance(self.manifest_manager, lkgm_manager.LKGMManager), \
        'Manifest manager instantiated with wrong class.'

    if self._build_config['master']:
      manifest = self.manifest_manager.CreateNewCandidate()
      if LKGMCandidateSyncStage.sub_manager:
        LKGMCandidateSyncStage.sub_manager.CreateFromManifest(manifest)

      return manifest

    else:
      return self.manifest_manager.GetLatestCandidate()


class CommitQueueSyncStage(LKGMCandidateSyncStage):
  """Commit Queue Sync stage that handles syncing and applying patches.

  This stage handles syncing to a manifest, passing around that manifest to
  other builders and finding the Gerrit Reviews ready to be committed and
  applying them into its out checkout.
  """

  # Path relative to the buildroot of where to store the pickled validation
  # pool.
  PICKLED_POOL_FILE = 'validation_pool.dump'

  pool = None

  def __init__(self, options, build_config):
    super(CommitQueueSyncStage, self).__init__(options, build_config)
    CommitQueueSyncStage.pool = None
    # Figure out the builder's name from the buildbot waterfall.
    builder_name = build_config['paladin_builder_name']
    self.builder_name = builder_name if builder_name else build_config['name']

  def SaveValidationPool(self):
    """Serializes the validation pool.

    Returns: returns a path to the serialized form of the validation pool.
    """
    path_to_file = os.path.join(self._build_root, self.PICKLED_POOL_FILE)
    with open(path_to_file, 'wb') as p_file:
      cPickle.dump(self.pool, p_file, protocol=cPickle.HIGHEST_PROTOCOL)

    return path_to_file

  def LoadValidationPool(self, path_to_file):
    """Loads the validation pool from the file."""
    with open(path_to_file, 'rb') as p_file:
      CommitQueueSyncStage.pool = cPickle.load(p_file)

  def HandleSkip(self):
    """Handles skip and initializes validation pool from manifest."""
    super(CommitQueueSyncStage, self).HandleSkip()
    if self._options.validation_pool:
      self.LoadValidationPool(self._options.validation_pool)
    else:
      self.SetPoolFromManifest(self.manifest_manager.GetLocalManifest())

  def SetPoolFromManifest(self, manifest):
    """Sets validation pool based on manifest path passed in."""
    CommitQueueSyncStage.pool = \
        validation_pool.ValidationPool.AcquirePoolFromManifest(
            manifest, self._build_config['overlays'], self._options.buildnumber,
            self.builder_name, self._build_config['master'],
            self._options.debug or
            self._build_config['unified_manifest_version'])

  def GetNextManifest(self):
    """Gets the next manifest using LKGM logic."""
    assert self.manifest_manager, \
        'Must run Initialize before we can get a manifest.'
    assert isinstance(self.manifest_manager, lkgm_manager.LKGMManager), \
        'Manifest manager instantiated with wrong class.'

    if self._build_config['master']:
      try:
        # In order to acquire a pool, we need an initialized buildroot.
        if not repository.InARepoRepository(self.repo.directory):
          self.repo.Initialize()

        pool = validation_pool.ValidationPool.AcquirePool(
            self._build_config['overlays'], self._build_root,
            self._options.buildnumber, self.builder_name,
            self._options.debug or
            self._build_config['unified_manifest_version'])

        # We only have work to do if there are changes to try.
        try:
          # Try our best to submit these but may have been overridden and won't
          # let that stop us from continuing the build.
          pool.SubmitNonManifestChanges()
        except validation_pool.FailedToSubmitAllChangesException as e:
          cros_lib.Warning(str(e))

        CommitQueueSyncStage.pool = pool

      except validation_pool.TreeIsClosedException as e:
        cros_lib.Warning(str(e))
        return None

      manifest = self.manifest_manager.CreateNewCandidate(validation_pool=pool)
      if LKGMCandidateSyncStage.sub_manager:
        LKGMCandidateSyncStage.sub_manager.CreateFromManifest(manifest)

      return manifest
    else:
      manifest = self.manifest_manager.GetLatestCandidate()
      if manifest:
        self.SetPoolFromManifest(manifest)
        self.pool.ApplyPoolIntoRepo(self._build_root)

      return manifest

  # Accessing a protected member.  TODO(sosa): Refactor PerformStage to not be
  # a protected member as children override it.
  # pylint: disable=W0212
  def _PerformStage(self):
    """Performs normal stage and prints blamelist at end."""
    if self._options.force_version:
      self.HandleSkip()
    else:
      ManifestVersionedSyncStage._PerformStage(self)


class ManifestVersionedSyncCompletionStage(ForgivingBuilderStage):
  """Stage that records board specific results for a unique manifest file."""

  option_name = 'sync'

  def __init__(self, options, build_config, success):
    super(ManifestVersionedSyncCompletionStage, self).__init__(
        options, build_config)
    self.success = success

  def _PerformStage(self):
    if ManifestVersionedSyncStage.manifest_manager:
      ManifestVersionedSyncStage.manifest_manager.UpdateStatus(
         success=self.success)


class ImportantBuilderFailedException(Exception):
  """Exception thrown when an important build fails to build."""
  pass


class LKGMCandidateSyncCompletionStage(ManifestVersionedSyncCompletionStage):
  """Stage that records whether we passed or failed to build/test manifest."""

  def _GetSlavesStatus(self):
    # If debugging or a slave, just check its local status.
    if not self._build_config['master'] or self._options.debug:
      return ManifestVersionedSyncStage.manifest_manager.GetBuildersStatus(
        [self._bot_id], os.path.join(self._build_root, constants.VERSION_FILE))

    if not LKGMCandidateSyncStage.sub_manager:
      return ManifestVersionedSyncStage.manifest_manager.GetBuildersStatus(
          self._GetSlavesForMaster(), os.path.join(self._build_root,
                                                   constants.VERSION_FILE))
    else:
      public_builders, private_builders = self._GetSlavesForUnifiedMaster()
      statuses = {}
      if public_builders:
        statuses.update(
          LKGMCandidateSyncStage.sub_manager.GetBuildersStatus(
              public_builders, os.path.join(self._build_root,
                                            constants.VERSION_FILE)))
      if private_builders:
        statuses.update(
            ManifestVersionedSyncStage.manifest_manager.GetBuildersStatus(
                private_builders, os.path.join(self._build_root,
                                          constants.VERSION_FILE)))
      return statuses

  def HandleSuccess(self):
    # We only promote for the pfq, not chrome pfq.
    if (cbuildbot_config.IsPFQType(self._build_config['build_type']) and
        self._build_config['master'] and
        self._target_manifest_branch == 'master' and
        ManifestVersionedSyncStage.manifest_manager != None and
        self._build_config['build_type'] != constants.CHROME_PFQ_TYPE):
      ManifestVersionedSyncStage.manifest_manager.PromoteCandidate()
      if LKGMCandidateSyncStage.sub_manager:
        LKGMCandidateSyncStage.sub_manager.PromoteCandidate()

  def HandleValidationFailure(self, failing_builders):
    print '\n@@@STEP_WARNINGS@@@'
    print 'The following builders failed with this manifest:'
    print ', '.join(sorted(failing_builders))
    print 'Please check the logs of the failing builders for details.'

  def HandleValidationTimeout(self, inflight_builders):
    print '\n@@@STEP_WARNINGS@@@'
    print 'The following builders took too long to finish:'
    print ', '.join(sorted(inflight_builders))
    print 'Please check the logs of these builders for details.'

  def _PerformStage(self):
    super(LKGMCandidateSyncCompletionStage, self)._PerformStage()

    if ManifestVersionedSyncStage.manifest_manager:
      statuses = self._GetSlavesStatus()
      failing_builders, inflight_builders = set(), set()
      for builder, status in statuses.iteritems():
        if status == lkgm_manager.LKGMManager.STATUS_FAILED:
          failing_builders.add(builder)
        elif status != lkgm_manager.LKGMManager.STATUS_PASSED:
          inflight_builders.add(builder)

      if failing_builders:
        self.HandleValidationFailure(failing_builders)

      if inflight_builders:
        self.HandleValidationTimeout(inflight_builders)

      if failing_builders or inflight_builders:
        raise bs.NonBacktraceBuildException()  # Suppress redundant output.
      else:
        self.HandleSuccess()


class CommitQueueCompletionStage(LKGMCandidateSyncCompletionStage):
  """Commits or reports errors to CL's that failed to be validated."""
  def HandleSuccess(self):
    if self._build_config['master']:
      CommitQueueSyncStage.pool.SubmitPool()
      # After submitting the pool, update the commit hashes for uprevved
      # ebuilds.
      portage_utilities.EBuild.UpdateCommitHashesForChanges(
          CommitQueueSyncStage.pool.changes, self._build_root)
      if cbuildbot_config.IsPFQType(self._build_config['build_type']):
        super(CommitQueueCompletionStage, self).HandleSuccess()

  def HandleValidationTimeout(self, inflight_builders):
    super(CommitQueueCompletionStage, self).HandleValidationTimeout(
        inflight_builders)
    CommitQueueSyncStage.pool.HandleValidationTimeout()

  def _PerformStage(self):
    if not self.success:
      CommitQueueSyncStage.pool.HandleValidationFailure()

    super(CommitQueueCompletionStage, self)._PerformStage()


class RefreshPackageStatusStage(bs.BuilderStage):
  """Stage for refreshing Portage package status in online spreadsheet."""
  def _PerformStage(self):
    commands.RefreshPackageStatus(buildroot=self._build_root,
                                  boards=self._boards,
                                  debug=self._options.debug)


class BuildBoardStage(bs.BuilderStage):
  """Stage that is responsible for building host pkgs and setting up a board."""

  option_name = 'build'

  def _PerformStage(self):
    chroot_upgrade = True
    chroot_path = os.path.join(self._build_root, 'chroot')
    if not os.path.isdir(chroot_path) or self._build_config['chroot_replace']:
      env = {}
      if self._options.clobber:
        env['IGNORE_PREFLIGHT_BINHOST'] = '1'

      commands.MakeChroot(
          buildroot=self._build_root,
          replace=self._build_config['chroot_replace'],
          use_sdk=self._build_config['use_sdk'],
          chrome_root=self._options.chrome_root,
          extra_env=env)
      chroot_upgrade = False
    else:
      commands.RunChrootUpgradeHooks(self._build_root)

    # Iterate through boards to setup.
    for board_to_build in self._boards:
      # Only build the board if the directory does not exist.
      board_path = os.path.join(chroot_path, 'build', board_to_build)
      if os.path.isdir(board_path):
        continue

      env = {}

      if self._options.clobber:
        env['IGNORE_PREFLIGHT_BINHOST'] = '1'

      latest_toolchain = self._build_config['latest_toolchain']

      if latest_toolchain and self._build_config['gcc_githash']:
        env['USE'] = 'git_gcc'
        env['GCC_GITHASH'] = self._build_config['gcc_githash']

      commands.SetupBoard(self._build_root,
                          board=board_to_build,
                          usepkg=self._build_config['usepkg_setup_board'],
                          latest_toolchain=latest_toolchain,
                          extra_env=env,
                          profile=self._options.profile or
                            self._build_config['profile'],
                          chroot_upgrade=chroot_upgrade)

      chroot_upgrade = False


class UprevStage(bs.BuilderStage):
  """Stage that uprevs Chromium OS packages that the builder intends to
  validate.
  """

  option_name = 'uprev'

  def _PerformStage(self):
    # Perform chrome uprev.
    chrome_atom_to_build = None
    if self._chrome_rev:
      chrome_atom_to_build = commands.MarkChromeAsStable(
          self._build_root, self._target_manifest_branch,
          self._chrome_rev, self._boards,
          chrome_root=self._options.chrome_root,
          chrome_version=self._options.chrome_version)

    # Perform other uprevs.
    if self._build_config['uprev']:
      overlays, _ = self._ExtractOverlays()
      commands.UprevPackages(self._build_root,
                             self._boards,
                             overlays)
    elif self._chrome_rev and not chrome_atom_to_build:
      # TODO(sosa): Do this in a better way.
      sys.exit(0)


class BuildTargetStage(BoardSpecificBuilderStage):
  """This stage builds Chromium OS for a target.

  Specifically, we build Chromium OS packages and perform imaging to get
  the images we want per the build spec."""

  option_name = 'build'

  def __init__(self, options, build_config, board, archive_stage, version):
    super(BuildTargetStage, self).__init__(options, build_config, board)
    self._env = {}
    if self._build_config.get('useflags'):
      self._env['USE'] = ' '.join(self._build_config['useflags'])

    if self._options.chrome_root:
      self._env['CHROME_ORIGIN'] = 'LOCAL_SOURCE'

    if self._options.clobber:
      self._env['IGNORE_PREFLIGHT_BINHOST'] = '1'

    self._archive_stage = archive_stage
    self._tarball_dir = None
    self._version = version if version else ''

  def _CommunicateVersion(self):
    """Communicates to archive_stage the image path of this stage."""
    image_path = self.GetImageDirSymlink()
    if os.path.isdir(image_path):
      version = os.readlink(image_path)
      # Non-versioned builds need the build number to uniquify the image.
      if not self._version:
        version = version + '-b%s' % self._options.buildnumber

      self._archive_stage.SetVersion(version)
    else:
      self._archive_stage.SetVersion(None)

  def HandleSkip(self):
    self._CommunicateVersion()

  def _BuildImages(self):
    # We only build base, dev, and test images from this stage.
    images_can_build = set(['base', 'dev', 'test'])
    images_to_build = set(self._build_config['images']).intersection(
        images_can_build)

    commands.BuildImage(self._build_root,
                        self._current_board,
                        list(images_to_build),
                        version=self._version,
                        extra_env=self._env)

    if self._build_config['vm_tests']:
      commands.BuildVMImageForTesting(self._build_root,
                                      self._current_board,
                                      extra_env=self._env)

    # Update link to latest image.
    latest_image = os.readlink(self.GetImageDirSymlink('latest'))
    cbuildbot_image_link = self.GetImageDirSymlink()
    if os.path.lexists(cbuildbot_image_link):
      os.remove(cbuildbot_image_link)

    os.symlink(latest_image, cbuildbot_image_link)
    self._CommunicateVersion()

  def _BuildAutotestTarballs(self):
    # Build autotest tarball, which is used in archive step. This is generated
    # here because the test directory is modified during the test phase, and we
    # don't want to include the modifications in the tarball.
    tarballs = commands.BuildAutotestTarballs(self._build_root,
                                              self._current_board,
                                              self._tarball_dir)
    self._archive_stage.AutotestTarballsReady(tarballs)

  def _PerformStage(self):
    build_autotest = (self._build_config['build_tests'] and
                      self._options.tests)

    # If we are using ToT toolchain, don't attempt to update
    # the toolchain during build_packages.
    skip_toolchain_update = self._build_config['latest_toolchain']

    commands.Build(self._build_root,
                   self._current_board,
                   build_autotest=build_autotest,
                   skip_toolchain_update=skip_toolchain_update,
                   usepkg=self._build_config['usepkg_build_packages'],
                   nowithdebug=self._build_config['nowithdebug'],
                   extra_env=self._env)

    # Build images and autotest tarball in parallel.
    steps = []
    if build_autotest and (self._build_config['upload_hw_test_artifacts'] or
                           self._build_config['archive_build_debug']):
      self._tarball_dir = tempfile.mkdtemp(prefix='autotest')
      steps.append(self._BuildAutotestTarballs)
    else:
      self._archive_stage.AutotestTarballsReady(None)

    steps.append(self._BuildImages)
    background.RunParallelSteps(steps)

    # TODO(sosa): Remove copy once crosbug.com/23690 is closed.
    if self._tarball_dir:
      shutil.copyfile(os.path.join(self._tarball_dir, 'autotest.tar.bz2'),
                      os.path.join(self.GetImageDirSymlink(),
                                   'autotest.tar.bz2'))

  def _HandleStageException(self, exception):
    # In case of an exception, this prevents any consumer from starving.
    self._archive_stage.AutotestTarballsReady(None)
    return super(BuildTargetStage, self)._HandleStageException(exception)


class ChromeTestStage(BoardSpecificBuilderStage):
  """Run chrome tests in a virtual machine."""

  option_name = 'tests'
  config_name = 'chrome_tests'

  def __init__(self, options, build_config, board, archive_stage):
    super(ChromeTestStage, self).__init__(options, build_config, board)
    self._archive_stage = archive_stage

  def _PerformStage(self):
    try:
      test_results_dir = None
      test_results_dir = commands.CreateTestRoot(self._build_root)
      commands.RunChromeSuite(self._build_root,
                              self._current_board,
                              self.GetImageDirSymlink(),
                              os.path.join(test_results_dir,
                                           'chrome_results'))
    except commands.TestException:
      raise bs.NonBacktraceBuildException()  # Suppress redundant output.
    finally:
      test_tarball = None
      if test_results_dir:
        test_tarball = commands.ArchiveTestResults(self._build_root,
                                                   test_results_dir,
                                                   prefix='chrome_')

      self._archive_stage.TestResultsReady(test_tarball)

  def HandleSkip(self):
    self._archive_stage.TestResultsReady(None)


class UnitTestStage(BoardSpecificBuilderStage):
  """Run unit tests."""

  option_name = 'tests'
  config_name = 'unittests'

  def _PerformStage(self):
    commands.RunUnitTests(self._build_root,
                          self._current_board,
                          full=(not self._build_config['quick_unit']),
                          nowithdebug=self._build_config['nowithdebug'])


class VMTestStage(BoardSpecificBuilderStage):
  """Run autotests in a virtual machine."""

  option_name = 'tests'
  config_name = 'vm_tests'

  def __init__(self, options, build_config, board, archive_stage):
    super(VMTestStage, self).__init__(options, build_config, board)
    self._archive_stage = archive_stage

  def _PerformStage(self):
    try:
      # These directories are used later to archive test artifacts.
      test_results_dir = None
      test_results_dir = commands.CreateTestRoot(self._build_root)

      commands.RunTestSuite(self._build_root,
                            self._current_board,
                            self.GetImageDirSymlink(),
                            os.path.join(test_results_dir,
                                         'test_harness'),
                            test_type=self._build_config['vm_tests'],
                            whitelist_chrome_crashes=self._chrome_rev is None,
                            build_config=self._bot_id)

    except commands.TestException:
      raise bs.NonBacktraceBuildException()  # Suppress redundant output.
    finally:
      test_tarball = None
      if test_results_dir:
        test_tarball = commands.ArchiveTestResults(self._build_root,
                                                   test_results_dir,
                                                   prefix='')

      self._archive_stage.TestResultsReady(test_tarball)

  def HandleSkip(self):
    self._archive_stage.TestResultsReady(None)


class HWTestStage(BoardSpecificBuilderStage, NonHaltingBuilderStage):
  """Stage that runs tests in the Autotest lab."""

  # If the tests take longer than an hour, abort.
  INSTRASTRUCTURE_TIMEOUT = 3600
  option_name = 'tests'
  config_name = 'hw_tests'

  def __init__(self, options, build_config, board, archive_stage, suite):
    super(HWTestStage, self).__init__(options, build_config, board,
                                      suffix=' [%s]' % suite)
    self._archive_stage = archive_stage
    self._suite = suite

  # Disable use of calling parents HandleStageException class.
  # pylint: disable=W0212
  def _HandleStageException(self, exception):
    """Override and don't set status to FAIL but FORGIVEN instead."""
    if (isinstance(exception, cros_lib.RunCommandError) and
        exception.result.returncode == 2):
      return self._HandleExceptionAsWarning(exception)
    else:
      return super(HWTestStage, self)._HandleStageException(exception)

  def _PerformStage(self):
    if not self._archive_stage.WaitForHWTestUploads():
      raise Exception('Missing uploads.')

    build = '%s/%s' % (self._bot_id, self._archive_stage.GetVersion())

    try:
      with cros_lib.SubCommandTimeout(HWTestStage.INSTRASTRUCTURE_TIMEOUT):
        commands.RunHWTestSuite(build, self._suite, self._current_board,
                                self._options.debug)

    except cros_lib.TimeoutError as exception:
      return self._HandleExceptionAsWarning(exception)


class SDKTestStage(bs.BuilderStage):
  """Stage that performs testing an SDK created in a previous stage"""
  def _PerformStage(self):
    tarball_location = os.path.join(self._build_root, 'built-sdk.tbz2')
    board_location = os.path.join(self._build_root, 'chroot/build/amd64-host')

    # Create a tarball of the latest SDK.
    cmd = ['tar', '-jcf', tarball_location]
    excluded_paths = ('usr/lib/debug', 'usr/local/autotest', 'packages',
                      'tmp')
    for path in excluded_paths:
      cmd.append('--exclude=%s/*' % path)
    cmd.append('.')
    cros_lib.SudoRunCommand(cmd, cwd=board_location)

    # Make sure the regular user has the permission to read.
    cmd = ['chmod', 'a+r', tarball_location]
    cros_lib.SudoRunCommand(cmd, cwd=board_location)

    new_chroot_cmd = ['cros_sdk', '--chroot', 'new-sdk-chroot']
    # Build a new SDK using the tarball.
    cmd = new_chroot_cmd + ['--download', '--replace',
        '--url', 'file://' + tarball_location]
    cros_lib.RunCommand(cmd, cwd=self._build_root)

    for board in cbuildbot_config.SDK_TEST_BOARDS:
      cmd = new_chroot_cmd + ['--', './setup_board',
          '--board', board, '--skip_chroot_upgrade']
      cros_lib.RunCommand(cmd, cwd=self._build_root)
      cmd = new_chroot_cmd + ['--', './build_packages',
          '--board', board, '--nousepkg', '--skip_chroot_upgrade']
      cros_lib.RunCommand(cmd, cwd=self._build_root)


class NothingToArchiveException(Exception):
  """Thrown if ArchiveStage found nothing to archive."""
  def __init__(self, message='No images found to archive.'):
    super(NothingToArchiveException, self).__init__(message)


class ArchiveStage(BoardSpecificBuilderStage):
  """Archives build and test artifacts for developer consumption."""

  option_name = 'archive'
  _VERSION_NOT_SET = '_not_set_version_'
  _REMOTE_TRYBOT_ARCHIVE_URL = 'gs://chromeos-trybot'

  @classmethod
  def GetTrybotArchiveRoot(cls, buildroot):
    """Return the location where trybot archive images are kept."""
    return os.path.join(buildroot, 'trybot_archive')

  # This stage is intended to run in the background, in parallel with tests.
  def __init__(self, options, build_config, board):
    super(ArchiveStage, self).__init__(options, build_config, board)
    # Set version is dependent on setting external to class.  Do not use
    # directly.  Use GetVersion() instead.
    self._set_version = ArchiveStage._VERSION_NOT_SET
    if self._options.buildbot:
      self._archive_root = BUILDBOT_ARCHIVE_PATH
    else:
      self._archive_root = self.GetTrybotArchiveRoot(self._build_root)

    self._bot_archive_root = os.path.join(self._archive_root, self._bot_id)

    # Queues that are populated during the Archive stage.
    self._breakpad_symbols_queue = multiprocessing.Queue()
    self._hw_test_uploads_status_queue = multiprocessing.Queue()

    # Queues that are populated by other stages.
    self._version_queue = multiprocessing.Queue()
    self._autotest_tarballs_queue = multiprocessing.Queue()
    self._test_results_queue = multiprocessing.Queue()

  def SetVersion(self, path_to_image):
    """Sets the cros version for the given built path to an image.

    This must be called in order for archive stage to finish.

    Args:
      path_to_image: Path to latest image.""
    """
    self._version_queue.put(path_to_image)

  def AutotestTarballsReady(self, autotest_tarballs):
    """Tell Archive Stage that autotest tarball is ready.

    This must be called in order for archive stage to finish.

    Args:
      autotest_tarballs: The paths of the autotest tarballs.
    """
    self._autotest_tarballs_queue.put(autotest_tarballs)

  def TestResultsReady(self, test_results):
    """Tell Archive Stage that test results are ready.

    This must be called twice (with VM test results and Chrome test results)
    in order for archive stage to finish.

    Args:
      test_results: The test results tarball from the tests. If no tests
                    results are available, this should be set to None.
    """
    self._test_results_queue.put(test_results)

  def GetVersion(self):
    """Gets the version for the archive stage."""
    if self._set_version == ArchiveStage._VERSION_NOT_SET:
      version = self._version_queue.get()
      self._set_version = version
      # Put the version right back on the queue in case anyone else is waiting.
      self._version_queue.put(version)

    return self._set_version

  def WaitForHWTestUploads(self):
    """Waits until artifacts needed for HWTest stage are uploaded.

    Returns:
      True if artifacts uploaded successfully.
      False otherswise.
    """
    cros_lib.Info('Waiting for uploads...')
    status = self._hw_test_uploads_status_queue.get()
    # Put the status back so other HWTestStage instances don't starve.
    self._hw_test_uploads_status_queue.put(status)
    return status

  def _BreakpadSymbolsGenerated(self, success):
    """Signal that breakpad symbols have been generated.

    Arguments:
      success: True to indicate the symbols were generated, else False.
    """
    self._breakpad_symbols_queue.put(success)

  def _WaitForBreakpadSymbols(self):
    """Wait for the breakpad symbols to be generated.

    Returns:
      True if the breakpad symbols were generated.
      False if the breakpad symbols were not generated within 20 mins.
    """
    success = False
    try:
      # TODO: Clean this up so that we no longer rely on a timeout
      success = self._breakpad_symbols_queue.get(True, 1200)
    except Queue.Empty:
      cros_lib.Warning('Breakpad symbols were not generated within timeout '
                       'period.')
    return success

  def GetDownloadUrl(self):
    """Get the URL where we can download artifacts."""
    version = self.GetVersion()
    if not version:
      return None

    if self._options.buildbot or self._options.remote_trybot:
      upload_location = self.GetGSUploadLocation()
      url_prefix = 'https://sandbox.google.com/storage/'
      return upload_location.replace('gs://', url_prefix)
    else:
      return self._GetArchivePath()

  def _GetGSUtilArchiveDir(self):
    if self._options.remote_trybot:
      current_time = datetime.datetime.utcnow()
      return os.path.join(
          self._REMOTE_TRYBOT_ARCHIVE_URL,
          current_time.strftime('%Y_%m_%d'),
          self._bot_id,
          str(self._options.buildnumber))
    elif self._build_config['gs_path'] == cbuildbot_config.GS_PATH_DEFAULT:
      return 'gs://chromeos-image-archive/' + self._bot_id
    else:
      return self._build_config['gs_path']

  def GetGSUploadLocation(self):
    """Get the Google Storage location where we should upload artifacts."""
    gsutil_archive = self._GetGSUtilArchiveDir()
    version = self.GetVersion()
    if version:
      return '%s/%s' % (gsutil_archive, version)

  def _GetArchivePath(self):
    version = self.GetVersion()
    if version:
      return os.path.join(self._bot_archive_root, version)

  def _GetAutotestTarballs(self):
    """Get the paths of the autotest tarballs."""
    autotest_tarballs = None
    if self._options.build:
      cros_lib.Info('Waiting for autotest tarballs ...')
      autotest_tarballs = self._autotest_tarballs_queue.get()
      if autotest_tarballs:
        cros_lib.Info('Found autotest tarballs %r ...' % autotest_tarballs)
      else:
        cros_lib.Info('No autotest tarballs found.')

    return autotest_tarballs

  def _GetTestResults(self):
    """Get the path to the test results tarball."""
    for _ in range(2):
      cros_lib.Info('Waiting for test results dir...')
      test_tarball = self._test_results_queue.get()
      if test_tarball:
        cros_lib.Info('Found test results tarball at %s...' % test_tarball)
        yield test_tarball
      else:
        cros_lib.Info('No test results.')

  def _SetupArchivePath(self):
    """Create a fresh directory for archiving a build."""
    archive_path = self._GetArchivePath()
    if not archive_path:
      return None

    if self._options.buildbot:
      # Buildbot: Clear out any leftover build artifacts, if present.
      shutil.rmtree(archive_path, ignore_errors=True)

    os.makedirs(archive_path)

    return archive_path

  def _PerformStage(self):
    if self._options.remote_trybot:
      debug = self._options.debug_forced
    else:
      debug = self._options.debug

    buildroot = self._build_root
    config = self._build_config
    board = self._current_board
    upload_url = self.GetGSUploadLocation()
    archive_path = self._SetupArchivePath()
    image_dir = self.GetImageDirSymlink()
    upload_queue = multiprocessing.Queue()
    hw_test_upload_queue = multiprocessing.Queue()
    bg_task_runner = background.BackgroundTaskRunner

    extra_env = {}
    if config['useflags']:
      extra_env['USE'] = ' '.join(config['useflags'])

    if not archive_path:
      raise NothingToArchiveException()

    # The following functions are run in parallel (except where indicated
    # otherwise)
    # \- BuildAndArchiveArtifacts
    #    \- ArchiveArtifactsForHWTesting
    #       \- ArchiveAutotestTarballs
    #       \- ArchivePayloads
    #    \- ArchiveTestResults
    #    \- ArchiveDebugSymbols
    #    \- ArchiveFirmwareImages
    #    \- BuildAndArchiveAllImages
    #       (builds recovery image first, then launches functions below)
    #       \- BuildAndArchiveFactoryImages
    #       \- ArchiveRegularImages

    def ArchiveAutotestTarballs():
      """Archives the autotest tarballs produced in BuildTarget."""
      autotest_tarballs = self._GetAutotestTarballs()
      if autotest_tarballs:
        for tarball in autotest_tarballs:
          hw_test_upload_queue.put([commands.ArchiveFile(tarball,
                                                         archive_path)])

    def ArchivePayloads():
      """Archives update payloads when they are ready."""
      if self._build_config['upload_hw_test_artifacts']:
        update_payloads_dir = tempfile.mkdtemp(prefix='cbuildbot')
        target_image_path = os.path.join(self.GetImageDirSymlink(),
                                         'chromiumos_test_image.bin')
        # For non release builds, we are only interested in generating payloads
        # for the purpose of imaging machines. This means we shouldn't generate
        # delta payloads for n-1->n testing.
        if self._build_config['build_type'] != constants.CANARY_TYPE:
          commands.GenerateFullPayload(
              buildroot, target_image_path, update_payloads_dir)
        else:
          commands.GenerateNPlus1Payloads(
              buildroot, self._bot_id, target_image_path, update_payloads_dir)

        for payload in os.listdir(update_payloads_dir):
          full_path = os.path.join(update_payloads_dir, payload)
          hw_test_upload_queue.put([commands.ArchiveFile(full_path,
                                                         archive_path)])

    def ArchiveTestResults():
      """Archives test results when they are ready."""
      got_symbols = self._WaitForBreakpadSymbols()
      for test_results in self._GetTestResults():
        if got_symbols:
          filenames = commands.GenerateMinidumpStackTraces(buildroot,
                                                           board, test_results,
                                                           archive_path)
          for filename in filenames:
            upload_queue.put([filename])
        upload_queue.put([commands.ArchiveFile(test_results, archive_path)])

    def ArchiveDebugSymbols():
      """Generate and upload debug symbols."""
      if config['archive_build_debug'] or config['vm_tests']:
        success = False
        try:
          commands.GenerateBreakpadSymbols(buildroot, board)
          success = True
        finally:
          self._BreakpadSymbolsGenerated(success)
        filename = commands.GenerateDebugTarball(
            buildroot, board, archive_path, config['archive_build_debug'])
        upload_queue.put([filename])
        if not debug and config['upload_symbols']:
          commands.UploadSymbols(buildroot,
                                 board=board,
                                 official=config['chromeos_official'])
      else:
        self._BreakpadSymbolsGenerated(False)

    def BuildAndArchiveFactoryImages():
      """Build and archive the factory zip file.

      The factory zip file consists of the factory test image and the factory
      install image. Both are built here.
      """

      # Build factory test image and create symlink to it.
      factory_test_symlink = None
      if 'factory_test' in config['images']:
        alias = commands.BuildFactoryTestImage(buildroot, board, extra_env)
        factory_test_symlink = self.GetImageDirSymlink(alias)

      # Build factory install image and create a symlink to it.
      factory_install_symlink = None
      if 'factory_install' in config['images']:
        alias = commands.BuildFactoryInstallImage(buildroot, board, extra_env)
        factory_install_symlink = self.GetImageDirSymlink(alias)
        if config['factory_install_netboot']:
          commands.MakeNetboot(buildroot, board, factory_install_symlink)

      # Build and upload factory zip.
      if factory_install_symlink and factory_test_symlink:
        image_root = os.path.dirname(factory_install_symlink)
        filename = commands.BuildFactoryZip(buildroot, archive_path, image_root)
        upload_queue.put([filename])

    def ArchiveRegularImages():
      """Build and archive image.zip and the hwqual image."""

      # Zip up everything in the image directory.
      upload_queue.put([commands.BuildImageZip(archive_path, image_dir)])

      # TODO(petermayo): This logic needs to be exported from the BuildTargets
      # stage rather than copied/re-evaluated here.
      autotest_built = config['build_tests'] and self._options.tests and (
          config['upload_hw_test_artifacts'] or config['archive_build_debug'])

      if config['chromeos_official'] and autotest_built:
        # Build hwqual image and upload to Google Storage.
        version = self.GetVersion()
        hwqual_name = 'chromeos-hwqual-%s-%s' % (board, version)
        filename = commands.ArchiveHWQual(buildroot, hwqual_name, archive_path)
        upload_queue.put([filename])

      # Archive au-generator.zip.
      filename = 'au-generator.zip'
      shutil.copy(os.path.join(image_dir, filename), archive_path)
      upload_queue.put([filename])

    def ArchiveFirmwareImages():
      """Archive firmware images built from source if available."""
      archive = commands.BuildFirmwareArchive(buildroot, board, archive_path)
      if archive:
        upload_queue.put([archive])

    def BuildAndArchiveAllImages():
      # If we're an official build, generate the recovery image. To conserve
      # loop devices, we try to only run one instance of build_image at a
      # time. TODO(davidjames): Move the image generation out of the archive
      # stage.
      if config['chromeos_official'] and 'base' in config['images']:
        commands.BuildRecoveryImage(buildroot, board, image_dir, extra_env)

      background.RunParallelSteps([BuildAndArchiveFactoryImages,
                                   ArchiveRegularImages])

    def UploadArtifact(filename):
      """Upload generated artifact to Google Storage."""
      commands.UploadArchivedFile(archive_path, upload_url, filename, debug)

    def ArchiveArtifactsForHWTesting(num_upload_processes=6):
      """Archives artifacts required for HWTest stage."""
      queue = hw_test_upload_queue
      success = False
      try:
        with bg_task_runner(queue, UploadArtifact, num_upload_processes):
          steps = [ArchiveAutotestTarballs, ArchivePayloads]
          background.RunParallelSteps(steps)
        success = True
      finally:
        self._hw_test_uploads_status_queue.put(success)

    def BuildAndArchiveArtifacts(num_upload_processes=10):
      with bg_task_runner(upload_queue, UploadArtifact, num_upload_processes):
        # Run archiving steps in parallel.
        steps = [ArchiveDebugSymbols, BuildAndArchiveAllImages,
                 ArchiveArtifactsForHWTesting, ArchiveTestResults,
                 ArchiveFirmwareImages]

        background.RunParallelSteps(steps)

    def PushImage():
      # Now that all data has been generated, we can upload the final result to
      # the image server.
      # TODO: When we support branches fully, the friendly name of the branch
      # needs to be used with PushImages
      if not debug and config['push_image']:
        commands.PushImages(buildroot,
                            board=board,
                            branch_name='master',
                            archive_url=upload_url,
                            profile=self._options.profile or config['profile'])

    def MarkAsLatest():
      # Update and upload LATEST file.
      version = self.GetVersion()
      if version:
        commands.UpdateLatestFile(self._bot_archive_root, version)
        commands.UploadArchivedFile(self._bot_archive_root,
                                    self._GetGSUtilArchiveDir(), 'LATEST',
                                    debug)

    try:
      BuildAndArchiveArtifacts()
      PushImage()
      MarkAsLatest()
    finally:
      commands.RemoveOldArchives(self._bot_archive_root,
                                 self._options.max_archive_builds)

  def _HandleStageException(self, exception):
    # Tell the HWTestStage not to wait for artifacts to be uploaded
    # in case ArchiveStage throws an exception.
    self._hw_test_uploads_status_queue.put(False)
    return super(ArchiveStage, self)._HandleStageException(exception)


class UploadPrebuiltsStage(BoardSpecificBuilderStage):
  """Uploads binaries generated by this build for developer use."""

  option_name = 'prebuilts'
  config_name = 'prebuilts'

  # TODO(sosa): Fix prebuilts for unified build.
  def _PerformStage(self):
    manifest_manager = ManifestVersionedSyncStage.manifest_manager
    overlay_config = self._build_config['overlays']
    prebuilt_type = self._prebuilt_type
    board = self._current_board
    binhost_bucket = self._build_config['binhost_bucket']
    binhost_key = self._build_config['binhost_key']
    binhost_base_url = self._build_config['binhost_base_url']
    git_sync = self._build_config['git_sync']
    private_bucket = overlay_config in (constants.PRIVATE_OVERLAYS,
                                        constants.BOTH_OVERLAYS)

    binhosts = []
    extra_args = []

    # Check if we are uploading dev_installer prebuilts.
    use_binhost_package_file = False
    if self._build_config['dev_installer_prebuilts']:
      use_binhost_package_file = True
      private_bucket = False

    if manifest_manager and manifest_manager.current_version:
      version = manifest_manager.current_version
      extra_args = ['--set-version', version]

    if cbuildbot_config.IsPFQType(prebuilt_type):
      overlays = self._build_config['overlays']
      # The master builder updates all the binhost conf files, and needs to do
      # so only once so as to ensure it doesn't try to update the same file
      # more than once. We arbitrarily decided to update the binhost conf
      # files when we run prebuilt.py for the last board. The other boards are
      # marked as slave boards.
      if self._build_config['master'] and board == self._boards[-1]:
        extra_args.append('--sync-binhost-conf')
        config = cbuildbot_config.config

        # If we share a version number with our slaves, we know what URLs
        # they are going to use, so we can update the binhost conf on their
        # behalf.
        builders = []
        if manifest_manager and manifest_manager.current_version:
          builders = self._GetSlavesForMaster()

        for builder in builders:
          builder_config = config[builder]
          if builder_config['prebuilts']:
            for slave_board in builder_config['boards']:
              if builder_config['master'] and slave_board == board:
                continue
              extra_args.extend(['--slave-board', slave_board])
              slave_profile = builder_config.get('profile')
              if slave_profile:
                extra_args.extend(['--slave-profile', slave_profile])

      # Pre-flight queues should upload host preflight prebuilts.
      if (cbuildbot_config.IsPFQType(prebuilt_type)
          and overlays == constants.PUBLIC_OVERLAYS
          and prebuilt_type != constants.CHROME_PFQ_TYPE):
        extra_args.append('--sync-host')

      # Deduplicate against previous binhosts.
      binhosts = []
      binhosts.extend(self._GetPortageEnvVar(_PORTAGE_BINHOST, board).split())
      binhosts.extend(self._GetPortageEnvVar(_PORTAGE_BINHOST, None).split())
      for binhost in binhosts:
        if binhost:
          extra_args.extend(['--previous-binhost-url', binhost])

    if self._options.debug:
      extra_args.append('--debug')

    profile = self._options.profile or self._build_config['profile']
    if profile:
      extra_args.extend(['--profile', profile])

    # Upload prebuilts.
    commands.UploadPrebuilts(
        self._build_root, board, private_bucket, prebuilt_type,
        self._chrome_rev, binhost_bucket, binhost_key, binhost_base_url,
        use_binhost_package_file, git_sync, extra_args)


class PublishUprevChangesStage(NonHaltingBuilderStage):
  """Makes uprev changes from pfq live for developers."""
  def _PerformStage(self):
    _, push_overlays = self._ExtractOverlays()
    if push_overlays:
      commands.UprevPush(self._build_root,
                         push_overlays,
                         self._options.debug or
                         self._build_config['unified_manifest_version'])
