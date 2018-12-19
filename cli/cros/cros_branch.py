# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Command for managing branches of chromiumos.

See go/cros-release-faq for information on types of branches, branching
frequency, naming conventions, etc.
"""

from __future__ import print_function

from chromite.cbuildbot import manifest_version
from chromite.cli import command
from chromite.lib import config_lib
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import git
from chromite.lib import repo_util
from collections import namedtuple

import os
import re

# For the purposes of branching, Chrome OS version is completely defined by
# release milestone and build number.
Version = namedtuple('Version', ['milestone', 'build_number'])


class BranchError(Exception):
  """Raised whenever any branch operation fails."""


class Branch(object):
  """Represents a branch of chromiumos, which may or may not exist yet."""

  def __init__(self, kind, name=None):
    """Cache various configuration used by all branch operations.

    Args:
      kind: A tag that describes the branch (e.g. 'release').
      name: The name of the branch. If not provided, it will be generated.
    """
    self._kind = kind
    self._name = name or self.GenerateName()
    self._repo = repo_util.Repository(constants.SOURCE_ROOT)

    site_params = config_lib.DefaultSiteParameters()
    self._manifest_versions_root = os.path.join(
        constants.SOURCE_ROOT, site_params['INTERNAL_MANIFEST_VERSIONS_PATH'])
    self._cros_remotes = site_params['CROS_REMOTES']
    self._branchable_project_patterns = site_params['BRANCHABLE_PROJECTS']

  def _CanBranchProject(self, project):
    """Determines if the given project is branchable.

    Args:
      project: A repo_manifest.Project.

    Returns:
      True if the project should branch.
    """
    remote = project.Remote().GitName()
    # The preferred way to specify branchability is by adding a "branch-mode"
    # annotation on the project in the manifest. Of course, only one project
    # in the manifest actually does this. ¯\_(ツ)_/¯
    explicit_mode = project.Annotations().get('branch-mode', None)
    if not explicit_mode:
      # The legacy method is to peek at the project's remote.
      return (remote in self._cros_remotes and
              remote in self._branchable_project_patterns and
              re.match(self._branchable_project_patterns[remote], project.name))
    return explicit_mode == constants.MANIFEST_ATTR_BRANCHING_CREATE

  @staticmethod
  def Version():
    """Returns the synced milestone."""
    vinfo = manifest_version.VersionInfo.from_repo(constants.SOURCE_ROOT)
    return Version(vinfo.chrome_branch, vinfo.build_number)

  def GenerateName(self):
    return '%s-%s.B' % (self._kind, self.Version().build_number)

  def Create(self, version, push=False, force=False):
    """Creates a new branch from the given version.

    Args:
      kind: The kind of branch to create (e.g., firmware).
      version: The manifest version off which to branch.
      push: Whether to push the new branch to remote.
      force: Whether or not to overwrite an existing branch.
    """
    if push or force:
      raise NotImplementedError('--push and --force unavailable.')

    # Sync to the manifest version.
    logging.info('Syncing to manifest version %s', version)
    cros_build_lib.RunCommand(
        [
            os.path.join(constants.CHROMITE_DIR, 'scripts/repo_sync_manifest'),
            '--repo-root', constants.SOURCE_ROOT, '--manifest-versions-int',
            self._manifest_versions_root, '--version', version
        ],
        quiet=True)

    # Create local git branches. If a local branch with the same name exists
    # in any project, it is overwritten.
    logging.info('Will create branch %s for all viable projects.', self._name)
    projects = [
        project for project in self._repo.Manifest().Projects()
        if self._CanBranchProject(project)
    ]
    for project in projects:
      git.CreateBranch(
          os.path.join(constants.SOURCE_ROOT, project.Path()), self._name,
          project.Revision())


class ReleaseBranch(Branch):
  """Represents a release branch."""

  def __init__(self, name=None):
    super(ReleaseBranch, self).__init__('release', name)

  def GenerateName(self):
    return '%s-R%s-%s.B' % ((self._kind,) + self.Version())


class FactoryBranch(Branch):
  """Represents a factory branch."""

  def __init__(self, name=None):
    super(FactoryBranch, self).__init__('factory', name)


class FirmwareBranch(Branch):
  """Represents a firmware branch."""

  def __init__(self, name=None):
    super(FirmwareBranch, self).__init__('firmware', name)


class StabilizeBranch(Branch):
  """Represents a factory branch."""

  def __init__(self, name=None):
    super(StabilizeBranch, self).__init__('stabilize', name)


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
        dest='cls',
        action='store_const',
        const=ReleaseBranch,
        help='The new branch is a release branch. '
        "Named as 'release-R<Milestone>-<Major Version>.B'.")
    create_ex_group.add_argument(
        '--factory',
        dest='cls',
        action='store_const',
        const=FactoryBranch,
        help='The new branch is a factory branch. '
        "Named as 'factory-<Major Version>.B'.")
    create_ex_group.add_argument(
        '--firmware',
        dest='cls',
        action='store_const',
        const=FirmwareBranch,
        help='The new branch is a firmware branch. '
        "Named as 'firmware-<Major Version>.B'.")
    create_ex_group.add_argument(
        '--stabilize',
        dest='cls',
        action='store_const',
        const=StabilizeBranch,
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
    if self.options.subcommand == 'create':
      # TODO(evanhernandez): If branch creation is interrupted, some artifacts
      # might be left over. We should check for this.
      self.options.cls(self.options.name).Create(
          version=self.options.version,
          push=self.options.push,
          force=self.options.force)
    elif self.options.subcommand == 'rename':
      raise NotImplementedError('Branch renaming is not yet implemented.')
    elif self.options.subcommand == 'delete':
      raise NotImplementedError('Branch deletion is not yet implemented.')
    else:
      raise BranchError('Unrecognized option.')
