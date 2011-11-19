# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing the various stages that a builder runs."""

import multiprocessing
import os
import Queue
import shutil
import socket
import sys
import tempfile
import traceback

from chromite.buildbot import builderstage as bs
from chromite.buildbot import cbuildbot_background as background
from chromite.buildbot import cbuildbot_results as results_lib
from chromite.buildbot import cbuildbot_commands as commands
from chromite.buildbot import cbuildbot_config
from chromite.buildbot import constants
from chromite.buildbot import lkgm_manager
from chromite.buildbot import manifest_version
from chromite.buildbot import patch as cros_patch
from chromite.buildbot import repository
from chromite.buildbot import validation_pool
from chromite.lib import cros_build_lib as cros_lib

_FULL_BINHOST = 'FULL_BINHOST'
_PORTAGE_BINHOST = 'PORTAGE_BINHOST'
_CROS_ARCHIVE_URL = 'CROS_ARCHIVE_URL'
_PRINT_INTERVAL = 1

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
    print '\n@@@STEP_WARNINGS@@@'
    description = traceback.format_exc()
    print >> sys.stderr, description
    return results_lib.Results.FORGIVEN, None


class CleanUpStage(bs.BuilderStage):
  """Stages that cleans up build artifacts from previous runs.

  This stage cleans up previous KVM state, temporary git commits,
  clobbers, and wipes tmp inside the chroot.
  """

  option_name = 'clean'

  def _PerformStage(self):
    if not self._options.buildbot and self._options.clobber:
      if not commands.ValidateClobber(self._build_root):
        sys.exit(0)

    if self._options.clobber or not os.path.exists(
        os.path.join(self._build_root, '.repo')):
      chroot = os.path.join(self._build_root, 'chroot')
      if os.path.exists(chroot):
        cros_lib.RunCommand(['cros_sdk', '--delete', '--chroot=%s' % chroot],
                            self._build_root,
                            cwd=self._build_root)
      repository.ClearBuildRoot(self._build_root)
    else:
      commands.PreFlightRinse(self._build_root)
      commands.CleanupChromeKeywordsFile(self._build_config['board'],
                                         self._build_root)
      chroot_tmpdir = os.path.join(self._build_root, 'chroot', 'tmp')
      if os.path.exists(chroot_tmpdir):
        cros_lib.RunCommand(['sudo', 'rm', '-rf', chroot_tmpdir],
                            print_cmd=False)
        cros_lib.RunCommand(['sudo', 'mkdir', '--mode', '1777', chroot_tmpdir],
                            print_cmd=False)


class SyncStage(bs.BuilderStage):
  """Stage that performs syncing for the builder."""

  option_name = 'sync'

  def _PerformStage(self):
    commands.ManifestCheckout(self._build_root, self._tracking_branch,
                              repository.RepoRepository.DEFAULT_MANIFEST,
                              self._build_config['git_url'])


class PatchChangesStage(bs.BuilderStage):
  """Stage that patches a set of Gerrit changes to the buildroot source tree."""
  def __init__(self, bot_id, options, build_config, gerrit_patches,
               local_patches):
    """Construct a PatchChangesStage.

    Args:
      bot_id, options, build_config: See arguments to bs.BuilderStage.__init__()
      gerrit_patches: A list of cros_patch.GerritPatch objects to apply.
                      Cannot be None.
      local_patches: A list cros_patch.LocalPatch objects to apply. Cannot be
                     None.
    """
    bs.BuilderStage.__init__(self, bot_id, options, build_config)
    self.gerrit_patches = gerrit_patches
    self.local_patches = local_patches

  def _PerformStage(self):
    for patch in self.gerrit_patches + self.local_patches:
      patch.Apply(self._build_root)

    if self.local_patches:
      patch_root = os.path.dirname(self.local_patches[0].patch_dir)
      cros_patch.RemovePatchRoot(patch_root)


class ManifestVersionedSyncStage(SyncStage):
  """Stage that generates a unique manifest file, and sync's to it."""

  manifest_manager = None

  def _GetManifestVersionsRepoUrl(self, read_only=False):
    return cbuildbot_config._GetManifestVersionsRepoUrl(
        cbuildbot_config.IsInternalBuild(self._build_config),
        read_only=read_only)

  def HandleSkip(self):
    """Initializes a manifest manager to the specified version if skipped."""
    if self._options.force_version:
      self._ForceVersion(self._options.force_version)

  def _ForceVersion(self, version):
    """Creates a manifest manager from given version and returns manifest."""
    self.InitializeManifestManager()
    return ManifestVersionedSyncStage.manifest_manager.BootstrapFromVersion(
        version)

  def InitializeManifestManager(self):
    """Initializes a manager that manages manifests for associated stages."""
    increment = 'build' if self._tracking_branch == 'master' else 'branch'

    dry_run = self._options.debug
    ManifestVersionedSyncStage.manifest_manager = \
        manifest_version.BuildSpecsManager(
            source_dir=self._build_root,
            checkout_repo=self._build_config['git_url'],
            manifest_repo=self._GetManifestVersionsRepoUrl(read_only=dry_run),
            branch=self._tracking_branch,
            build_name=self._bot_id,
            incr_type=increment,
            dry_run=dry_run)

  def GetNextManifest(self):
    """Uses the initialized manifest manager to get the next manifest."""
    assert self.manifest_manager, \
        'Must run GetStageManager before checkout out build.'
    return self.manifest_manager.GetNextBuildSpec()

  def _PerformStage(self):
    if self._options.force_version:
      next_manifest = self._ForceVersion(self._options.force_version)
    else:
      self.InitializeManifestManager()
      next_manifest = self.GetNextManifest()

    if not next_manifest:
      print 'Found no work to do.'
      if ManifestVersionedSyncStage.manifest_manager.DidLastBuildSucceed():
        sys.exit(0)
      else:
        cros_lib.Die('Last build status was non-passing.')

    # Log this early on for the release team to grep out before we finish.
    if ManifestVersionedSyncStage.manifest_manager:
      print
      print 'RELEASETAG: %s' % (
          ManifestVersionedSyncStage.manifest_manager.current_version)
      print

    commands.ManifestCheckout(self._build_root,
                              self._tracking_branch,
                              next_manifest,
                              self._build_config['git_url'])


class LKGMCandidateSyncStage(ManifestVersionedSyncStage):
  """Stage that generates a unique manifest file candidate, and sync's to it."""

  def InitializeManifestManager(self):
    """Override: Creates an LKGMManager rather than a ManifestManager."""
    dry_run = self._options.debug
    ManifestVersionedSyncStage.manifest_manager = lkgm_manager.LKGMManager(
        source_dir=self._build_root,
        checkout_repo=self._build_config['git_url'],
        manifest_repo=self._GetManifestVersionsRepoUrl(read_only=dry_run),
        branch=self._tracking_branch,
        build_name=self._bot_id,
        build_type=self._build_config['build_type'],
        dry_run=dry_run)

  def GetNextManifest(self):
    """Gets the next manifest using LKGM logic."""
    assert self.manifest_manager, \
        'Must run InitializeManifestManager before we can get a manifest.'
    assert isinstance(self.manifest_manager, lkgm_manager.LKGMManager), \
        'Manifest manager instantiated with wrong class.'

    if self._build_config['master']:
      return self.manifest_manager.CreateNewCandidate()
    else:
      return self.manifest_manager.GetLatestCandidate()

  def _PerformStage(self):
    """Performs normal stage and prints blamelist at end."""
    super(LKGMCandidateSyncStage, self)._PerformStage()
    if self._tracking_branch == 'master':
      self.manifest_manager.GenerateBlameListSinceLKGM()


class CommitQueueSyncStage(LKGMCandidateSyncStage):
  """Commit Queue Sync stage that handles syncing and applying patches.

  This stage handles syncing to a manifest, passing around that manifest to
  other builders and finding the Gerrit Reviews ready to be committed and
  applying them into its out checkout.
  """
  pool = None

  def __init__(self, bot_id, options, build_config):
    super(CommitQueueSyncStage, self).__init__(bot_id, options, build_config)
    CommitQueueSyncStage.pool = None

  def HandleSkip(self):
    """Handles skip and initializes validation pool from manifest."""
    super(CommitQueueSyncStage, self).HandleSkip()
    self.SetPoolFromManifest(self.manifest_manager.GetLocalManifest())

  def SetPoolFromManifest(self, manifest):
    """Sets validation pool based on manifest path passed in."""
    internal = cbuildbot_config.IsInternalBuild(self._build_config)
    CommitQueueSyncStage.pool = \
        validation_pool.ValidationPool.AcquirePoolFromManifest(
            manifest, internal, self._options.buildnumber, self._options.debug)

  def GetNextManifest(self):
    """Gets the next manifest using LKGM logic."""
    assert self.manifest_manager, \
        'Must run InitializeManifestManager before we can get a manifest.'
    assert isinstance(self.manifest_manager, lkgm_manager.LKGMManager), \
        'Manifest manager instantiated with wrong class.'

    if self._build_config['master']:
      try:
        # In order to acquire a pool, we need an initialized buildroot.
        if not repository.InARepoRepository(self._build_root):
          repository.RepoRepository(
              self._build_config['git_url'], self._build_root,
              self._tracking_branch).Initialize()

        internal = cbuildbot_config.IsInternalBuild(self._build_config)
        pool = validation_pool.ValidationPool.AcquirePool(
            self._tracking_branch, internal, self._build_root,
            self._options.buildnumber, self._options.debug)
        # We only have work to do if there are changes to try.
        if pool.changes:
          CommitQueueSyncStage.pool = pool
        else:
          return None

      except validation_pool.TreeIsClosedException as e:
        cros_lib.Warning(str(e))
        return None

      return self.manifest_manager.CreateNewCandidate(patches=self.pool.changes)
    else:
      manifest = self.manifest_manager.GetLatestCandidate()
      self.SetPoolFromManifest(manifest)
      return manifest

  def _PerformStage(self):
    """Performs normal stage and prints blamelist at end."""
    if self._options.force_version:
      self.HandleSkip()
    else:
      super(LKGMCandidateSyncStage, self)._PerformStage()

    if not self.pool.ApplyPoolIntoRepo(self._build_root):
      print 'No patches applied cleanly.  Found no work to do.'
      sys.exit(0)


class LKGMSyncStage(ManifestVersionedSyncStage):
  """Stage that syncs to the last known good manifest blessed by builders."""

  def InitializeManifestManager(self):
    """Override: don't do anything."""
    pass

  def GetNextManifest(self):
    """Override: Gets the LKGM."""
    manifests_dir = lkgm_manager.LKGMManager.GetManifestDir()
    if os.path.exists(manifests_dir):
      shutil.rmtree(manifests_dir)

    repository.CloneGitRepo(manifests_dir,
                            self._GetManifestVersionsRepoUrl(read_only=True))
    return lkgm_manager.LKGMManager.GetAbsolutePathToLKGM()


class ManifestVersionedSyncCompletionStage(ForgivingBuilderStage):
  """Stage that records board specific results for a unique manifest file."""

  option_name = 'sync'

  def __init__(self, bot_id, options, build_config, success):
    bs.BuilderStage.__init__(self, bot_id, options, build_config)
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

  def WasBuildSuccessful(self):
    if not ManifestVersionedSyncStage.manifest_manager:
      return True

    super(LKGMCandidateSyncCompletionStage, self)._PerformStage()
    if not self._build_config['master']:
      return True

    if self._options.debug:
      builders = []
    else:
      builders = self._GetImportantBuildersForMaster(cbuildbot_config.config)

    statuses = ManifestVersionedSyncStage.manifest_manager.GetBuildersStatus(
        builders, os.path.join(self._build_root, constants.VERSION_FILE))
    success = True
    for builder in builders:
      status = statuses[builder]
      if status != lkgm_manager.LKGMManager.STATUS_PASSED:
        cros_lib.Warning('Builder %s reported status %s' % (builder, status))
        success = False

    return success

  def HandleError(self):
    raise ImportantBuilderFailedException(
          'An important build failed with this manifest.')

  def HandleSuccess(self):
    # We only promote for the pfq, not chrome pfq.
    if (self._build_config['build_type'] == constants.PFQ_TYPE and
        self._build_config['master'] and self._tracking_branch == 'master' and
        ManifestVersionedSyncStage.manifest_manager != None):
      ManifestVersionedSyncStage.manifest_manager.PromoteCandidate()

  def _PerformStage(self):
    success = self.WasBuildSuccessful()
    if success:
      self.HandleSuccess()
    else:
      self.HandleError()


class CommitQueueCompletionStage(LKGMCandidateSyncCompletionStage):
  """Commits or reports errors to CL's that failed to be validated."""
  def HandleSuccess(self):
    if self._build_config['master']:
      CommitQueueSyncStage.pool.SubmitPool()
      # TODO(sosa): Generate a manifest after we submit -- new LKGM.

  def HandleError(self):
    if self._build_config['master']:
      CommitQueueSyncStage.pool.HandleValidationFailure()
      super(CommitQueueCompletionStage, self).HandleError()


class RefreshPackageStatusStage(bs.BuilderStage):
  """Stage for refreshing Portage package status in online spreadsheet."""
  def _PerformStage(self):
    # If board is a string, convert to list.
    boards = self._ListifyBoard(self._build_config['board'])
    commands.RefreshPackageStatus(buildroot=self._build_root,
                                  boards=boards, debug=self._options.debug)


class BuildBoardStage(bs.BuilderStage):
  """Stage that is responsible for building host pkgs and setting up a board."""

  option_name = 'build'

  def _PerformStage(self):
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
    else:
      commands.RunChrootUpgradeHooks(self._build_root)

    # If board is a string, convert to list.
    board = self._ListifyBoard(self._build_config['board'])

    # Iterate through boards to setup.
    for board_to_build in board:
      # Only build the board if the directory does not exist.
      board_path = os.path.join(chroot_path, 'build', board_to_build)
      if os.path.isdir(board_path):
        continue

      env = {}
      if self._build_config['gcc_46']:
        env['GCC_PV'] = '4.6.0'

      if self._options.clobber:
        env['IGNORE_PREFLIGHT_BINHOST'] = '1'

      latest_toolchain = self._build_config['latest_toolchain']

      commands.SetupBoard(self._build_root,
                          board=board_to_build,
                          fast=self._build_config['fast'],
                          usepkg=self._build_config['usepkg_setup_board'],
                          latest_toolchain=latest_toolchain,
                          extra_env=env,
                          profile=self._options.profile or
                            self._build_config['profile'])



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
          self._build_root, self._tracking_branch,
          self._chrome_rev, self._build_config['board'],
          chrome_root=self._options.chrome_root,
          chrome_version=self._options.chrome_version)

    # Perform other uprevs.
    if self._build_config['uprev']:
      overlays, _ = self._ExtractOverlays()
      commands.UprevPackages(self._build_root,
                             self._build_config['board'],
                             overlays)
    elif self._chrome_rev and not chrome_atom_to_build:
      # TODO(sosa): Do this in a better way.
      sys.exit(0)


class BuildTargetStage(bs.BuilderStage):
  """This stage builds Chromium OS for a target.

  Specifically, we build Chromium OS packages and perform imaging to get
  the images we want per the build spec."""

  option_name = 'build'

  def _PerformStage(self):
    build_autotest = (self._build_config['build_tests'] and
                      self._options.tests)
    env = {}
    if self._build_config.get('useflags'):
      env['USE'] = ' '.join(self._build_config['useflags'])

    if self._options.chrome_root:
      env['CHROME_ORIGIN'] = 'LOCAL_SOURCE'

    if self._options.clobber:
      env['IGNORE_PREFLIGHT_BINHOST'] = '1'

    # If we are using ToT toolchain, don't attempt to update
    # the toolchain during build_packages.
    skip_toolchain_update = self._build_config['latest_toolchain']

    commands.Build(self._build_root,
                   self._build_config['board'],
                   build_autotest=build_autotest,
                   skip_toolchain_update=skip_toolchain_update,
                   fast=self._build_config['fast'],
                   usepkg=self._build_config['usepkg_build_packages'],
                   nowithdebug=self._build_config['nowithdebug'],
                   extra_env=env)

    # We only build base, dev, and test images from this stage.
    images_can_build = set(['base', 'dev', 'test'])
    images_to_build = set(self._build_config['images']).intersection(
        images_can_build)

    commands.BuildImage(self._build_root,
                        self._build_config['board'],
                        list(images_to_build),
                        extra_env=env)

    if self._build_config['vm_tests']:
      commands.BuildVMImageForTesting(self._build_root,
                                      self._build_config['board'],
                                      extra_env=env)

    # Update link to latest image.
    latest_image = os.readlink(self.GetImageDirSymlink('latest'))
    cbuildbot_image_link = self.GetImageDirSymlink()
    if os.path.lexists(cbuildbot_image_link):
      os.remove(cbuildbot_image_link)

    os.symlink(latest_image, cbuildbot_image_link)


class UnitTestStage(bs.BuilderStage):
  """Run unit tests."""

  option_name = 'tests'

  def _PerformStage(self):
    if self._build_config['unittests'] and self._options.tests:
      commands.RunUnitTests(self._build_root,
                            self._build_config['board'],
                            full=(not self._build_config['quick_unit']),
                            nowithdebug=self._build_config['nowithdebug'])


class VMTestStage(bs.BuilderStage):
  """Run autotests in a virtual machine."""

  option_name = 'tests'

  def __init__(self, bot_id, options, build_config, archive_stage):
    super(VMTestStage, self).__init__(bot_id, options, build_config)
    self._archive_stage = archive_stage

  def _CreateTestRoot(self):
    """Returns a temporary directory for test results in chroot.

    Returns:
      Returns relative path from chroot rather than whole path.
    """
    # Create test directory within tmp in chroot.
    chroot = os.path.join(self._build_root, 'chroot')
    chroot_tmp = os.path.join(chroot, 'tmp')
    test_root = tempfile.mkdtemp(prefix='cbuildbot', dir=chroot_tmp)

    # Relative directory.
    (_, _, relative_path) = test_root.partition(chroot)
    return relative_path

  def _PerformStage(self):
    # VM tests should run with higher priority than other tasks
    # because they are usually the bottleneck, and don't use much CPU.
    commands.SetNiceness(foreground=True)

    # Build autotest tarball, which is used in archive step.
    if self._build_config['archive_build_debug']:
      filename = None
      try:
        filename = commands.BuildAutotestTarball(self._build_root,
                                                 self._build_config['board'],
                                                 self.GetImageDirSymlink())
      finally:
        if self._archive_stage:
          self._archive_stage.AutotestTarballReady(filename)

    # These directories are used later to archive test artifacts.
    test_results_dir = None
    payloads_dir = None
    try:
      if self._build_config['vm_tests'] and self._options.tests:
        # Payloads dir is not in the chroot as ctest archives them outside of
        # the chroot.
        payloads_dir = tempfile.mkdtemp(prefix='cbuildbot')

        test_results_dir = self._CreateTestRoot()
        commands.RunTestSuite(self._build_root,
                              self._build_config['board'],
                              self.GetImageDirSymlink(),
                              os.path.join(test_results_dir,
                                           'test_harness'),
                              test_type=self._build_config['vm_tests'],
                              nplus1_archive_dir=payloads_dir,
                              build_config=self._bot_id)

        if self._build_config['chrome_tests']:
          commands.RunChromeSuite(self._build_root,
                                  self._build_config['board'],
                                  self.GetImageDirSymlink(),
                                  os.path.join(test_results_dir,
                                               'chrome_results'))

    except commands.TestException:
      raise bs.NonBacktraceBuildException()  # Suppress redundant output.
    finally:
      test_tarball = None
      if test_results_dir:
        test_tarball = commands.ArchiveTestResults(self._build_root,
                                                   test_results_dir)

      if self._archive_stage:
        self._archive_stage.UpdatePayloadsReady(payloads_dir)
        self._archive_stage.TestResultsReady(test_tarball)


class HWTestStage(NonHaltingBuilderStage):
  """Stage that performs testing on actual HW."""

  option_name = 'hw_tests'

  def _PerformStage(self):
    if not self._build_config['hw_tests']:
      return

    if self._options.remote_ip:
      ip = self._options.remote_ip
    elif self._build_config['remote_ip']:
      ip = self._build_config['remote_ip']
    else:
      raise Exception('Please specify remote_ip.')

    if self._build_config['hw_tests_reimage']:
      commands.UpdateRemoteHW(self._build_root,
                              self._build_config['board'],
                              self.GetImageDirSymlink(),
                              ip)

    for test in self._build_config['hw_tests']:
      test_name = test[0]
      test_args = test[1:]

      commands.RunRemoteTest(self._build_root,
                             self._build_config['board'],
                             ip,
                             test_name,
                             test_args)


class SDKTestStage(bs.BuilderStage):
  """Stage that performs testing an SDK created in a previous stage"""
  def _PerformStage(self):
    tarball_location = os.path.join(self._build_root, 'built-sdk.tbz2')
    board_location = os.path.join(self._build_root, 'chroot/build/amd64-host')

    # Create a tarball of the latest SDK.
    cmd = ['sudo', 'tar', '-jcf', tarball_location]
    excluded_paths = ('usr/lib/debug', 'usr/local/autotest', 'packages',
                      'tmp')
    for path in excluded_paths:
      cmd.append('--exclude=%s/*' % path)
    cmd.append('.')
    cros_lib.RunCommand(cmd, cwd=board_location)

    # Make sure the regular user has the permission to read.
    cmd = ['sudo', 'chmod', 'a+r', tarball_location]
    cros_lib.RunCommand(cmd, cwd=board_location)

    # Build a new SDK using the tarball.
    cmd = ['cros_sdk', '--download', '--chroot', 'new-sdk-chroot', '--replace',
           '--url', 'file://' + tarball_location]
    cros_lib.RunCommand(cmd, cwd=self._build_root)


class RemoteTestStatusStage(bs.BuilderStage):
  """Stage that performs testing steps."""

  option_name = 'remote_test_status'

  def _PerformStage(self):
    test_status_cmd = ['./crostools/get_test_status.py',
                       '--board=%s' % self._build_config['board'],
                       '--build=%s' % self._options.buildnumber]
    for job in self._options.remote_test_status.split(','):
      result = cros_lib.RunCommand(
          test_status_cmd + ['--category=%s' % job],
          redirect_stdout=True, print_cmd=False)
      # Emit annotations for buildbot status updates.
      print result.output


class ArchiveStage(NonHaltingBuilderStage):
  """Archives build and test artifacts for developer consumption."""

  option_name = 'archive'

  # This stage is intended to run in the background, in parallel with tests.
  # When the tests have completed, TestStageComplete method must be
  # called. (If no tests are run, the TestStageComplete method must be
  # called with 'None'.)
  def __init__(self, bot_id, options, build_config):
    super(ArchiveStage, self).__init__(bot_id, options, build_config)
    if build_config['gs_path'] == cbuildbot_config.GS_PATH_DEFAULT:
      self._gsutil_archive = 'gs://chromeos-image-archive/' + bot_id
    else:
      self._gsutil_archive = build_config['gs_path']

    image_id = os.readlink(self.GetImageDirSymlink())
    self._set_version = '%s-b%s' % (image_id, self._options.buildnumber)
    if self._options.buildbot:
      self._archive_root = '/var/www/archive'
    else:
      self._archive_root = os.path.join(self._build_root,
                                        'trybot_archive')

    self._bot_archive_root = os.path.join(self._archive_root, self._bot_id)
    self._autotest_tarball_queue = multiprocessing.Queue()
    self._test_results_queue = multiprocessing.Queue()
    self._update_payloads_queue = multiprocessing.Queue()
    self._breakpad_symbols_queue = multiprocessing.Queue()

  def AutotestTarballReady(self, autotest_tarball):
    """Tell Archive Stage that autotest tarball is ready.

       Args:
         autotest_tarball: The filename of the autotest tarball.
    """
    self._autotest_tarball_queue.put(autotest_tarball)

  def UpdatePayloadsReady(self, update_payloads_dir):
    """Tell Archive Stage that test results are ready.

       Args:
         test_results: The test results tarball from the tests. If no tests
                       results are available, this should be set to None.
    """
    self._update_payloads_queue.put(update_payloads_dir)

  def TestResultsReady(self, test_results):
    """Tell Archive Stage that test results are ready.

       Args:
         test_results: The test results tarball from the tests. If no tests
                       results are available, this should be set to None.
    """
    self._test_results_queue.put(test_results)

  def TestStageExited(self):
    """Tell Archive Stage that test stage has exited.

    If the test phase failed strangely, this failsafe ensures that the archive
    stage doesn't sit around waiting for data.
    """
    self._autotest_tarball_queue.put(None)
    self._test_results_queue.put(None)
    self._update_payloads_queue.put(None)

  def _BreakpadSymbolsGenerated(self, success):
    """Signal that breakpad symbols have been generated.

    Arguments:
      success: True to indicate the symbols were generated, else False.
    """
    if not success:
      cros_lib.Warning('Failed to generate breakpad symbols.')
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
    if not self._options.buildbot:
      return self._GetArchivePath()
    elif self._gsutil_archive:
      upload_location = self._GetGSUploadLocation()
      url_prefix = 'https://sandbox.google.com/storage/'
      return upload_location.replace('gs://', url_prefix)
    else:
      # 'http://botname/archive/bot_id/version'
      return 'http://%s/archive/%s/%s' % (socket.getfqdn(), self._bot_id,
                                          self._set_version)

  def _GetGSUploadLocation(self):
    """Get the Google Storage location where we should upload artifacts."""
    if self._gsutil_archive:
      return '%s/%s' % (self._gsutil_archive, self._set_version)
    else:
      return None

  def _GetArchivePath(self):
    return os.path.join(self._bot_archive_root, self._set_version)

  def _GetAutotestTarball(self):
    """Get the path to the autotest tarball."""
    cros_lib.Info('Waiting for autotest tarball...')
    autotest_tarball = self._autotest_tarball_queue.get()
    if autotest_tarball:
      cros_lib.Info('Found autotest tarball at %s...' % autotest_tarball)
    else:
      cros_lib.Info('No autotest tarball.')
    return autotest_tarball

  def _GetUpdatePayloads(self):
    """Get the path to the directory containing update payloads."""
    cros_lib.Info('Waiting for update payloads dir...')
    update_payloads_dir = self._update_payloads_queue.get()
    if update_payloads_dir:
      cros_lib.Info('Found update payloads at %s...' % update_payloads_dir)
    else:
      cros_lib.Info('No update payloads found.')
    return update_payloads_dir

  def _GetTestResults(self):
    """Get the path to the test results tarball."""
    cros_lib.Info('Waiting for test results dir...')
    test_tarball = self._test_results_queue.get()
    if test_tarball:
      cros_lib.Info('Found test results tarball at %s...' % test_tarball)
    else:
      cros_lib.Info('No test results.')
    return test_tarball

  def _SetupArchivePath(self):
    """Create a fresh directory for archiving a build."""
    archive_path = self._GetArchivePath()
    if not self._options.buildbot:
      # Trybot: Clear artifacts from all previous runs.
      shutil.rmtree(self._archive_root, ignore_errors=True)
    else:
      # Buildbot: Clear out any leftover build artifacts, if present.
      shutil.rmtree(archive_path, ignore_errors=True)

    os.makedirs(archive_path)

    return archive_path

  def _PerformStage(self):
    buildroot = self._build_root
    config = self._build_config
    board = config['board']
    debug = self._options.debug
    upload_url = self._GetGSUploadLocation()
    archive_path = self._SetupArchivePath()
    image_dir = self.GetImageDirSymlink()
    extra_env = {}
    if config['useflags']:
      extra_env['USE'] = ' '.join(config['useflags'])

    # The following three functions are run in parallel.
    #  1. UploadUpdatePayloads: Upload update payloads from test phase.
    #  2. UploadTestResults: Upload results from test phase.
    #  3. ArchiveDebugSymbols: Generate and upload debug symbols.
    #  4. BuildAndArchiveAllImages: Build and archive images.

    def UploadUpdatePayloads():
      """Uploads update payloads when ready."""
      update_payloads_dir = self._GetUpdatePayloads()
      if update_payloads_dir:
        for payload in os.listdir(update_payloads_dir):
          full_path = os.path.join(update_payloads_dir, payload)
          filename = commands.ArchiveFile(full_path, archive_path)
          commands.UploadArchivedFile(archive_path, upload_url,
                                      filename, debug)

    def UploadTestResults():
      """Upload test results when they are ready."""
      test_results = self._GetTestResults()
      if test_results:
        if self._WaitForBreakpadSymbols():
          filenames = commands.GenerateMinidumpStackTraces(buildroot,
                                                           board, test_results,
                                                           archive_path)
          for filename in filenames:
            commands.UploadArchivedFile(archive_path, upload_url,
                                        filename, debug)
        filename = commands.ArchiveFile(test_results, archive_path)
        commands.UploadArchivedFile(archive_path, upload_url, filename, debug)

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
        commands.UploadArchivedFile(archive_path, upload_url, filename, debug)

      if not debug and config['upload_symbols']:
        commands.UploadSymbols(buildroot,
                               board=board,
                               official=config['chromeos_official'])

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
        filename = commands.BuildFactoryZip(archive_path, image_root)
        commands.UploadArchivedFile(archive_path, upload_url, filename, debug)

    def ArchiveRegularImages():
      """Build and archive image.zip and the hwqual image."""

      if config['archive_build_debug']:
        # Wait for the autotest tarball to be ready. This tarball will be
        # included in the zip file.
        self._GetAutotestTarball()

      # Zip up everything in the image directory.
      filename = commands.BuildImageZip(archive_path, image_dir)

      # Upload image.zip to Google Storage.
      commands.UploadArchivedFile(archive_path, upload_url, filename, debug)

      if config['chromeos_official']:
        # Build hwqual image and upload to Google Storage.
        version = os.path.basename(os.path.realpath(image_dir))
        hwqual_name = 'chromeos-hwqual-%s-%s' % (board, version)
        filename = commands.ArchiveHWQual(buildroot, hwqual_name, archive_path)
        commands.UploadArchivedFile(archive_path, upload_url, filename, debug)

      # Archive au-generator.zip.
      filename = 'au-generator.zip'
      shutil.copy(os.path.join(image_dir, filename), archive_path)
      commands.UploadArchivedFile(archive_path, upload_url, filename, debug)

    def BuildAndArchiveAllImages():
      # If we're an official build, generate the recovery image. To conserve
      # loop devices, we try to only run one instance of build_image at a
      # time. TODO(davidjames): Move the image generation out of the archive
      # stage.
      if config['chromeos_official']:
        commands.BuildRecoveryImage(buildroot, board, image_dir, extra_env)

      background.RunParallelSteps([BuildAndArchiveFactoryImages,
                                   ArchiveRegularImages])

    background.RunParallelSteps([
      UploadUpdatePayloads,
      UploadTestResults,
      ArchiveDebugSymbols,
      BuildAndArchiveAllImages])

    # Update and upload LATEST file.
    commands.UpdateLatestFile(self._bot_archive_root, self._set_version)
    commands.UploadArchivedFile(self._bot_archive_root, self._gsutil_archive,
                                'LATEST', debug)

    # Now that all data has been generated, we can upload the final result to
    # the image server.
    # TODO: When we support branches fully, the friendly name of the branch
    # needs to be used with PushImages
    if not debug and config['push_image']:
      commands.PushImages(buildroot,
                          board=board,
                          branch_name='master',
                          archive_dir=archive_path,
                          profile=self._options.profile or
                            self._build_config['profile'])


    commands.RemoveOldArchives(self._bot_archive_root,
                               self._options.max_archive_builds)


class UploadPrebuiltsStage(bs.BuilderStage):
  """Uploads binaries generated by this build for developer use."""

  option_name = 'prebuilts'

  def _PerformStage(self):
    if not self._build_config['prebuilts']:
      return

    manifest_manager = ManifestVersionedSyncStage.manifest_manager
    overlay_config = self._build_config['overlays']
    prebuilt_type = self._prebuilt_type
    board = self._build_config['board']
    binhost_bucket = self._build_config['binhost_bucket']
    binhost_key = self._build_config['binhost_key']
    binhost_base_url = self._build_config['binhost_base_url']
    use_binhost_package_file = self._build_config['use_binhost_package_file']
    git_sync = self._build_config['git_sync']
    binhosts = []
    extra_args = []

    if manifest_manager and manifest_manager.current_version:
      version = manifest_manager.current_version
      extra_args = ['--set-version', version]

    if prebuilt_type == constants.CHROOT_BUILDER_TYPE:
      board = 'amd64'
    elif prebuilt_type not in [constants.BUILD_FROM_SOURCE_TYPE,
                               constants.CANARY_TYPE]:
      assert prebuilt_type in (constants.PFQ_TYPE, constants.CHROME_PFQ_TYPE)

      overlays = self._build_config['overlays']
      if self._build_config['master']:
        extra_args.append('--sync-binhost-conf')

        # Update binhost conf files for slaves.
        if manifest_manager:
          config = cbuildbot_config.config
          builders = self._GetImportantBuildersForMaster(config)
          for builder in builders:
            builder_config = config[builder]
            if not builder_config['master']:
              slave_board = builder_config['board']
              extra_args.extend(['--slave-board', slave_board])

      # Pre-flight queues should upload host preflight prebuilts.
      if prebuilt_type == constants.PFQ_TYPE and overlays == 'public':
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

    # Upload prebuilts.
    commands.UploadPrebuilts(
        self._build_root, board, overlay_config, prebuilt_type,
        self._chrome_rev, self._options.buildnumber,
        binhost_bucket, binhost_key, binhost_base_url,
        use_binhost_package_file, git_sync, extra_args)


class PublishUprevChangesStage(NonHaltingBuilderStage):
  """Makes uprev changes from pfq live for developers."""
  def _PerformStage(self):
    _, push_overlays = self._ExtractOverlays()
    if push_overlays:
      commands.UprevPush(self._build_root,
                         self._build_config['board'],
                         push_overlays,
                         self._options.debug)
