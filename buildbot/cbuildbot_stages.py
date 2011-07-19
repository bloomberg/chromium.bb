# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing the various stages that a builder runs."""

import datetime
import math
import os
import re
import shutil
import sys
import tempfile
import time
import traceback

from chromite.buildbot import cbuildbot_commands as commands
from chromite.buildbot import cbuildbot_config
from chromite.buildbot import constants
from chromite.buildbot import lkgm_manager
from chromite.buildbot import manifest_version
from chromite.buildbot import patch as cros_patch
from chromite.buildbot import repository
from chromite.lib import cros_build_lib as cros_lib

_FULL_BINHOST = 'FULL_BINHOST'
_PORTAGE_BINHOST = 'PORTAGE_BINHOST'
PUBLIC_OVERLAY = '%(buildroot)s/src/third_party/chromiumos-overlay'
_CROS_ARCHIVE_URL = 'CROS_ARCHIVE_URL'
OVERLAY_LIST_CMD = '%(buildroot)s/src/platform/dev/host/cros_overlay_list'

class BuildException(Exception):
  pass

class Results:
  """Static class that collects the results of our BuildStages as they run."""

  # List of results for all stages that's built up as we run. Members are of
  #  the form ('name', SUCCESS | SKIPPED | Exception, None | description)
  _results_log = []

  # Stages run in a previous run and restored. Stored as a list of
  # stage names.
  _previous = []

  # Stored in the results log for a stage skipped because it was previously
  # completed successfully.
  SUCCESS = "Stage was successful"
  SKIPPED = "Stage skipped because successful on previous run"
  FORGIVEN = "Stage failed but was optional"

  @classmethod
  def Clear(cls):
    """Clear existing stage results."""
    cls._results_log = []
    cls._previous = []

  @classmethod
  def PreviouslyCompleted(cls, name):
    """Check to see if this stage was previously completed.

       Returns:
         A boolean showing the stage was successful in the previous run.
    """
    return name in cls._previous

  @classmethod
  def Success(cls):
    """Return true if all stages so far have passed.

    This method returns true if all was successful or forgiven.
    """
    for entry in cls._results_log:
      _, result, _, _ = entry

      if not result in (cls.SUCCESS, cls.SKIPPED, cls.FORGIVEN):
        return False

    return True

  @classmethod
  def WasStageSuccessfulOrSkipped(cls, name):
    """Return true stage passed."""
    for entry in cls._results_log:
      entry, result, _, _ = entry

      if entry == name:
        if result not in (cls.SUCCESS, cls.SKIPPED):
          return False
        else:
          return True

    return False

  @classmethod
  def Record(cls, name, result, description=None, time=0):
    """Store off an additional stage result.

       Args:
         name: The name of the stage
         result:
           Result should be one of:
             Results.SUCCESS if the stage was successful.
             Results.SKIPPED if the stage was completed in a previous run.
             The exception the stage errored with.
         description:
           The textual backtrace of the exception, or None
    """
    cls._results_log.append((name, result, description, time))

  @classmethod
  def UpdateResult(cls, name, result, description=None):
    """Updates a stage result.with a different result.

       Args:
         name: The name of the stage
         result:
           Result should be one of:
             Results.SUCCESS if the stage was successful.
             Results.SKIPPED if the stage was completed in a previous run.
             The exception the stage errored with.
          description:
           The textual backtrace of the exception, or None
    """
    for index in range(len(cls._results_log)):
      if cls._results_log[index][0] == name: break
    else:
      return

    _, _, _, time = cls._results_log[index]
    cls._results_log[index] = name, result, description, time

  @classmethod
  def Get(cls):
    """Fetch stage results.

       Returns:
         A list with one entry per stage run with a result.
    """
    return cls._results_log

  @classmethod
  def GetPrevious(cls):
    """Fetch stage results.

       Returns:
         A list of stages names that were completed in a previous run.
    """
    return cls._previous

  @classmethod
  def SaveCompletedStages(cls, file):
    """Save out the successfully completed stages to the provided file."""
    for name, result, _, _ in cls._results_log:
      if result != cls.SUCCESS and result != cls.SKIPPED: break
      file.write(name)
      file.write('\n')

  @classmethod
  def RestoreCompletedStages(cls, file):
    """Load the successfully completed stages from the provided file."""
    # Read the file, and strip off the newlines
    cls._previous = [line.strip() for line in file.readlines()]

  @classmethod
  def Report(cls, file):
    """Generate a user friendly text display of the results data."""
    results = cls._results_log

    line = '*' * 60 + '\n'
    edge = '*' * 2

    if ManifestVersionedSyncStage.manifest_manager:
      file.write(line)
      file.write(edge +
                 ' RELEASE VERSION: ' +
                 ManifestVersionedSyncStage.manifest_manager.current_version +
                 '\n')

    file.write(line)
    file.write(edge + ' Stage Results\n')

    first_exception = None

    for name, result, description, run_time in results:
      timestr = datetime.timedelta(seconds=math.ceil(run_time))

      file.write(line)

      if result == cls.SUCCESS:
        # These was no error
        file.write('%s PASS %s (%s)\n' % (edge, name, timestr))

      elif result == cls.SKIPPED:
        # The stage was executed previously, and skipped this time
        file.write('%s %s previously completed\n' %
                   (edge, name))
      elif result == cls.FORGIVEN:
        # The stage was executed previously, and skipped this time
        file.write('%s FAILED BUT FORGIVEN %s (%s)\n' %
                   (edge, name, timestr))
      else:
        if type(result) in (cros_lib.RunCommandException,
                            cros_lib.RunCommandError):
          # If there was a RunCommand error, give just the command that
          # failed, not it's full argument list, since those are usually
          # too long.
          file.write('%s FAIL %s (%s) in %s\n' %
                     (edge, name, timestr, result.cmd[0]))
        else:
          # There was a normal error, give the type of exception
          file.write('%s FAIL %s (%s) with %s\n' %
                     (edge, name, timestr, type(result).__name__))

        if not first_exception:
          first_exception = description

    file.write(line)

    if BuilderStage.archive_url:
      file.write('%s BUILD ARTIFACTS FOR THIS BUILD CAN BE FOUND AT:\n' % edge)
      file.write('%s  %s\n' % (edge, BuilderStage.archive_url))
      file.write(line)

    if first_exception:
      file.write('\n')
      file.write('Build failed with:\n')
      file.write('\n')
      file.write(first_exception)


class BuilderStage(object):
  """Parent class for stages to be performed by a builder."""
  name_stage_re = re.compile('(\w+)Stage')

  # TODO(sosa): Remove these once we have a SEND/RECIEVE IPC mechanism
  # implemented.
  test_tarball = None
  overlays = None
  push_overlays = None
  archive_url = None

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
    manifest_version_url = self._build_config['manifest_version']
    for build_name, config in config.iteritems():
      if (config['important'] and config['build_type'] == build_type and
          config['overlays'] == overlay_config and
          config['manifest_version'] == manifest_version_url):
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

    if Results.PreviouslyCompleted(self.name):
      self._PrintLoudly('Skipping Stage %s' % self.name)
      Results.Record(self.name, Results.SKIPPED)
      return

    start_time = time.time()

    # Set default values
    result = Results.SUCCESS
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
      Results.Record(self.name, result, description, time=elapsed_time)
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
    return Results.FORGIVEN, None


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

  def InitializeManifestManager(self):
    """Initializes a manager that manages manifests for associated stages."""
    increment = 'branch' if self._tracking_branch == 'master' else 'patch'

    ManifestVersionedSyncStage.manifest_manager = \
        manifest_version.BuildSpecsManager(
            source_dir=self._build_root,
            checkout_repo=self._build_config['git_url'],
            manifest_repo=self._build_config['manifest_version'],
            branch=self._tracking_branch,
            build_name=self._bot_id,
            incr_type=increment,
            clobber=self._options.clobber,
            dry_run=self._options.debug)

  def GetNextManifest(self):
    """Uses the initialized manifest manager to get the next manifest."""
    assert self.manifest_manager, \
        'Must run GetStageManager before checkout out build.'
    return self.manifest_manager.GetNextBuildSpec(
        force_version=self._options.force_version, latest=True)

  def PostSyncStep(self):
    """Called at the end of the sync stage.  Base class version is noop."""
    pass

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

    self.PostSyncStep()


class LKGMCandidateSyncStage(ManifestVersionedSyncStage):
  """Stage that generates a unique manifest file candidate, and sync's to it."""

  def InitializeManifestManager(self):
    """Override: Creates an LKGMManager rather than a ManifestManager."""
    ManifestVersionedSyncStage.manifest_manager = lkgm_manager.LKGMManager(
        source_dir=self._build_root,
        checkout_repo=self._build_config['git_url'],
        manifest_repo=self._build_config['manifest_version'],
        branch=self._tracking_branch,
        build_name=self._bot_id,
        build_type=self._build_config['build_type'],
        clobber=self._options.clobber,
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
                            self._build_config['manifest_version'])
    return lkgm_manager.LKGMManager.GetAbsolutePathToLKGM()

  def PostSyncStep(self):
    """Override base class version.  Updates the prebuilt urls."""
    # The LKGM does not include the updated prebuilt url.
    # Update the url before running the build.
    if (not self._options.buildbot
        and (self._build_config['usepkg_chroot']
             or self._build_config['usepkg_setup_board']
             or self._build_config['usepkg_build_packages'])):
      commands.UploadPrebuilts(
          self._build_root, self._build_config['board'],
          self._build_config['overlays'],
          self._prebuilt_type, self._options.chrome_rev,
          self._options.buildnumber,
          ['--set-version', lkgm_manager.LKGMManager.GetLKGMVersion(),
           '--sync-binhost-conf', '--sync-host', '--skip-upload'])


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

  def _PerformStage(self):
    if not ManifestVersionedSyncStage.manifest_manager:
      return

    super(LKGMCandidateSyncCompletionStage, self)._PerformStage()
    if not self._build_config['master']:
      return

    builders = self._GetImportantBuildersForMaster(cbuildbot_config.config)
    statuses = ManifestVersionedSyncStage.manifest_manager.GetBuildersStatus(
        builders)
    success = True
    for builder in builders:
      status = statuses[builder]
      if status != lkgm_manager.LKGMManager.STATUS_PASSED:
        cros_lib.Warning('Builder %s reported status %s' % (builder, status))
        success = False

    if not success:
      raise ImportantBuilderFailedException(
          'An important build failed with this manifest.')
    elif self._build_config['build_type'] == 'binary':
      # We only promote for the pfq, not chrome pfq.
      ManifestVersionedSyncStage.manifest_manager.PromoteCandidate()


class BuildBoardStage(BuilderStage):
  """Stage that is responsible for building host pkgs and setting up a board."""
  def _PerformStage(self):
    chroot_path = os.path.join(self._build_root, 'chroot')
    if not os.path.isdir(chroot_path) or self._build_config['chroot_replace']:
      commands.MakeChroot(
          buildroot=self._build_root,
          replace=self._build_config['chroot_replace'],
          fast=self._build_config['fast'],
          usepkg=self._build_config['usepkg_chroot'])
    else:
      commands.RunChrootUpgradeHooks(self._build_root)

    # If board is a string, convert to array.
    if isinstance(self._build_config['board'], str):
      board = [self._build_config['board']]
    else:
      assert self._build_config['build_type'] == 'chroot', \
        'Board array requires chroot type.'
      board = self._build_config['board']

    assert isinstance(board, list), 'Board was neither an array or a string.'

    # Iterate through boards to setup.
    for board_to_build in board:
      # Only build the board if the directory does not exist.
      board_path = os.path.join(chroot_path, 'build', board_to_build)
      if os.path.isdir(board_path):
        continue

      env = {}
      if self._build_config['gcc_46']:
        env['GCC_PV'] = '4.6.0'
        env['USE'] = 'svn_sources'

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
    if self._options.chrome_rev:
      chrome_atom_to_build = commands.MarkChromeAsStable(
          self._build_root, self._tracking_branch,
          self._options.chrome_rev, self._build_config['board'])

    # Perform other uprevs.
    if self._build_config['uprev']:
      commands.UprevPackages(self._build_root,
                             self._build_config['board'],
                             BuilderStage.overlays)
    elif self._options.chrome_rev and not chrome_atom_to_build:
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
                   extra_env=env)

    if self._options.tests and (self._build_config['vm_tests'] or
                                self._options.hw_tests):
      mod_for_test = True
    else:
      mod_for_test = False

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


class TestStage(BuilderStage):
  """Stage that performs testing steps."""
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
    if self._build_config['unittests']:
      commands.RunUnitTests(self._build_root,
                            self._build_config['board'],
                            full=(not self._build_config['quick_unit']))

    if self._build_config['vm_tests']:
      test_results_dir = self._CreateTestRoot()
      try:
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
        BuilderStage.test_tarball = commands.ArchiveTestResults(
            self._build_root, test_results_dir)


class TestHWStage(BuilderStage):
  """Stage that performs testing on actual HW."""
  def _PerformStage(self):
    if self._options.remote_ip:
      ip = self._options.remote_ip
    elif self._build_config['remote_ip']:
      ip = self._build_config['remote_ip']
    else:
      raise Exception('Please specify remote_ip.')

    commands.UpdateRemoteHW(self._build_root,
                            self._build_config['board'],
                            self.GetImageDirSymlink(),
                            ip)
    commands.RemoteRunPyAuto(self._build_root,
                             self._build_config['board'],
                             ip,
                             internal_test=False)


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
  def _PerformStage(self):
    if self._options.buildbot:
      archive_path = '/var/www/archive'
    else:
      archive_path = os.path.join(self._build_root, 'trybot_archive')
      # Clear artifacts from previous run
      shutil.rmtree(archive_path, ignore_errors=True)

    BuilderStage.archive_url, archive_dir = commands.LegacyArchiveBuild(
        self._build_root, self._bot_id, self._build_config,
        self._options.buildnumber, BuilderStage.test_tarball, archive_path,
        self._options.debug)

    if not self._options.debug and self._build_config['upload_symbols']:
      commands.UploadSymbols(self._build_root,
                             board=self._build_config['board'],
                             official=self._build_config['chromeos_official'])

    # TODO: When we support branches fully, the friendly name of the branch
    # needs to be used with PushImages
    if not self._options.debug and  self._build_config['push_image']:
      commands.PushImages(self._build_root,
                          board=self._build_config['board'],
                          branch_name='master',
                          archive_dir=archive_dir)


class UploadPrebuiltsStage(NonHaltingBuilderStage):
  """Uploads binaries generated by this build for developer use."""
  def _PerformStage(self):
    manifest_manager = ManifestVersionedSyncStage.manifest_manager
    overlay_config = self._build_config['overlays']
    prebuilt_type = self._prebuilt_type
    board = self._build_config['board']
    binhosts = []
    extra_args = []

    if manifest_manager and manifest_manager.current_version:
      version = manifest_manager.current_version
      extra_args = ['--set-version', version]

    if prebuilt_type == 'chroot':
      board = 'amd64'
    elif prebuilt_type != 'full':
      assert prebuilt_type in ('binary', 'chrome')

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
                  self._prebuilt_type, self._options.chrome_rev,
                  self._options.buildnumber, extra_args + ['--skip-upload'])

        # Master binary bot should upload host preflight prebuilts.
        if prebuilt_type == 'binary' and push_overlays == 'public':
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
        self._options.chrome_rev, self._options.buildnumber, extra_args)


class PublishUprevChangesStage(NonHaltingBuilderStage):
  """Makes uprev changes from pfq live for developers."""
  def _PerformStage(self):
    commands.UprevPush(self._build_root,
                       self._build_config['board'],
                       BuilderStage.push_overlays,
                       self._options.debug)
