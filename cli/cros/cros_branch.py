# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Command for managing branches of chromiumos.

See go/cros-release-faq for information on types of branches, branching
frequency, naming conventions, etc.
"""

from __future__ import print_function

from chromite.cli import command

RELEASE = 'release'
FACTORY = 'factory'
FIRMWARE = 'firmware'
STABILIZE = 'stabilize'


@command.CommandDecorator('branch')
class BranchCommand(command.CliCommand):
  """Create, delete, or rename a branch of chromiumos.

  Branch creation implies branching all git repositories under chromiumos and
  then updating metadata on the new branch and occassionally the source branch.

  Metadata is updated as follows:
    1. The new branch's manifest is repaired to point to the new branch.
    2. Chrome OS version increments on new branch (e.g., 4230.0.0 -> 4230.1.0).
    3. If the new branch is a release branch, Chrome major version increments
       the on source branch (e.g., R70 -> R71).

  Performing any of these operations remotely requires special permissions.
  Please see go/cros-release-faq for details on obtaining those permissions.
  """

  EPILOG = """
Create Examples:
  cros branch create 11030.0.0 --factory
  cros branch --force --push create 11030.0.0 --firmware
  cros branch create 11030.0.0 --name my-custom-branch --stabilize

Rename Examples:
  cros branch rename release-10509.0.B release-10508.0.B
  cros branch --force --push rename release-10509.0.B release-10508.0.B

Delete Examples:
  cros branch delete release-10509.0.B
  cros branch --force --push delete release-10509.0.B
"""

  @classmethod
  def AddParser(cls, parser):
    """Add parser arguments."""
    super(BranchCommand, cls).AddParser(parser)

    # Common flags.
    parser.add_argument(
        '--push',
        dest='push',
        action='store_true',
        help='Push branch modifications to remote repos. '
        'Before setting this flag, ensure that you have the proper '
        'permissions and that you know what you are doing. Ye be warned.')
    parser.add_argument(
        '--force',
        dest='force',
        action='store_true',
        help='Required for any remote operation that would delete an existing '
        'branch.')

    # Create subcommand and flags.
    subparser = parser.add_subparsers(dest='subcommand')
    create_parser = subparser.add_parser(
        'create', help='Create a branch from a specified maniefest version.')
    create_parser.add_argument(
        'version', help="Manifest version to branch off, e.g. '10509.0.0'.")
    create_parser.add_argument(
        '--name',
        dest='name',
        help='Name for the new branch. If unspecified, name is generated.')

    create_group = create_parser.add_argument_group(
        'Branch Type',
        description='You must specify the type of the new branch. '
        'This affects how manifest metadata is updated and how '
        'the branch is named (if not specified manually).')
    create_ex_group = create_group.add_mutually_exclusive_group(required=True)
    create_ex_group.add_argument(
        '--release',
        dest='kind',
        action='store_const',
        const=RELEASE,
        help='The new branch is a release branch. '
        "Named as 'release-R<Milestone>-<Major Version>.B'.")
    create_ex_group.add_argument(
        '--factory',
        dest='kind',
        action='store_const',
        const=FACTORY,
        help='The new branch is a factory branch. '
        "Named as 'factory-<Major Version>.B'.")
    create_ex_group.add_argument(
        '--firmware',
        dest='kind',
        action='store_const',
        const=FIRMWARE,
        help='The new branch is a firmware branch. '
        "Named as 'firmware-<Major Version>.B'.")
    create_ex_group.add_argument(
        '--stabilize',
        dest='kind',
        action='store_const',
        const=STABILIZE,
        help='The new branch is a minibranch. '
        "Named as 'stabilize-<Major Version>.B'.")

    # Rename subcommand and flags.
    rename_parser = subparser.add_parser('rename', help='Rename a branch.')
    rename_parser.add_argument('old', help='Branch to rename.')
    rename_parser.add_argument('new', help='New name for the branch.')

    # Delete subcommand and flags.
    delete_parser = subparser.add_parser('delete', help='Delete a branch.')
    delete_parser.add_argument('branch', help='Name of the branch to delete.')

  def Run(self):
    raise NotImplementedError(
        'This command is not yet implemented. See chromium:825241.')
