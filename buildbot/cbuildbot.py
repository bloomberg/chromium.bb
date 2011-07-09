#!/usr/bin/python

# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Main builder code for Chromium OS.

Used by Chromium OS buildbot configuration for all Chromium OS builds including
full and pre-flight-queue builds.
"""

import constants
import optparse
import os
import pprint
import subprocess
import sys
import time

from multiprocessing import Process

if __name__ == '__main__':
  sys.path.append(constants.SOURCE_ROOT)

from chromite.buildbot import cbuildbot_commands as commands
from chromite.buildbot import cbuildbot_config
from chromite.buildbot import cbuildbot_stages as stages
from chromite.buildbot import patch as cros_patch
from chromite.lib import cros_build_lib as cros_lib


_BUILDBOT_LOG_FILE = 'cbuildbot.log'
_DEFAULT_EXT_BUILDROOT = 'trybot'
_DEFAULT_INT_BUILDROOT = 'trybot-internal'


def _PrintValidConfigs():
  """Print a list of valid buildbot configs."""
  config_names = cbuildbot_config.config.keys()
  config_names.sort()
  for name in config_names:
    print '  %s' % name


def _GetConfig(config_name, options):
  """Gets the configuration for the build"""
  if not cbuildbot_config.config.has_key(config_name):
    print 'Non-existent configuration specified.'
    print 'Please specify one of:'
    _PrintValidConfigs()
    sys.exit(1)

  result = cbuildbot_config.config[config_name]

  # Use the config specific url, if not given on command line.
  if options.url:
    result['git_url'] = options.url

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


def _RunSudoPeriodically():
  """Runs inside of a separate process.  Periodically runs sudo."""
  SUDO_INTERVAL_MINUTES = 5
  while True:
    print ('Trybot background sudo keep-alive process is running.  Run '
           "'kill -9 %s' to terminate." % str(os.getpid()))
    _RunSudoNoOp()
    time.sleep(SUDO_INTERVAL_MINUTES * 60)


def _LaunchSudoKeepAliveProcess():
  """"Start the background process that avoids the 15 min sudo timeout."""
  p = Process(target=_RunSudoPeriodically)
  p.start()


def RunBuildStages(bot_id, options, build_config):
  """Run the requested build stages."""
  completed_stages_file = os.path.join(options.buildroot, '.completed_stages')

  if options.resume and os.path.exists(completed_stages_file):
    with open(completed_stages_file, 'r') as load_file:
      stages.Results.RestoreCompletedStages(load_file)

  # TODO, Remove here and in config after bug chromium-os:14649 is fixed.
  if build_config['chromeos_official']: os.environ['CHROMEOS_OFFICIAL'] = '1'

  # Determine the stages to use for syncing and completion.
  sync_stage_class = stages.SyncStage
  completion_stage_class = None
  if not options.buildbot:
    # For trybots, always patch to last known good manifest.
    sync_stage_class = stages.LKGMSyncStage
  elif build_config['manifest_version'] and options.chrome_rev != 'tot':
    # TODO(sosa): Fix temporary hack for chrome_rev tot.
    if build_config['build_type'] in ('binary', 'chrome'):
      sync_stage_class = stages.LKGMCandidateSyncStage
      completion_stage_class = stages.LKGMCandidateSyncCompletionStage
    else:
      sync_stage_class = stages.ManifestVersionedSyncStage
      completion_stage_class = stages.ManifestVersionedSyncCompletionStage

  tracking_branch = _GetChromiteTrackingBranch()
  stages.BuilderStage.SetTrackingBranch(tracking_branch)

  gerrit_patches, local_patches = _PreProcessPatches(options.gerrit_patches,
                                                     options.local_patches,
                                                     tracking_branch)

  # For trybots, after most of the preprocessing is done, run sudo to set the
  # timestamp and kick off the sudo keep-alive process.
  if not options.buildbot:
    _RunSudoNoOp()
    _LaunchSudoKeepAliveProcess()

  if _IsIncrementalBuild(options.buildroot, options.clobber):
    _CheckBuildRootBranch(options.buildroot, tracking_branch)
    commands.PreFlightRinse(options.buildroot)

  build_success = False
  build_and_test_success = False
  already_uploaded_prebuilts = False
  prebuilts = options.prebuilts and build_config['prebuilts']

  try:
    if options.sync:
      sync_stage_class(bot_id, options, build_config).Run()

    if options.gerrit_patches or options.local_patches:
      stages.PatchChangesStage(bot_id, options, build_config, gerrit_patches,
                               local_patches).Run()

    if options.build:
      stages.BuildBoardStage(bot_id, options, build_config).Run()

    if build_config['build_type'] == 'chroot':
      stages.UploadPrebuiltsStage(bot_id, options, build_config).Run()
      stages.Results.Report(sys.stdout)
      return stages.Results.Success()

    if options.uprev:
      stages.UprevStage(bot_id, options, build_config).Run()

    if options.build:
      stages.BuildTargetStage(bot_id, options, build_config).Run()

    if prebuilts and build_config['build_type'] == 'full':
      stages.UploadPrebuiltsStage(bot_id, options, build_config).Run()
      already_uploaded_prebuilts = True

    build_success = True

    if options.tests:
      stages.TestStage(bot_id, options, build_config).Run()
      if options.remote_ip:
        stages.TestHWStage(bot_id, options, build_config).Run()

    if options.remote_test_status:
      stages.RemoteTestStatusStage(bot_id, options, build_config).Run()

    build_and_test_success = True

    if prebuilts and not already_uploaded_prebuilts:
      stages.UploadPrebuiltsStage(bot_id, options, build_config).Run()
  except stages.BuildException:
    # We skipped out of this build block early, all we need to do.
    pass

  completion_stage = None
  if (options.sync and completion_stage_class and
      stages.ManifestVersionedSyncStage.manifest_manager):
    completion_stage = completion_stage_class(bot_id, options, build_config,
                                              success=build_and_test_success)

  if not build_config['master'] and completion_stage:
    # Report success or failure to the master.
    completion_stage.Run()

  if build_success and options.archive:
    stages.ArchiveStage(bot_id, options, build_config).Run()

  if build_config['master'] and completion_stage:
    # Wait for slave builds to complete.
    completion_stage.Run()

  if (options.buildbot and build_config['master'] and build_and_test_success
      and (not completion_stage or
           stages.Results.WasStageSuccessfulOrSkipped(completion_stage.name))):
    stages.PublishUprevChangesStage(bot_id, options, build_config).Run()

  if os.path.exists(options.buildroot):
    with open(completed_stages_file, 'w+') as save_file:
      stages.Results.SaveCompletedStages(save_file)

  # Tell the buildbot to break out the report as a final step
  print '\n\n\n@@@BUILD_STEP Report@@@\n'
  stages.Results.Report(sys.stdout)

  return stages.Results.Success()


def _SetupRedirectOutputToFile():
  """Create a tee subprocess and redirect stdout and stderr to it."""
  cros_lib.Info('Saving output to %s file' % _BUILDBOT_LOG_FILE)
  sys.stdout = os.fdopen(sys.stdout.fileno(), 'w', 0)

  tee = subprocess.Popen(['tee', _BUILDBOT_LOG_FILE], stdin=subprocess.PIPE)
  os.dup2(tee.stdin.fileno(), sys.stdout.fileno())
  os.dup2(tee.stdin.fileno(), sys.stderr.fileno())


# Input validation functions


def _GetInput(prompt):
  """Helper function that makes testing easier."""
  return raw_input(prompt)


def _ValidateClobber(buildroot, buildbot):
  """Do due diligence if user wants to clobber buildroot.

  Args:
    buildroot: passed in buildroot
    buildbot: whether this is running on a buildbot
  """
  cwd = os.path.dirname(os.path.realpath(__file__))
  if cwd.startswith(buildroot):
    cros_lib.Die('You are trying to clobber this chromite checkout!')

  # Don't ask for clobber confirmation if we are run with --buildbot
  if not buildbot and os.path.exists(buildroot):
    cros_lib.Warning('This will delete %s' % buildroot)
    prompt = ('\nDo you want to continue (yes/NO)? ')
    response = _GetInput(prompt).lower()
    if response != 'yes':
      sys.exit(0)


def _ConfirmBuildRoot(buildroot):
  """Make sure the user wants to use inferred buildroot."""
  cros_lib.Warning('Using default directory %s as buildroot' % buildroot)
  prompt = ('\nDo you want to continue (yes/NO)? ')
  response = _GetInput(prompt).lower()
  if response != 'yes':
    print('Please specify a buildroot with the --buildroot option.')
    sys.exit(0)


def _DetermineDefaultBuildRoot(git_url):
  """Default buildroot to a top-level directory in current source checkout.

  We separate the buildroot for external and internal configurations.
  """
  repo_dir = cros_lib.FindRepoDir()
  if not repo_dir:
    cros_lib.Die('Could not find root of local checkout.  Please specify'
                 'using --buildroot option.')

  srcroot = os.path.realpath(os.path.dirname(repo_dir))
  if git_url == cbuildbot_config.MANIFEST_INT_URL:
    buildroot = os.path.join(srcroot, _DEFAULT_INT_BUILDROOT)
  else:
    buildroot = os.path.join(srcroot, _DEFAULT_EXT_BUILDROOT)

  return buildroot


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
                    help="List the valid buildbot configs to use.")
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
  group.add_option('--clobber', action='store_true', dest='clobber',
                    default=False,
                    help='Clears an old checkout before syncing')
  group.add_option('--noarchive', action='store_false', dest='archive',
                    default=True,
                    help="Don't run archive stage.")
  group.add_option('--nobuild', action='store_false', dest='build',
                    default=True,
                    help="Don't actually build (for cbuildbot dev")
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
                   help='IP of the remote machine where tests are run.')
  group.add_option('--remoteteststatus', dest='remote_test_status',
                    default=None, help='List of remote jobs to check status')
  group.add_option('--resume', action='store_true',
                    default=False,
                    help='Skip stages already successfully completed.')
  group.add_option('--url', dest='url',
                   default=None,
                   help='Override the GIT repo URL from the build config.')
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


def main(argv=None):
  if not argv:
    argv = sys.argv[1:]

  parser = _CreateParser()
  (options, args) = parser.parse_args(argv)

  if options.list:
    _PrintValidConfigs()
    sys.exit(0)

  if len(args) >= 1:
    bot_id = args[-1]
    build_config = _GetConfig(bot_id, options)
  else:
    parser.error('Invalid usage.  Use -h to see usage.')

  if options.dump_config:
    # This works, but option ordering is bad...
    print 'Configuration %s:' % bot_id
    pp = pprint.PrettyPrinter(indent=2)
    pp.pprint(build_config)
    sys.exit(0)

  if not options.buildroot:
    if options.buildbot:
      parser.error('Please specify a buildroot with the --buildroot option.')
    else:
      options.buildroot = _DetermineDefaultBuildRoot(build_config['git_url'])
      if not os.path.exists(options.buildroot):
        _ConfirmBuildRoot(options.buildroot)

  if options.clobber:
    _ValidateClobber(options.buildroot, options.buildbot)

  _SetupRedirectOutputToFile()

  if not RunBuildStages(bot_id, options, build_config):
    sys.exit(1)


if __name__ == '__main__':
  main()
