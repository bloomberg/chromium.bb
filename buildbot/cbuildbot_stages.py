# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing the various stages that a builder runs."""

import multiprocessing
import os
import Queue
import re
import shutil
import socket
import sys
import tempfile
import time
import traceback

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
PUBLIC_OVERLAY = '%(buildroot)s/src/third_party/chromiumos-overlay'
_CROS_ARCHIVE_URL = 'CROS_ARCHIVE_URL'
OVERLAY_LIST_CMD = '%(buildroot)s/src/platform/dev/host/cros_overlay_list'
_MAX_ARCHIVED_BUILDS = 3
_PRINT_INTERVAL = 1

class BuildException(Exception):
  pass

class BuilderStage(object):
  """Parent class for stages to be performed by a builder."""
  name_stage_re = re.compile('(\w+)Stage')

  # TODO(sosa): Remove these once we have a SEND/RECIEVE IPC mechanism
  # implemented.
  overlays = None
  push_overlays = None

  # Class variable that stores the branch to build and test
  _tracking_branch = None

  @staticmethod
  def SetTrackingBranch(tracking_branch):
    BuilderStage._tracking_branch = tracking_branch

  def __init__(self, bot_id, options, build_config):
    self._bot_id = bot_id
    self._options = options
    self._build_config = build_config
    self._prebuilt_type = None
    self.name = self.name_stage_re.match(self.__class__.__name__).group(1)
    self._ExtractVariables()
    repo_dir = os.path.join(self._build_root, '.repo')

    # Determine correct chrome_rev.
    self._chrome_rev = self._build_config['chrome_rev']
    if self._options.chrome_rev: self._chrome_rev = self._options.chrome_rev

    if not self._options.clobber and os.path.isdir(repo_dir):
      self._ExtractOverlays()

  def _ExtractVariables(self):
    """Extracts common variables from build config and options into class."""
    self._build_root = os.path.abspath(self._options.buildroot)
    if self._options.prebuilts and self._build_config['prebuilts']:
      self._prebuilt_type = self._build_config['build_type']

  def _ExtractOverlays(self):
    """Extracts list of overlays into class."""
    if not BuilderStage.overlays or not BuilderStage.push_overlays:
      overlays = self._ResolveOverlays(self._build_config['overlays'])
      push_overlays = self._ResolveOverlays(self._build_config['push_overlays'])

      # Sanity checks.
      # We cannot push to overlays that we don't rev.
      assert set(push_overlays).issubset(set(overlays))
      # Either has to be a master or not have any push overlays.
      assert self._build_config['master'] or not push_overlays

      BuilderStage.overlays = overlays
      BuilderStage.push_overlays = push_overlays

  def _ListifyBoard(self, board):
    """Return list of boards from either str or list |board|."""
    boards = None
    if isinstance(board, str):
      boards = [board]
    else:
      boards = board

    assert isinstance(boards, list), 'Board was neither an array or a string.'
    return boards

  def _ResolveOverlays(self, overlays):
    """Return the list of overlays to use for a given buildbot.

    Args:
      overlays: A string describing which overlays you want.
                'private': Just the private overlay.
                'public': Just the public overlay.
                'both': Both the public and private overlays.
    """
    cmd = OVERLAY_LIST_CMD % {'buildroot': self._build_root}
    # Check in case we haven't checked out the source yet.
    if not os.path.exists(cmd):
      return []

    public_overlays = cros_lib.RunCommand([cmd, '--all_boards', '--noprivate'],
                                          redirect_stdout=True,
                                          print_cmd=False).output.split()
    private_overlays = cros_lib.RunCommand([cmd, '--all_boards', '--nopublic'],
                                           redirect_stdout=True,
                                           print_cmd=False).output.split()

    # TODO(davidjames): cros_overlay_list should include chromiumos-overlay in
    #                   its list of public overlays. But it doesn't yet...
    public_overlays.append(PUBLIC_OVERLAY % {'buildroot': self._build_root})

    if overlays == 'private':
      paths = private_overlays
    elif overlays == 'public':
      paths = public_overlays
    elif overlays == 'both':
      paths = public_overlays + private_overlays
    else:
      cros_lib.Info('No overlays found.')
      paths = []

    return paths

  def _PrintLoudly(self, msg):
    """Prints a msg with loudly."""

    border_line = '*' * 60
    edge = '*' * 2

    print border_line

    msg_lines = msg.split('\n')

    # If the last line is whitespace only drop it.
    if not msg_lines[-1].rstrip():
      del msg_lines[-1]

    for msg_line in msg_lines:
      print '%s %s' % (edge, msg_line)

    print border_line

  def _GetPortageEnvVar(self, envvar, board):
    """Get a portage environment variable for the configuration's board.

    envvar: The environment variable to get. E.g. 'PORTAGE_BINHOST'.

    Returns:
      The value of the environment variable, as a string. If no such variable
      can be found, return the empty string.
    """
    cwd = os.path.join(self._build_root, 'src', 'scripts')
    if board:
      portageq = 'portageq-%s' % board
    else:
      portageq = 'portageq'
    binhost = cros_lib.OldRunCommand(
        [portageq, 'envvar', envvar], cwd=cwd, redirect_stdout=True,
        enter_chroot=True, error_ok=True)
    return binhost.rstrip('\n')

  def _GetImportantBuildersForMaster(self, config):
    """Gets the important builds corresponding to this master builder.

    Given that we are a master builder, find all corresponding slaves that
    are important to me.  These are those builders that share the same
    build_type and manifest_version url.
    """
    builders = []
    build_type = self._build_config['build_type']
    overlay_config = self._build_config['overlays']
    use_manifest_version = self._build_config['manifest_version']
    for build_name, config in config.iteritems():
      if (config['important'] and config['build_type'] == build_type and
          config['chrome_rev'] == self._chrome_rev and
          config['overlays'] == overlay_config and
          config['manifest_version'] == use_manifest_version):
        builders.append(build_name)

    return builders

  def _Begin(self):
    """Can be overridden.  Called before a stage is performed."""

    # Tell the buildbot we are starting a new step for the waterfall
    print '@@@BUILD_STEP %s@@@\n' % self.name

    self._PrintLoudly('Start Stage %s - %s\n\n%s' % (
        self.name, time.strftime('%H:%M:%S'), self.__doc__))

  def _Finish(self):
    """Can be overridden.  Called after a stage has been performed."""
    self._PrintLoudly('Finished Stage %s - %s' %
                      (self.name, time.strftime('%H:%M:%S')))

  def _PerformStage(self):
    """Subclassed stages must override this function to perform what they want
    to be done.
    """
    pass

  def _HandleStageException(self, exception):
    """Called when _PerformStages throws an exception.  Can be overriden.

    Should return result, description.  Description should be None if result
    is not an exception.
    """
    # Tell the user about the exception, and record it
    print '@@@STEP_FAILURE@@@'
    description = traceback.format_exc()
    print >> sys.stderr, description
    return exception, description

  def GetImageDirSymlink(self, pointer='latest-cbuildbot'):
    """Get the location of the current image."""
    buildroot, board = self._options.buildroot, self._build_config['board']
    return os.path.join(buildroot, 'src', 'build', 'images', board, pointer)

  def Run(self):
    """Have the builder execute the stage."""

    if results_lib.Results.PreviouslyCompleted(self.name):
      self._PrintLoudly('Skipping Stage %s' % self.name)
      results_lib.Results.Record(self.name, results_lib.Results.SKIPPED)
      return

    start_time = time.time()

    # Set default values
    result = results_lib.Results.SUCCESS
    description = None

    self._Begin()
    try:
      self._PerformStage()
    except Exception as e:
      # Tell the build bot this step failed for the waterfall
      result, description = self._HandleStageException(e)
      raise BuildException()
    finally:
      elapsed_time = time.time() - start_time
      results_lib.Results.Record(self.name, result, description,
                                 time=elapsed_time)
      self._Finish()


class NonHaltingBuilderStage(BuilderStage):
  """Build stage that fails a build but finishes the other steps."""
  def Run(self):
    try:
      super(NonHaltingBuilderStage, self).Run()
    except BuildException:
      pass


class ForgivingBuilderStage(NonHaltingBuilderStage):
  """Build stage that turns a build step red but not a build."""
  def _HandleStageException(self, exception):
    """Override and don't set status to FAIL but FORGIVEN instead."""
    print '@@@STEP_WARNINGS@@@'
    description = traceback.format_exc()
    print >> sys.stderr, description
    return results_lib.Results.FORGIVEN, None


class CleanUpStage(BuilderStage):
  """Stages that cleans up build artifacts from previous runs.

  This stage cleans up previous KVM state, temporary git commits,
  clobbers, and wipes tmp inside the chroot.
  """
  def _PerformStage(self):
    if not self._options.buildbot and self._options.clobber:
      if not commands.ValidateClobber(self._build_root):
        sys.exit(0)

    if self._options.clobber or not os.path.exists(
        os.path.join(self._build_root, '.repo')):
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


class SyncStage(BuilderStage):
  """Stage that performs syncing for the builder."""

  def _PerformStage(self):
    commands.ManifestCheckout(self._build_root, self._tracking_branch,
                              repository.RepoRepository.DEFAULT_MANIFEST,
                              self._build_config['git_url'])

    # Check that all overlays can be found.
    self._ExtractOverlays() # Our list of overlays are from pre-sync, refresh
    for path in BuilderStage.overlays:
      assert os.path.isdir(path), 'Missing overlay: %s' % path


class PatchChangesStage(BuilderStage):
  """Stage that patches a set of Gerrit changes to the buildroot source tree."""
  def __init__(self, bot_id, options, build_config, gerrit_patches,
               local_patches):
    """Construct a PatchChangesStage.

    Args:
      bot_id, options, build_config: See arguments to BuilderStage.__init__()
      gerrit_patches: A list of cros_patch.GerritPatch objects to apply.
                      Cannot be None.
      local_patches: A list cros_patch.LocalPatch objects to apply. Cannot be
                     None.
    """
    BuilderStage.__init__(self, bot_id, options, build_config)
    assert(gerrit_patches is not None and local_patches is not None)
    self.gerrit_patches = gerrit_patches
    self.local_patches = local_patches

  def _PerformStage(self):
    for patch in self.gerrit_patches + self.local_patches:
      patch.Apply(self._build_root)

    if self.local_patches:
      patch_root = os.path.dirname(self.local_patches[0].patch_dir)
      cros_patch.RemovePatchRoot(patch_root)


class ManifestVersionedSyncStage(BuilderStage):
  """Stage that generates a unique manifest file, and sync's to it."""

  manifest_manager = None

  def _GetManifestVersionsRepoUrl(self):
    if cbuildbot_config._IsInternalBuild(self._build_config['git_url']):
      return cbuildbot_config.MANIFEST_VERSIONS_INT_URL
    else:
      return cbuildbot_config.MANIFEST_VERSIONS_URL

  def InitializeManifestManager(self):
    """Initializes a manager that manages manifests for associated stages."""
    increment = 'branch' if self._tracking_branch == 'master' else 'patch'

    ManifestVersionedSyncStage.manifest_manager = \
        manifest_version.BuildSpecsManager(
            source_dir=self._build_root,
            checkout_repo=self._build_config['git_url'],
            manifest_repo=self._GetManifestVersionsRepoUrl(),
            branch=self._tracking_branch,
            build_name=self._bot_id,
            incr_type=increment,
            dry_run=self._options.debug)

  def GetNextManifest(self):
    """Uses the initialized manifest manager to get the next manifest."""
    assert self.manifest_manager, \
        'Must run GetStageManager before checkout out build.'
    return self.manifest_manager.GetNextBuildSpec(
        force_version=self._options.force_version, latest=True)


  def _PerformStage(self):
    self.InitializeManifestManager()
    next_manifest = self.GetNextManifest()
    if not next_manifest:
      print 'Manifest Revision: Nothing to build!'
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

    # Check that all overlays can be found.
    self._ExtractOverlays()
    for path in BuilderStage.overlays:
      assert os.path.isdir(path), 'Missing overlay: %s' % path


class LKGMCandidateSyncStage(ManifestVersionedSyncStage):
  """Stage that generates a unique manifest file candidate, and sync's to it."""

  def InitializeManifestManager(self):
    """Override: Creates an LKGMManager rather than a ManifestManager."""
    ManifestVersionedSyncStage.manifest_manager = lkgm_manager.LKGMManager(
        source_dir=self._build_root,
        checkout_repo=self._build_config['git_url'],
        manifest_repo=self._GetManifestVersionsRepoUrl(),
        branch=self._tracking_branch,
        build_name=self._bot_id,
        build_type=self._build_config['build_type'],
        dry_run=self._options.debug)

  def GetNextManifest(self):
    """Gets the next manifest using LKGM logic."""
    assert self.manifest_manager, \
        'Must run InitializeManifestManager before we can get a manifest.'
    assert isinstance(self.manifest_manager, lkgm_manager.LKGMManager), \
        'Manifest manager instantiated with wrong class.'

    if self._build_config['master']:
      return self.manifest_manager.CreateNewCandidate(
          force_version=self._options.force_version)
    else:
      return self.manifest_manager.GetLatestCandidate(
          force_version=self._options.force_version)

  def _PerformStage(self):
    """Performs normal stage and prints blamelist at end."""
    super(LKGMCandidateSyncStage, self)._PerformStage()
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

  def GetNextManifest(self):
    """Gets the next manifest using LKGM logic."""
    assert self.manifest_manager, \
        'Must run InitializeManifestManager before we can get a manifest.'
    assert isinstance(self.manifest_manager, lkgm_manager.LKGMManager), \
        'Manifest manager instantiated with wrong class.'

    internal = cbuildbot_config._IsInternalBuild(self._build_config['git_url'])
    if self._build_config['master']:
      CommitQueueSyncStage.pool = validation_pool.ValidationPool.AcquirePool(
        self._tracking_branch, internal, self._build_root)
      return self.manifest_manager.CreateNewCandidate(
          force_version=self._options.force_version, patches=self.pool.changes)
    else:
      manifest = self.manifest_manager.GetLatestCandidate(
          force_version=self._options.force_version)
      CommitQueueSyncStage.pool = \
          validation_pool.ValidationPool.AcquirePoolFromManifest(
              manifest, internal, self._build_root)
      return manifest

  def _PerformStage(self):
    """Performs normal stage and prints blamelist at end."""
    super(LKGMCandidateSyncStage, self)._PerformStage()
    self.pool.ApplyPoolIntoRepo(self._build_root)


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

    repository.CloneGitRepo(manifests_dir, self._GetManifestVersionsRepoUrl())
    return lkgm_manager.LKGMManager.GetAbsolutePathToLKGM()


class ManifestVersionedSyncCompletionStage(ForgivingBuilderStage):
  """Stage that records board specific results for a unique manifest file."""

  def __init__(self, bot_id, options, build_config, success):
    BuilderStage.__init__(self, bot_id, options, build_config)
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

    builders = self._GetImportantBuildersForMaster(cbuildbot_config.config)
    statuses = ManifestVersionedSyncStage.manifest_manager.GetBuildersStatus(
        builders)
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
        self._build_config['master']):
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


class RefreshPackageStatusStage(BuilderStage):
  """Stage for refreshing Portage package status in online spreadsheet."""
  def _PerformStage(self):
    # If board is a string, convert to list.
    boards = self._ListifyBoard(self._build_config['board'])
    commands.RefreshPackageStatus(buildroot=self._build_root,
                                  boards=boards, debug=self._options.debug)


class BuildBoardStage(BuilderStage):
  """Stage that is responsible for building host pkgs and setting up a board."""
  def _PerformStage(self):
    chroot_path = os.path.join(self._build_root, 'chroot')
    if not os.path.isdir(chroot_path) or self._build_config['chroot_replace']:
      commands.MakeChroot(
          buildroot=self._build_root,
          replace=self._build_config['chroot_replace'],
          use_sdk=self._build_config['use_sdk'])
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

      latest_toolchain = self._build_config['latest_toolchain']

      commands.SetupBoard(self._build_root,
                          board=board_to_build,
                          fast=self._build_config['fast'],
                          usepkg=self._build_config['usepkg_setup_board'],
                          latest_toolchain=latest_toolchain,
                          extra_env=env,
                          profile=self._options.profile or
                            self._build_config['profile'])



class UprevStage(BuilderStage):
  """Stage that uprevs Chromium OS packages that the builder intends to
  validate.
  """
  def _PerformStage(self):
    # Perform chrome uprev.
    chrome_atom_to_build = None
    if self._chrome_rev:
      chrome_atom_to_build = commands.MarkChromeAsStable(
          self._build_root, self._tracking_branch,
          self._chrome_rev, self._build_config['board'])

    # Perform other uprevs.
    if self._build_config['uprev']:
      commands.UprevPackages(self._build_root,
                             self._build_config['board'],
                             BuilderStage.overlays)
    elif self._chrome_rev and not chrome_atom_to_build:
      # TODO(sosa): Do this in a better way.
      sys.exit(0)


class BuildTargetStage(BuilderStage):
  """This stage builds Chromium OS for a target.

  Specifically, we build Chromium OS packages and perform imaging to get
  the images we want per the build spec."""
  def _PerformStage(self):
    build_autotest = (self._build_config['build_tests'] and
                      self._options.tests)
    env = {}
    if self._build_config.get('useflags'):
      env['USE'] = ' '.join(self._build_config['useflags'])

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

    if self._options.tests and (self._build_config['vm_tests'] or
                                self._options.hw_tests):
      mod_for_test = True
    else:
      mod_for_test = self._build_config['test_mod']

    commands.BuildImage(self._build_root,
                        self._build_config['board'],
                        mod_for_test,
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


class UnitTestStage(BuilderStage):
  """Run unit tests."""
  def _PerformStage(self):
    if self._build_config['unittests'] and self._options.tests:
      commands.RunUnitTests(self._build_root,
                            self._build_config['board'],
                            full=(not self._build_config['quick_unit']),
                            nowithdebug=self._build_config['nowithdebug'])


class VMTestStage(BuilderStage):
  """Run autotests in a virtual machine."""
  def __init__(self, bot_id, options, build_config, archive_stage):
    super(VMTestStage, self).__init__(bot_id, options, build_config)
    self._archive_stage = archive_stage

  def _CreateTestRoot(self):
    """Returns a temporary directory for test results in chroot.

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
    if self._build_config['test_mod']:
      filename = None
      try:
        filename = commands.BuildAutotestTarball(self._build_root,
                                                 self._build_config['board'],
                                                 self.GetImageDirSymlink())
      finally:
        if self._archive_stage:
          self._archive_stage.AutotestTarballReady(filename)

    test_results_dir = None
    try:
      if self._build_config['vm_tests'] and self._options.tests:
        test_results_dir = self._CreateTestRoot()
        commands.RunTestSuite(self._build_root,
                              self._build_config['board'],
                              self.GetImageDirSymlink(),
                              os.path.join(test_results_dir,
                                           'test_harness'),
                              full=(not self._build_config['quick_vm']))

        if self._build_config['chrome_tests']:
          commands.RunChromeSuite(self._build_root,
                                  self._build_config['board'],
                                  self.GetImageDirSymlink(),
                                  os.path.join(test_results_dir,
                                               'chrome_results'))
    finally:
      test_tarball = None
      if test_results_dir:
        test_tarball = commands.ArchiveTestResults(self._build_root,
                                                   test_results_dir)
      if self._archive_stage:
        self._archive_stage.TestResultsReady(test_tarball)


class TestHWStage(NonHaltingBuilderStage):
  """Stage that performs testing on actual HW."""
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


class TestSDKStage(BuilderStage):
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


class RemoteTestStatusStage(BuilderStage):
  """Stage that performs testing steps."""
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
    self._breakpad_symbols_queue = multiprocessing.Queue()

  def AutotestTarballReady(self, autotest_tarball):
    """Tell Archive Stage that autotest tarball is ready.

       Args:
         autotest_tarball: The filename of the autotest tarball.
    """
    self._autotest_tarball_queue.put(autotest_tarball)

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
    #  1. UploadTestResults: Upload results from test phase.
    #  2. ArchiveDebugSymbols: Generate and upload debug symbols.
    #  3. BuildAndArchiveAllImages: Build and archive images.

    def UploadTestResults():
      """Upload test results when they are ready."""
      test_results = self._GetTestResults()
      if test_results:
        if config['archive_build_debug'] and self._WaitForBreakpadSymbols():
          commands.GenerateMinidumpStackTraces(buildroot, board, test_results)
        filename = commands.ArchiveTestTarball(test_results, archive_path)
        commands.UploadArchivedFile(archive_path, upload_url, filename, debug)

    def ArchiveDebugSymbols():
      """Generate and upload debug symbols."""
      # TODO(thieule): Generate breakpad symbols if a crash was detected for
      # bots that do not normally generate breakpad symbols
      if config['archive_build_debug']:
        success = False
        try:
          commands.GenerateBreakpadSymbols(buildroot, board)
          success = True
        finally:
          self._BreakpadSymbolsGenerated(success)
        filename = commands.GenerateDebugTarball(
          buildroot, board, archive_path)
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
      if config['factory_test_mod']:
        alias = commands.BuildFactoryTestImage(buildroot, board, extra_env)
        factory_test_symlink = self.GetImageDirSymlink(alias)

      # Build factory install image and create a symlink to it.
      factory_install_symlink = None
      if config['factory_install_mod']:
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

      if config['test_mod']:
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
      UploadTestResults,
      ArchiveDebugSymbols,
      BuildAndArchiveAllImages])

    # Update and upload LATEST file.
    commands.UpdateLatestFile(self._bot_archive_root, self._gsutil_archive)
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
                          archive_dir=archive_path)

    commands.RemoveOldArchives(self._bot_archive_root, _MAX_ARCHIVED_BUILDS)


class UploadPrebuiltsStage(ForgivingBuilderStage):
  """Uploads binaries generated by this build for developer use."""
  def _PerformStage(self):
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
    elif prebuilt_type != constants.BUILD_FROM_SOURCE_TYPE:
      assert prebuilt_type in (constants.PFQ_TYPE, constants.CHROME_PFQ_TYPE)

      push_overlays = self._build_config['push_overlays']
      if self._build_config['master']:
        extra_args.append('--sync-binhost-conf')

        # Update binhost conf files for slaves.
        if manifest_manager:
          config = cbuildbot_config.config
          builders = self._GetImportantBuildersForMaster(config)
          for builder in builders:
            builder_config = config[builder]
            builder_board = builder_config['board']
            if not builder_config['master']:
              commands.UploadPrebuilts(
                  self._build_root, builder_board, overlay_config,
                  self._prebuilt_type, self._chrome_rev,
                  self._options.buildnumber, binhost_bucket, binhost_key,
                  binhost_base_url, use_binhost_package_file, git_sync,
                  extra_args + ['--skip-upload'])

        # Master pfq should upload host preflight prebuilts.
        if prebuilt_type == constants.PFQ_TYPE and push_overlays == 'public':
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
    commands.UprevPush(self._build_root,
                       self._build_config['board'],
                       BuilderStage.push_overlays,
                       self._options.debug)


