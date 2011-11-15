#!/usr/bin/python

# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Main builder code for Chromium OS.

Used by Chromium OS buildbot configuration for all Chromium OS builds including
full and pre-flight-queue builds.
"""

import constants
import multiprocessing
import Queue
import optparse
import os
import pprint
import subprocess
import sys

if __name__ == '__main__':
  sys.path.append(constants.SOURCE_ROOT)

from chromite.buildbot import builderstage as bs
from chromite.buildbot import cbuildbot_background as background
from chromite.buildbot import cbuildbot_commands as commands
from chromite.buildbot import cbuildbot_config
from chromite.buildbot import cbuildbot_stages as stages
from chromite.buildbot import cbuildbot_results as results_lib
from chromite.buildbot import patch as cros_patch
from chromite.buildbot import repository
from chromite.buildbot import tee
from chromite.lib import cros_build_lib as cros_lib


_BUILDBOT_LOG_FILE = 'cbuildbot.log'
_DEFAULT_EXT_BUILDROOT = 'trybot'
_DEFAULT_INT_BUILDROOT = 'trybot-internal'
_PATH_TO_CBUILDBOT = 'chromite/buildbot/cbuildbot'
_DISTRIBUTED_TYPES = [constants.COMMIT_QUEUE_TYPE, constants.PFQ_TYPE,
                      constants.CANARY_TYPE, constants.CHROME_PFQ_TYPE ]


def _PrintValidConfigs(trybot_only=True):
  """Print a list of valid buildbot configs.

  Arguments:
    trybot_only: Only print selected trybot configs, as specified by the
                 'trybot_list' config setting.
  """
  COLUMN_WIDTH = 45
  print 'config'.ljust(COLUMN_WIDTH), 'description'
  print '------'.ljust(COLUMN_WIDTH), '-----------'
  config_names = cbuildbot_config.config.keys()
  config_names.sort()
  for name in config_names:
    if not trybot_only or cbuildbot_config.config[name]['trybot_list']:
      desc = ''
      if cbuildbot_config.config[name]['description']:
        desc = cbuildbot_config.config[name]['description']

      print name.ljust(COLUMN_WIDTH), desc


def _GetConfig(config_name, options):
  """Gets the configuration for the build"""
  if not cbuildbot_config.config.has_key(config_name):
    print 'Non-existent configuration specified.'
    print 'Please specify one of:'
    _PrintValidConfigs()
    sys.exit(1)

  result = cbuildbot_config.config[config_name]

  return result


def _GetChromiteTrackingBranch():
  """Returns the current Chromite tracking_branch.

  If Chromite is on a detached HEAD, we assume it's the manifest branch.
  """
  cwd = os.path.dirname(os.path.realpath(__file__))
  current_branch = cros_lib.GetCurrentBranch(cwd)
  if current_branch:
    (_, tracking_branch) = cros_lib.GetPushBranch(current_branch, cwd)
  else:
    tracking_branch = cros_lib.GetManifestDefaultBranch(cwd)

  return tracking_branch


def _CheckBuildRootBranch(buildroot, tracking_branch):
  """Make sure buildroot branch is the same as Chromite branch."""
  manifest_branch = cros_lib.GetManifestDefaultBranch(buildroot)
  if manifest_branch != tracking_branch:
    cros_lib.Die('Chromite is not on same branch as buildroot checkout\n' +
                 'Chromite is on branch %s.\n' % tracking_branch +
                 'Buildroot checked out to %s\n' % manifest_branch)


def _PreProcessPatches(gerrit_patches, local_patches, tracking_branch):
  """Validate patches ASAP to catch user errors.  Also generate patch info.

  Args:
    gerrit_patches: List of gerrit CL ID's passed in by user.
    local_patches: List of local project branches to generate patches from.
    tracking_branch: The branch the buildroot will be checked out to.

  Returns:
    A tuple containing a list of cros_patch.GerritPatch and a list of
    cros_patch.LocalPatch objects.
  """
  gerrit_patch_info = []
  local_patch_info = []

  try:
    if gerrit_patches:
      gerrit_patch_info = cros_patch.GetGerritPatchInfo(gerrit_patches)
      for patch in gerrit_patch_info:
        if patch.IsAlreadyMerged():
          cros_lib.Warning('Patch %s has already been merged.' % str(patch))

    if local_patches:
      local_patch_info = cros_patch.PrepareLocalPatches(local_patches,
                                                   tracking_branch)

  except cros_patch.PatchException as e:
    cros_lib.Die(str(e))

  return gerrit_patch_info, local_patch_info


def _IsIncrementalBuild(buildroot, clobber):
  """Returns True if we are reusing an existing buildroot."""
  repo_dir = os.path.join(buildroot, '.repo')
  return not clobber and os.path.isdir(repo_dir)


def _RunSudoNoOp():
  """Run sudo with a noop, to reset the sudo timestamp."""
  proc = subprocess.Popen(['sudo', 'echo'], stdout=subprocess.PIPE, shell=False)
  proc.communicate()


def _RunSudoPeriodically(queue):
  """Runs inside of a separate process.  Periodically runs sudo.

  Put anything in the queue to signal the process to quit.
  """
  SUDO_INTERVAL_MINUTES = 5
  while True:
    print ('Trybot background sudo keep-alive process is running.  Run '
           "'kill -9 %s' to terminate." % str(os.getpid()))
    _RunSudoNoOp()
    try:
      # End the process if we get an exit signal
      queue.get(True, SUDO_INTERVAL_MINUTES * 60)
      break
    except Queue.Empty:
      pass


def _LaunchSudoKeepAliveProcess():
  """Start the background process that avoids the 15 min sudo timeout.

  Returns:
    A multiprocessing.Queue that can be used to stop the process.  Stop
    the process by putting anything in the queue.
  """
  queue = multiprocessing.Queue()
  p = multiprocessing.Process(target=_RunSudoPeriodically, args=(queue,))
  p.start()
  return queue


class Builder(object):
  """Parent class for all builder types.

  This class functions as a parent class for various build types.  It's intended
  use is builder_instance.Run().

  Vars:
    bot_id:  Name of the build configuration.
    build_config:  The configuration dictionary from cbuildbot_config.
    options:  The options provided from optparse in main().
    completed_stages_file: Where we store resume state.
    archive_url:  Where our artifacts for this builder will be archived.
    tracking_branch: The tracking branch for this build.
    release_tag:  The associated "chrome os version" of this build.
    gerrit_patches: Gerrit patches to be included in build.
    local_patches: Local patches to be included in build.
  """

  def __init__(self, bot_id, options, build_config):
    """Initializes instance variables. Must be called by all subclasses."""
    self.bot_id = bot_id
    self.build_config = build_config
    self.options = options

    # TODO, Remove here and in config after bug chromium-os:14649 is fixed.
    if self.build_config['chromeos_official']:
      os.environ['CHROMEOS_OFFICIAL'] = '1'

    self.completed_stages_file = os.path.join(options.buildroot,
                                              '.completed_stages')
    self.archive_url = None
    self.release_tag = None
    self.tracking_branch = _GetChromiteTrackingBranch()
    self.gerrit_patches = None
    self.local_patches = None

  def Initialize(self):
    """Runs through the initialization steps of an actual build."""
    self.gerrit_patches, self.local_patches = _PreProcessPatches(
      self.options.gerrit_patches, self.options.local_patches,
      self.tracking_branch)

    bs.BuilderStage.SetTrackingBranch(self.tracking_branch)

    # Check branch matching early.
    if _IsIncrementalBuild(self.options.buildroot, self.options.clobber):
      _CheckBuildRootBranch(self.options.buildroot, self.tracking_branch)

    if self.options.resume and os.path.exists(self.completed_stages_file):
      with open(self.completed_stages_file, 'r') as load_file:
        results_lib.Results.RestoreCompletedStages(load_file)

    self._RunStage(stages.CleanUpStage)

  def _GetStageInstance(self, stage, *args, **kwargs):
    """Helper function to get an instance given the args.

    Useful as almost all stages just take in bot_id, options, and build_config.
    """
    return stage(self.bot_id, self.options, self.build_config, *args, **kwargs)

  def _RunStage(self, stage, *args, **kwargs):
    """Wrapper to run a stage."""
    stage_instance = self._GetStageInstance(stage, *args, **kwargs)
    return stage_instance.Run()

  def Sync(self):
    """Subclasses must override this method.  Syncs the appropriate code."""
    raise NotImplementedError()

  def RunStages(self):
    """Subclasses must override this method.  Runs the appropriate code."""
    raise NotImplementedError()

  def _WriteCheckpoint(self):
    with open(self.completed_stages_file, 'w+') as save_file:
      results_lib.Results.SaveCompletedStages(save_file)

  def _ShouldReExecuteInBuildRoot(self):
    """Returns True if this build should be re-executed in the buildroot."""
    return not os.path.abspath(
        __file__).startswith(os.path.abspath(self.options.buildroot))

  def _ReExecuteInBuildroot(self):
    """Reexecutes self in buildroot and returns True if build succeeds.

    This allows the buildbot code to test itself when changes are patched for
    buildbot-related code.  This is a no-op if the buildroot == buildroot
    of the running chromite checkout.

    Returns:
      True if the Build succeeded.
    """
    # If we are resuming, use last checkpoint.
    if not self.options.resume:
      self._WriteCheckpoint()

    # Re-write paths to use absolute paths.
    args_to_append = ['--resume', '--buildroot',
                      os.path.abspath(self.options.buildroot)]
    if self.options.chrome_root:
      args_to_append += ['--chrome_root',
                         os.path.abspath(self.options.chrome_root)]

    # Re-run the command in the buildroot.
    return_obj = cros_lib.RunCommand(
        [_PATH_TO_CBUILDBOT] + sys.argv + args_to_append,
        cwd=self.options.buildroot, error_code_ok=True)
    return return_obj.returncode == 0

  def Run(self):
    """Main runner for this builder class.  Runs build and prints summary."""
    print_report = True
    success = True
    try:
      self.Initialize()
      self.Sync()
      if self.gerrit_patches or self.local_patches:
        self._RunStage(stages.PatchChangesStage,
                       self.gerrit_patches, self.local_patches)

      if self._ShouldReExecuteInBuildRoot():
        print_report = False
        success = self._ReExecuteInBuildroot()
      else:
        self.RunStages()

    finally:
      if print_report:
        self._WriteCheckpoint()
        print '\n\n\n@@@BUILD_STEP Report@@@\n'
        results_lib.Results.Report(sys.stdout, self.archive_url,
                                   self.release_tag)
        success = results_lib.Results.Success()

    return success


class SimpleBuilder(Builder):
  """Builder that performs basic vetting operations."""
  def Sync(self):
    """Sync to lkgm or TOT as necessary."""
    if self.options.lkgm or self.build_config['use_lkgm']:
      self._RunStage(stages.LKGMSyncStage)
    else:
      self._RunStage(stages.SyncStage)

  def RunStages(self):
    """Runs through build process."""
    self._RunStage(stages.BuildBoardStage)

    # TODO(sosa): Split these out into classes.
    if self.build_config['build_type'] == constants.CHROOT_BUILDER_TYPE:
      self._RunStage(stages.SDKTestStage)
      self._RunStage(stages.UploadPrebuiltsStage)
      return
    elif self.build_config['build_type'] == constants.REFRESH_PACKAGES_TYPE:
      self._RunStage(stages.RefreshPackageStatusStage)
      return

    self._RunStage(stages.UprevStage)
    self._RunStage(stages.BuildTargetStage)

    bg = background.BackgroundSteps()
    build_and_test_success = False

    archive_stage = self._GetStageInstance(stages.ArchiveStage)
    self.archive_url = archive_stage.GetDownloadUrl()
    vm_test_stage = self._GetStageInstance(stages.VMTestStage, archive_stage)
    unit_test_stage = self._GetStageInstance(stages.UnitTestStage)
    prebuilts_stage = self._GetStageInstance(stages.UploadPrebuiltsStage)
    try:
      bg.AddStep(archive_stage.Run)
      bg.start()
      # Tell the archive_stage not to wait for any more data from the test
      # phase. If the test phase failed strangely, this failsafe ensures
      # that the archive stage doesn't sit around waiting for data.
      try:
        # Run the steps in parallel. If any exceptions occur, RunParallelSteps
        # will combine them into a single BackgroundException and throw it.
        steps = [vm_test_stage.Run, unit_test_stage.Run, prebuilts_stage.Run]
        background.RunParallelSteps(steps)
      finally:
        archive_stage.TestStageExited()

      self._RunStage(stages.HWTestStage)
      self._RunStage(stages.RemoteTestStatusStage)
      build_and_test_success = True

    # We skipped out of this build block early, so one of the tests we ran in
    # the background or in parallel must have failed.
    except (bs.NonBacktraceBuildException, background.BackgroundException):
      pass

    # Wait for remaining stages to finish. Ignore any errors.
    while not bg.Empty(): bg.WaitForStep()
    bg.join()

    return build_and_test_success


class DistributedBuilder(SimpleBuilder):
  """Build class that has special logic to handle distributed builds.

  These builds sync using git/manifest logic in manifest_versions.  In general
  they use a non-distributed builder code for the bulk of the work.
  """
  def __init__(self, bot_id, options, build_config):
    """Initializes a buildbot builder.

    Extra variables:
      completion_stage_class:  Stage used to complete a build.  Set in the Sync
        stage.
    """
    super(DistributedBuilder, self).__init__(bot_id, options, build_config)
    self.completion_stage_class = None

  def Sync(self):
    """Syncs the tree using one of the distributed sync logic paths."""
    # Determine sync class to use.
    if self.build_config['build_type'] in (constants.PFQ_TYPE,
                                           constants.CHROME_PFQ_TYPE):
      sync_stage_class = stages.LKGMCandidateSyncStage
      self.completion_stage_class = stages.LKGMCandidateSyncCompletionStage
    elif self.build_config['build_type'] == constants.COMMIT_QUEUE_TYPE:
      sync_stage_class = stages.CommitQueueSyncStage
      self.completion_stage_class = stages.CommitQueueCompletionStage
    else:
      sync_stage_class = stages.ManifestVersionedSyncStage
      self.completion_stage_class = stages.ManifestVersionedSyncCompletionStage

    self._RunStage(sync_stage_class)

    # Extract version we have decided to build into self.release_tag.
    manifest_manager = stages.ManifestVersionedSyncStage.manifest_manager
    if manifest_manager:
      self.release_tag = manifest_manager.current_version

  def Publish(self, was_build_successful):
    """Completes build by publishing any required information."""
    completion_stage = self._GetStageInstance(self.completion_stage_class,
                                              was_build_successful)
    completion_stage.Run()
    name = completion_stage.name
    if not results_lib.Results.WasStageSuccessfulOrSkipped(name):
      should_publish_changes = False
    else:
      should_publish_changes = (self.build_config['master'] and
                                was_build_successful)

    if should_publish_changes:
      self._RunStage(stages.PublishUprevChangesStage)

  def RunStages(self):
    """Runs simple builder logic and publishes information to overlays."""
    was_build_successful = super(DistributedBuilder, self).RunStages()
    self.Publish(was_build_successful)
    return was_build_successful


def _ConfirmBuildRoot(buildroot):
  """Confirm with user the inferred buildroot, and mark it as confirmed."""
  cros_lib.Warning('Using default directory %s as buildroot' % buildroot)
  prompt = ('\nDo you want to continue (yes/NO)? ')
  response = commands.GetInput(prompt).lower()
  if response != 'yes':
    print('Please specify a buildroot with the --buildroot option.')
    sys.exit(0)

  if not os.path.exists(buildroot):
    os.mkdir(buildroot)

  repository.CreateTrybotMarker(buildroot)


def _DetermineDefaultBuildRoot(internal_build):
  """Default buildroot to be under the directory that contains current checkout.

  Arguments:
    internal_build: Whether the build is an internal build
  """
  repo_dir = cros_lib.FindRepoDir()
  if not repo_dir:
    cros_lib.Die('Could not find root of local checkout.  Please specify'
                 'using --buildroot option.')

  # Place trybot buildroot under the directory containing current checkout.
  top_level = os.path.dirname(os.path.realpath(os.path.dirname(repo_dir)))
  if internal_build:
    buildroot = os.path.join(top_level, _DEFAULT_INT_BUILDROOT)
  else:
    buildroot = os.path.join(top_level, _DEFAULT_EXT_BUILDROOT)

  return buildroot


def _RunBuildStagesWrapper(bot_id, options, build_config):
  """Helper function that wraps RunBuildStages()."""
  def IsDistributedBuilder():
    """Determines whether the build_config should be a DistributedBuilder."""
    if not options.buildbot:
      return False
    elif build_config['build_type'] in _DISTRIBUTED_TYPES:
      chrome_rev = build_config['chrome_rev']
      if options.chrome_rev: chrome_rev = options.chrome_rev
      # We don't do distributed logic to TOT Chrome PFQ's, nor local
      # chrome roots (e.g. chrome try bots)
      if chrome_rev not in [constants.CHROME_REV_TOT,
                            constants.CHROME_REV_LOCAL,
                            constants.CHROME_REV_SPEC]:
        return True

    return False

  # Start tee-ing output to file.
  log_file = os.path.join(os.path.dirname(os.path.realpath(__file__)),
                          _BUILDBOT_LOG_FILE)
  cros_lib.Info('Saving output to %s' % log_file)
  tee_proc = tee.Tee(log_file)
  tee_proc.start()

  try:
    if IsDistributedBuilder():
      buildbot = DistributedBuilder(bot_id, options, build_config)
    else:
      buildbot = SimpleBuilder(bot_id, options, build_config)

    if not buildbot.Run():
      sys.exit(1)
  finally:
    tee_proc.stop()


def _RunBuildStagesWithSudoProcess(bot_id, options, build_config):
  """Starts sudo process before running build stages, and manages cleanup."""
  # Reset sudo timestamp
  _RunSudoNoOp()
  sudo_queue = _LaunchSudoKeepAliveProcess()

  try:
    _RunBuildStagesWrapper(bot_id, options, build_config)
  finally:
    # Pass the stop message to the sudo process.
    sudo_queue.put(object())


# Parser related functions


def _CheckLocalPatches(option, opt_str, value, parser):
  """Do an early quick check of the passed-in patches.

  If the branch of a project is not specified we append the current branch the
  project is on.
  """
  patch_args = value.split()
  if not patch_args:
    raise optparse.OptionValueError('No patches specified')

  parser.values.local_patches = []
  for patch in patch_args:
    components = patch.split(':')
    if len(components) > 2:
      msg = 'Specify local patches in project[:branch] format.'
      raise optparse.OptionValueError(msg)

    # validate project
    project = components[0]
    try:
      project_dir = cros_lib.GetProjectDir('.', project)
    except cros_lib.RunCommandError:
      raise optparse.OptionValueError('Project %s does not exist.' % project)

    # If no branch was specified, we use the project's current branch.
    if len(components) == 1:
      branch = cros_lib.GetCurrentBranch(project_dir)
      if not branch:
        raise optparse.OptionValueError('project %s is not on a branch!'
                                        % project)
      # Append branch information to patch
      patch = '%s:%s' % (project, branch)
    else:
      branch = components[1]
      if not cros_lib.DoesLocalBranchExist(project_dir, branch):
        raise optparse.OptionValueError('Project %s does not have branch %s'
                                        % (project, branch))

    parser.values.local_patches.append(patch)


def _CheckGerritPatches(option, opt_str, value, parser):
  """Do an early quick check of the passed-in patches."""
  parser.values.gerrit_patches = value.split()
  if not parser.values.gerrit_patches:
    raise optparse.OptionValueError('No patches specified')


def _CheckBuildRootOption(option, opt_str, value, parser):
  """Validate and convert buildroot to full-path form."""
  value = value.strip()
  if not value or value == '/':
    raise optparse.OptionValueError('Invalid buildroot specified')

  parser.values.buildroot = os.path.realpath(os.path.expanduser(value))


def _CheckChromeVersionOption(option, opt_str, value, parser):
  """Upgrade other options based on chrome_version being passed."""
  value = value.strip()

  if parser.values.chrome_rev is None and value:
    parser.values.chrome_rev = constants.CHROME_REV_SPEC

  parser.values.chrome_version = value


def _CheckChromeRootOption(option, opt_str, value, parser):
  """Validate and convert chrome_root to full-path form."""
  value = value.strip()
  if not value or value == '/':
    raise optparse.OptionValueError('Invalid chrome_root specified')

  if parser.values.chrome_rev is None:
    parser.values.chrome_rev = constants.CHROME_REV_LOCAL

  parser.values.chrome_root = os.path.realpath(os.path.expanduser(value))


def _CheckChromeRevOption(option, opt_str, value, parser):
  """Validate the chrome_rev option."""
  value = value.strip()
  if value not in constants.VALID_CHROME_REVISIONS:
    raise optparse.OptionValueError('Invalid chrome rev specified')

  parser.values.chrome_rev = value


def _ProcessBuildBotOption(option, opt_str, value, parser):
  """Set side-effects of --buildbot option"""
  parser.values.debug = False
  parser.values.buildbot = True


def _CreateParser():
  """Generate and return the parser with all the options."""
  # Parse options
  usage = "usage: %prog [options] buildbot_config"
  parser = optparse.OptionParser(usage=usage)

  # Main options
  parser.add_option('-a', '--all', action='store_true', dest='print_all',
                    default=False,
                    help=('List all of the buildbot configs available. Use '
                          'with the --list option'))
  parser.add_option('-r', '--buildroot', action='callback', dest='buildroot',
                    type='string', callback=_CheckBuildRootOption,
                    help=('Root directory where source is checked out to, and '
                          'where the build occurs. For external build configs, '
                          "defaults to 'trybot' directory at top level of your "
                          'repo-managed checkout.'))
  parser.add_option('--chrome_rev', default=None, type='string',
                    action='callback', dest='chrome_rev',
                    callback=_CheckChromeRevOption,
                    help=('Revision of Chrome to use, of type '
                          '[%s]' % '|'.join(constants.VALID_CHROME_REVISIONS)))
  parser.add_option('-g', '--gerrit-patches', action='callback',
                    type='string', callback=_CheckGerritPatches,
                    metavar="'Id1 *int_Id2...IdN'",
                    help=("Space-separated list of short-form Gerrit "
                          "Change-Id's or change numbers to patch.  Please "
                          "prepend '*' to internal Change-Id's"))
  parser.add_option('-l', '--list', action='store_true', dest='list',
                    default=False,
                    help=('List the suggested trybot configs to use.  Use '
                          '--all to list all of the available configs.'))
  parser.add_option('-p', '--local-patches', action='callback',
                    metavar="'<project1>[:<branch1>]...<projectN>[:<branchN>]'",
                    type='string', callback=_CheckLocalPatches,
                    help=('Space-separated list of project branches with '
                          'patches to apply.  Projects are specified by name. '
                          'If no branch is specified the current branch of the '
                          'project will be used.'))
  parser.add_option('--profile', default=None, type='string', action='store',
                    dest='profile',
                    help=('Name of profile to sub-specify board variant.'))


  # Advanced options
  group = optparse.OptionGroup(
      parser,
      'Advanced Options',
      'Caution: use these options at your own risk.')

  group.add_option('--buildbot', dest='buildbot', action='callback',
                    default=False, callback=_ProcessBuildBotOption,
                    help='This is running on a buildbot')
  group.add_option('--buildnumber',
                   help='build number', type='int', default=0)
  group.add_option('--chrome_root', default=None, type='string',
                    action='callback', dest='chrome_root',
                    callback=_CheckChromeRootOption,
                    help='Local checkout of Chrome to use.')
  group.add_option('--chrome_version', default=None, type='string',
                   action='callback', dest='chrome_version',
                   callback=_CheckChromeVersionOption,
                   help='Used with SPEC logic to force a particular SVN '
                        'revision of chrome rather than the latest.')
  group.add_option('--clobber', action='store_true', dest='clobber',
                    default=False,
                    help='Clears an old checkout before syncing')
  group.add_option('--hwtests', action='store_true', dest='hw_tests',
                    default=False,
                    help='Run tests on remote machine')
  group.add_option('--lkgm', action='store_true', dest='lkgm', default=False,
                    help='Sync to last known good manifest blessed by PFQ')
  group.add_option('--maxarchives', dest='max_archive_builds',
                    default=3, type='int',
                    help="Change the local saved build count limit.")
  group.add_option('--noarchive', action='store_false', dest='archive',
                    default=True,
                    help="Don't run archive stage.")
  group.add_option('--nobuild', action='store_false', dest='build',
                    default=True,
                    help="Don't actually build (for cbuildbot dev")
  group.add_option('--noclean', action='store_false', dest='clean',
                    default=True,
                    help="Don't clean the buildroot")
  group.add_option('--noprebuilts', action='store_false', dest='prebuilts',
                    default=True,
                    help="Don't upload prebuilts.")
  group.add_option('--nosync', action='store_false', dest='sync',
                    default=True,
                    help="Don't sync before building.")
  group.add_option('--notests', action='store_false', dest='tests',
                    default=True,
                    help='Override values from buildconfig and run no tests.')
  group.add_option('--nouprev', action='store_false', dest='uprev',
                    default=True,
                    help='Override values from buildconfig and never uprev.')
  group.add_option('--remoteip', dest='remote_ip', default=None,
                    help='IP of the remote Chromium OS machine used for '
                         'testing.')
  group.add_option('--remoteteststatus', dest='remote_test_status',
                    default=None, help='List of remote jobs to check status')
  group.add_option('--resume', action='store_true',
                    default=False,
                    help='Skip stages already successfully completed.')
  group.add_option('--version', dest='force_version', default=None,
                   help='Used with manifest logic.  Forces use of this version '
                        'rather than create or get latest.')

  parser.add_option_group(group)

  # Debug options
  group = optparse.OptionGroup(parser, "Debug Options")

  group.add_option('--debug', action='store_true', dest='debug',
                    default=True,
                    help='Override some options to run as a developer.')
  group.add_option('--dump_config', action='store_true', dest='dump_config',
                    default=False,
                    help='Dump out build config options, and exit.')
  parser.add_option_group(group)

  # These options are not used anymore and are being phased out
  parser.add_option('-f', '--revisionfile',
                    help=optparse.SUPPRESS_HELP)
  parser.add_option('-t', '--tracking-branch', dest='tracking_branch_old',
                    default='cros/master', help=optparse.SUPPRESS_HELP)
  return parser


def _PostParseCheck(options):
  """Perform some usage validation after we've parsed the arguments

  Args:
    options: The options object returned by optparse
  """
  if cros_lib.IsInsideChroot():
    cros_lib.Die('Please run cbuildbot from outside the chroot.')

  if options.chrome_root:
    if options.chrome_rev != constants.CHROME_REV_LOCAL:
      cros_lib.Die('Chrome rev must be %s if chrome_root is set.' %
                   constants.CHROME_REV_LOCAL)
  else:
    if options.chrome_rev == constants.CHROME_REV_LOCAL:
      cros_lib.Die('Chrome root must be set if chrome_rev is %s.' %
                   constants.CHROME_REV_LOCAL)

  if options.chrome_version:
    if options.chrome_rev != constants.CHROME_REV_SPEC:
      cros_lib.Die('Chrome rev must be %s if chrome_version is set.' %
                   constants.CHROME_REV_SPEC)
  else:
    if options.chrome_rev == constants.CHROME_REV_SPEC:
      cros_lib.Die('Chrome rev must not be %s if chrome_version is not set.' %
                   constants.CHROME_REV_SPEC)



def main(argv=None):
  if not argv:
    argv = sys.argv[1:]

  # Set umask to 022 so files created by buildbot are readable.
  os.umask(022)

  parser = _CreateParser()
  (options, args) = parser.parse_args(argv)

  if options.list:
    _PrintValidConfigs(not options.print_all)
    sys.exit(0)

  _PostParseCheck(options)

  if len(args) >= 1:
    bot_id = args[-1]
    build_config = _GetConfig(bot_id, options)
  else:
    parser.error('Invalid usage.  Use -h to see usage.')

  if options.dump_config:
    # This works, but option ordering is bad...
    print 'Configuration %s:' % bot_id
    pretty_printer = pprint.PrettyPrinter(indent=2)
    pretty_printer.pprint(build_config)
    sys.exit(0)

  if not options.buildroot:
    if options.buildbot:
      parser.error('Please specify a buildroot with the --buildroot option.')
    else:
      internal = cbuildbot_config.IsInternalBuild(build_config)
      options.buildroot = _DetermineDefaultBuildRoot(internal)
      # We use a marker file in the buildroot to indicate the user has consented
      # to using this directory.
      if not os.path.exists(repository.GetTrybotMarkerPath(options.buildroot)):
        _ConfirmBuildRoot(options.buildroot)

  if options.buildbot:
    _RunBuildStagesWrapper(bot_id, options, build_config)
  else:
    build_config = cbuildbot_config.OverrideConfigForTrybot(build_config)
    _RunBuildStagesWithSudoProcess(bot_id, options, build_config)


if __name__ == '__main__':
  main()
