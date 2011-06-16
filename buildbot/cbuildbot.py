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
import sys

if __name__ == '__main__':
  sys.path.append(constants.SOURCE_ROOT)

from chromite.buildbot import cbuildbot_commands as commands
from chromite.buildbot import cbuildbot_config
from chromite.buildbot import cbuildbot_stages as stages
from chromite.lib import cros_build_lib as cros_lib


def _GetConfig(config_name, options):
  """Gets the configuration for the build"""
  if not cbuildbot_config.config.has_key(config_name):
    print 'Non-existent configuration specified.'
    print 'Please specify one of:'
    config_names = cbuildbot_config.config.keys()
    config_names.sort()
    for name in config_names:
      print '  %s' % name
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


def _PreProcessPatches(patches):
  patch_info = None

  if patches:
    try:
      patch_info = commands.GetGerritPatchInfo(patches)
    except commands.PatchException as e:
      cros_lib.Die(str(e))

  return patch_info


def RunBuildStages(bot_id, options, build_config):
  """Run the requested build stages."""

  completed_stages_file = os.path.join(options.buildroot, '.completed_stages')

  if options.resume and os.path.exists(completed_stages_file):
    with open(completed_stages_file, 'r') as load_file:
      stages.Results.RestoreCompletedStages(load_file)

  # TODO, Remove here and in config after bug chromium-os:14649 is fixed.
  if build_config['chromeos_official']: os.environ['CHROMEOS_OFFICIAL'] = '1'

  # Determine the stages to use for syncing and completion.
  sync_stage = stages.SyncStage
  completion_stage = None
  if build_config['manifest_version']:
    if build_config['build_type'] == 'binary':
      sync_stage = stages.LGKMVersionedSyncStage
      completion_stage = stages.LGKMVersionedSyncCompletionStage
    else:
      sync_stage = stages.ManifestVersionedSyncStage
      completion_stage = stages.ManifestVersionedSyncCompletionStage

  tracking_branch = _GetChromiteTrackingBranch()
  stages.BuilderStage.SetTrackingBranch(tracking_branch)

  repo_dir = os.path.join(options.buildroot, '.repo')
  if not options.clobber and os.path.isdir(repo_dir):
    _CheckBuildRootBranch(options.buildroot, tracking_branch)

  patches = _PreProcessPatches(options.patches)

  build_success = False
  build_and_test_success = False

  try:
    if options.sync:
      sync_stage(bot_id, options, build_config).Run()

    if options.patches:
      stages.PatchChangesStage(bot_id, options, build_config, patches).Run()

    if options.build:
      stages.BuildBoardStage(bot_id, options, build_config).Run()
      if build_config['build_type'] == 'chroot':
        stages.Results.Report(sys.stdout)
        return stages.Results.Success()

    if options.uprev:
      stages.UprevStage(bot_id, options, build_config).Run()

    if options.build:
      stages.BuildTargetStage(bot_id, options, build_config).Run()

    build_success = True

    if options.tests:
      stages.TestStage(bot_id, options, build_config).Run()

    if options.remote_test_status:
      stages.RemoteTestStatusStage(bot_id, options, build_config).Run()

    build_and_test_success = True
  except stages.BuildException:
    # We skipped out of this build block early, all we need to do.
    pass

  if options.sync:
    if (completion_stage and
        stages.ManifestVersionedSyncStage.manifest_manager):
      completion_stage(bot_id, options, build_config,
                       success=build_and_test_success).Run()

    if build_config['master'] and build_and_test_success:
      try:
        stages.PushChangesStage(bot_id, options, build_config).Run()
      except stages.BuildException:
        pass

  if build_success and options.archive:
    try:
      stages.ArchiveStage(bot_id, options, build_config).Run()
    except stages.BuildException:
      pass

  if os.path.exists(options.buildroot):
    with open(completed_stages_file, 'w+') as save_file:
      stages.Results.SaveCompletedStages(save_file)

  # Tell the buildbot to break out the report as a final step
  print '\n\n\n@@@BUILD_STEP Report@@@\n'
  stages.Results.Report(sys.stdout)

  return stages.Results.Success()


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


def _CheckPatches(option, opt_str, value, parser):
  """Do an early quick check of the passed-in patches."""
  parser.values.patches = value.split()
  if not len(parser.values.patches):
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
  usage = "usage: %prog [options] cbuildbot_config"
  parser = optparse.OptionParser(usage=usage)

  # Main options
  parser.add_option('-r', '--buildroot', action='callback', dest='buildroot',
                    type='string', callback=_CheckBuildRootOption,
                    help=('Root directory where source is checked out to, and '
                         'where the build occurs'))
  parser.add_option('--chrome_rev', default=None, type='string',
                    action='callback', dest='chrome_rev',
                    callback=_CheckChromeRevOption,
                    help=('Revision of Chrome to use, of type '
                          '[%s]' % '|'.join(constants.VALID_CHROME_REVISIONS)))
  parser.add_option('-p', '--patches', action='callback', dest='patches',
                    metavar="'Id1 *int_Id2...IdN'",
                    default=None, type='string', callback=_CheckPatches,
                    help=("Space-separated list of short-form Gerrit "
                          "Change-Id's or change numbers to patch.  Please "
                          "prepend '*' to internal Change-Id's"))

  # Advanced options
  group = optparse.OptionGroup(
      parser,
      'Advanced Options',
      'Caution: use these options at your own risk.')

  group.add_option('--buildbot', dest='buildbot', action='callback',
                    default=False, callback=_ProcessBuildBotOption,
                    help='This is running on a buildbot')
  group.add_option('-n', '--buildnumber',
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
  group.add_option('--remoteteststatus', dest='remote_test_status',
                    default=None, help='List of remote jobs to check status')
  group.add_option('--resume', action='store_true',
                    default=False,
                    help='Skip stages already successfully completed.')
  group.add_option('-u', '--url', dest='url',
                    default=None,
                    help='Override the GIT repo URL from the build config.')
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


def main():
  parser = _CreateParser()
  (options, args) = parser.parse_args()

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
    cros_lib.Die('Please specify a buildroot with the --buildroot option.')

  if options.clobber:
    _ValidateClobber(options.buildroot, options.buildbot)

  if not RunBuildStages(bot_id, options, build_config):
    sys.exit(1)


if __name__ == '__main__':
    main()
