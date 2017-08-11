# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""cros tryjob: Schedule a tryjob."""

from __future__ import print_function

import argparse
import os

from chromite.lib import constants
from chromite.cli import command
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import path_util

from chromite.cbuildbot import remote_try
from chromite.cbuildbot import trybot_patch_pool


def CbuildbotArgs(options):
  """Function to generate cbuidlbot command line args.

  This are pre-api version filtering.

  Args:
    options: Parsed cros tryjob tryjob arguments.

  Returns:
    List of strings in ['arg1', 'arg2'] format.
  """
  args = []

  if options.production:
    args.append('--buildbot')
  else:
    # TODO: Remove from remote_try.py after cbuildbot --remote removed.
    args.append('--remote-trybot')

  if options.branch:
    args.extend(('-b', options.branch))

  if options.hwtest:
    args.append('--hwtest')

  for g in options.gerrit_patches:
    args.extend(('-g', g))

  # Specialty options.
  if options.version:
    args.extend(('--version', options.version))

  for c in options.channels:
    args.extend(('--channel', c))

  # Tack on the pass through options.
  args.extend(options.passthrough)

  return args


def RunLocal(options):
  """Run a local tryjob."""
  # Create the buildroot, if needed.
  if not os.path.exists(options.buildroot):
    prompt = 'Create %s as buildroot' % options.buildroot
    if not cros_build_lib.BooleanPrompt(prompt=prompt, default=False):
      print('Please specify a different buildroot via the --buildroot option.')
      return 1
    os.makedirs(options.buildroot)

  # Define the command to run.
  launcher = os.path.join(constants.CHROMITE_DIR, 'scripts', 'cbuildbot_launch')
  args = CbuildbotArgs(options)  # The requested build arguments.
  cmd = ([launcher, '--buildroot', options.buildroot] +
         args +
         options.build_configs)

  # Run the tryjob.
  result = cros_build_lib.RunCommand(cmd, debug_level=logging.CRITICAL,
                                     error_code_ok=True, cwd=options.buildroot)
  return result.returncode


def RunRemote(options, patch_pool):
  """Schedule remote tryjobs."""
  logging.info('Scheduling remote tryjob(s): %s',
               ', '.join(options.build_configs))

  # Figure out the cbuildbot command line to pass in.
  args = CbuildbotArgs(options)

  # Figure out the tryjob description.
  description = options.remote_description
  if description is None:
    description = remote_try.DefaultDescription(
        options.branch,
        options.gerrit_patches+options.local_patches)

  print('Submitting tryjob...')
  tryjob = remote_try.RemoteTryJob(options.build_configs,
                                   patch_pool.local_patches,
                                   args,
                                   path_util.GetCacheDir(),
                                   description,
                                   options.committer_email)
  tryjob.Submit(dryrun=False)
  print('Tryjob submitted!')
  print(('Go to %s to view the status of your job.'
         % tryjob.GetTrybotWaterfallLink()))


@command.CommandDecorator('tryjob')
class TryjobCommand(command.CliCommand):
  """Schedule a tryjob."""

  EPILOG = """
Remote Examples:
  cros tryjob -g 123 lumpy-compile-only-pre-cq
  cros tryjob -g 123 -g 456 lumpy-compile-only-pre-cq daisy-pre-cq
  cros tryjob -g 123 --hwtest daisy-paladin

Local Examples:
  cros tryjob --local -g 123 daisy-paladin
  cros tryjob --local --buildroot /my/cool/path -g 123 daisy-paladin

Production Examples (danger, can break production if misused):
  cros tryjob --production --branch release-R61-9765.B asuka-release
  cros tryjob --production --version 9795.0.0 --channel canary  lumpy-payloads
"""

  @classmethod
  def AddParser(cls, parser):
    """Adds a parser."""
    super(cls, TryjobCommand).AddParser(parser)
    parser.add_argument(
        'build_configs', nargs='+',
        help='One or more configs to build.')
    parser.add_argument(
        '-b', '--branch',
        help='The manifest branch to test.  The branch to '
             'check the buildroot out to.')
    parser.add_argument(
        '--hwtest', action='store_true', default=False,
        help='Enable hwlab testing. Default false, except for production.')
    parser.add_argument(
        '--yes', action='store_true', default=False,
        help='Never prompt to confirm.')
    parser.add_argument(
        '--production', action='store_true', default=False,
        help='This is a production build, NOT a test build. Use with care.')
    parser.add_argument(
        '--passthrough', nargs=argparse.REMAINDER, default=[],
        help='Arguments to pass to cbuildbot.')

    # Do we build locally, on on a trybot builder?
    where_group = parser.add_argument_group(
        'Where',
        description='Where do we run the tryjob?')
    where_ex = where_group.add_mutually_exclusive_group()
    where_ex.add_argument(
        '--local', action='store_false', dest='remote',
        help='Run the tryjob on your local machine.')
    where_ex.add_argument(
        '--remote', action='store_true', default=True,
        help='Run the tryjob on a remote builder. (default)')
    where_group.add_argument(
        '-r', '--buildroot', type='path', dest='buildroot',
        default=os.path.join(os.path.dirname(constants.SOURCE_ROOT), 'tryjob'),
        help='Root directory to use for the local tryjob. '
             'NOT the current checkout.')

    # What patches do we include in the build?
    what_group = parser.add_argument_group(
        'Patch',
        description='Which patches should be included with the tryjob?')
    what_group.add_argument(
        '-g', '--gerrit-patches', action='append', default=[],
        # metavar='Id1 *int_Id2...IdN',
        help='Space-separated list of short-form Gerrit '
             "Change-Id's or change numbers to patch. "
             "Please prepend '*' to internal Change-Id's")
    what_group.add_argument(
        '-p', '--local-patches', action='append', default=[],
        # metavar="'<project1>[:<branch1>]...<projectN>[:<branchN>]'",
        help='Space-separated list of project branches with '
             'patches to apply.  Projects are specified by name. '
             'If no branch is specified the current branch of the '
             'project will be used.')

    # Identifing the request.
    who_group = parser.add_argument_group(
        'Requestor',
        description='Who is submitting the jobs?')
    who_group.add_argument(
        '--committer-email',
        help='Override default git committer email.')
    who_group.add_argument(
        '--remote-description',
        help='Attach an optional description to a --remote run '
             'to make it easier to identify the results when it '
             'finishes')

    # Specialized tryjob options.
    special_group = parser.add_argument_group(
        'Specialty',
        description='Options only used by specific tryjobs.')
    special_group.add_argument(
        '--version', dest='version', default=None,
        help='Specify the release version for payload regeneration. '
             'Ex: 9799.0.0')
    special_group.add_argument(
        '--channel', dest='channels', action='append', default=[],
        help='Specify a channel for a payloads trybot. Can '
             'be specified multiple times. No valid for '
             'non-payloads configs.')

  def Run(self):
    """Runs `cros chroot`."""
    self.options.Freeze()

    # Ask for confirmation if there are no patches to test.
    if (not self.options.yes and
        not self.options.production and
        not self.options.gerrit_patches and
        not self.options.local_patches):
      prompt = ('No patches were provided; are you sure you want to just '
                'run a remote build of %s?' % (
                    self.options.branch if self.options.branch else 'ToT'))
      if not cros_build_lib.BooleanPrompt(prompt=prompt, default=False):
        cros_build_lib.Die('No confirmation.')

    # Verify gerrit patches are valid.
    print('Verifying patches...')
    patch_pool = trybot_patch_pool.TrybotPatchPool.FromOptions(
        gerrit_patches=self.options.gerrit_patches,
        local_patches=self.options.local_patches,
        sourceroot=constants.SOURCE_ROOT,
        remote_patches=[])

    if self.options.remote:
      return RunRemote(self.options, patch_pool)
    else:
      return RunLocal(self.options)
