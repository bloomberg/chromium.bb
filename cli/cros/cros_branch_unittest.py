# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module tests the `cros branch` command."""

from __future__ import print_function

from chromite.cbuildbot.manifest_version import VersionInfo
from chromite.cli import command_unittest
from chromite.cli.cros.cros_branch import BranchCommand
from chromite.cli.cros.cros_branch import FactoryBranch
from chromite.cli.cros.cros_branch import FirmwareBranch
from chromite.cli.cros.cros_branch import ReleaseBranch
from chromite.cli.cros.cros_branch import StabilizeBranch
from chromite.lib import cros_test_lib
from chromite.lib import partial_mock
from chromite.lib import repo_manifest

BRANCH_NAME = 'branch'
MILESTONE = '12'
VERSION = '3.4.5'
MANIFEST_OUTER_XML = '''\
<?xml version="1.0" encoding="UTF-8"?>
<manifest>%s</manifest>
'''
REMOTES_XML = '''\
  <remote fetch="https://chromium.googlesource.com"
        name="cros"
        review="chromium-review.googlesource.com"/>
'''
DEFAULTS_XML = '''\
  <default remote="cros" revision="refs/heads/master" sync-j="8"/>
'''
BRANCHABLE_PROJECTS_XML = '''\
  <!-- A standard project that should always be branched. -->
  <project groups="minilayout,buildtools"
          name="chromiumos/chromite"
          path="chromite"
          revision="refs/heads/special-branch"/>

  <!-- Explicitly specified branching strategy. -->
  <project name="chromiumos/explicit-create"
          path="explicit-create"
          revision="refs/heads/special-branch">
    <annotation name="branch-mode" value="create"/>
  </project>
'''
NOT_BRANCHABLE_PROJECTS_XML = '''\
  <!-- Explicitly specified branching strategies. -->
  <project name="chromiumos/external-explicitly-pinned"
          path="explicit-external"
          revision="refs/heads/master">
    <annotation name="branch-mode" value="pin"/>
  </project>

  <project name="chromiumos/external-explicitly-unpinned"
          path="explicit-unpinned"
          revision="refs/heads/master">
    <annotation name="branch-mode" value="tot"/>
  </project>

  <!-- The next two projects test legacy heristic to determine branching
      strategy for projects -->
  <project name="faraway/external"
          path="external"
          revision="refs/heads/master"/>

  <project name="faraway/unpinned"
          path="unpinned"
          revision="refs/heads/master"
          pin="False"/>
'''
FULL_MANIFEST = MANIFEST_OUTER_XML % (
    REMOTES_XML + DEFAULTS_XML + BRANCHABLE_PROJECTS_XML +
    NOT_BRANCHABLE_PROJECTS_XML)
BRANCHABLE_MANIFEST = MANIFEST_OUTER_XML % (
    REMOTES_XML + DEFAULTS_XML + BRANCHABLE_PROJECTS_XML)


class MockBranchCommand(command_unittest.MockCommand):
  """Mock out the `cros branch` command."""
  TARGET = 'chromite.cli.cros.cros_branch.BranchCommand'
  TARGET_CLASS = BranchCommand
  COMMAND = 'branch'

  def __init__(self, *args, **kwargs):
    command_unittest.MockCommand.__init__(self, *args, **kwargs)

  def Run(self, inst):
    return command_unittest.MockCommand.Run(self, inst)


class BranchCommandTest(cros_test_lib.MockTestCase):
  """Tests for BranchCommand functions."""

  def SetUpCommandMock(self, args):
    cmd = MockBranchCommand(args)
    self.StartPatcher(cmd)
    return cmd

  def testBranchNameGeneration(self):
    self.PatchObject(
        VersionInfo, 'from_repo', return_value=VersionInfo(VERSION, MILESTONE))
    self.assertEqual(ReleaseBranch().GenerateName(), 'release-R12-3.B')
    self.assertEqual(FactoryBranch().GenerateName(), 'factory-3.B')
    self.assertEqual(FirmwareBranch().GenerateName(), 'firmware-3.B')
    self.assertEqual(StabilizeBranch().GenerateName(), 'stabilize-3.B')

  def testCreateBranchesLocal(self):
    # Configure and run `cros branch`.
    cmd = self.SetUpCommandMock(
        ['create', VERSION, '--name', BRANCH_NAME, '--factory'])
    cmd.rc_mock.AddCmdResult(
        [partial_mock.ListRegex('.*/repo'), 'manifest'], output=FULL_MANIFEST)
    cmd.inst.Run()

    # Assert that local checkout was synced to correct version, and that
    # correct projects were branched.
    cmd.rc_mock.assertCommandContains(
        [partial_mock.ListRegex('.*/repo_sync_manifest'), '--version', VERSION])
    branchable_projects = repo_manifest.Manifest.FromString(
        BRANCHABLE_MANIFEST).Projects()
    for project in branchable_projects:
      cmd.rc_mock.assertCommandContains(
          ['git', 'checkout', '-B', BRANCH_NAME,
           project.Revision()],
          cwd=partial_mock.ListRegex('.*%s' % project.Path()))
