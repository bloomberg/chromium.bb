# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module tests the `cros branch` command."""

from __future__ import print_function

from chromite.cbuildbot.manifest_version import VersionInfo
from chromite.cli import command_unittest
from chromite.cli.cros.cros_branch import Branch
from chromite.cli.cros.cros_branch import BranchableProjects
from chromite.cli.cros.cros_branch import BranchCommand
from chromite.cli.cros.cros_branch import FactoryBranch
from chromite.cli.cros.cros_branch import FirmwareBranch
from chromite.cli.cros.cros_branch import ReleaseBranch
from chromite.cli.cros.cros_branch import StabilizeBranch
from chromite.lib import cros_test_lib
from chromite.lib import partial_mock
from chromite.lib import repo_manifest
from chromite.lib import repo_util


class BranchableProjectsTest(cros_test_lib.TestCase):
  """Tests for BranchableProjects helper."""

  MANIFEST_CONTENTS = '''\
<?xml version="1.0" encoding="UTF-8"?>
<manifest>
  <remote fetch="https://chromium.googlesource.com"
          name="cros"
          review="chromium-review.googlesource.com"/>

  <default remote="cros" revision="refs/heads/master" sync-j="8"/>

  <project groups="minilayout,buildtools"
           name="chromiumos/chromite"
           path="chromite"
           revision="refs/heads/special-branch"/>

  <project name="chromiumos/special"
           path="src/special-new"
           revision="new-special-branch"/>

  <project name="chromiumos/special"
           path="src/special-old"
           revision="old-special-branch" />

  <!-- Test the explicitly specified branching strategy for projects. -->
  <project name="chromiumos/external-explicitly-pinned"
           path="explicit-external"
           revision="refs/heads/master">
    <annotation name="branch-mode" value="pin" />
  </project>

  <project name="chromiumos/external-explicitly-unpinned"
           path="explicit-unpinned"
           revision="refs/heads/master">
    <annotation name="branch-mode" value="tot" />
  </project>

  <project name="chromiumos/external-explicitly-pinned-sha1"
           path="explicit-external-sha1"
           revision="12345">
    <annotation name="branch-mode" value="pin" />
  </project>

  <project name="chromiumos/external-explicitly-unpinned-sha1"
           path="explicit-unpinned-sha1"
           revision="12345">
    <annotation name="branch-mode" value="tot" />
  </project>

  <!-- The next two projects test legacy heristic to determine branching
       strategy for projects -->
  <project name="faraway/external"
           path="external"
           revision="refs/heads/master" />

  <project name="faraway/unpinned"
           path="unpinned"
           revision="refs/heads/master"
           pin="False" />

</manifest>
  '''

  def testBranchableProjects(self):
    manifest = repo_manifest.Manifest.FromString(self.MANIFEST_CONTENTS)
    projects = [project.Path() for project in BranchableProjects(manifest)]
    self.assertEqual(len(projects), 3)
    self.assertIn('chromite', projects)
    self.assertIn('src/special-old', projects)
    self.assertIn('src/special-new', projects)


class BranchTest(cros_test_lib.MockTestCase):
  """Establishes environment for testing sublcasses of Branch."""

  MILESTONE = '12'
  VERSION = '3.4.5'
  MANIFEST = '''\
<?xml version="1.0" encoding="UTF-8"?>
<manifest>
  <remote fetch="https://chromium.googlesource.com"
          name="cros"
          review="chromium-review.googlesource.com"/>
  <default remote="cros" revision="refs/heads/master" sync-j="8"/>
  <project name="chromiumos/manifest"
          path="path/manifest"
          revision="refs/heads/manifest-branch"/>
  <project name="chromiumos/chromite"
          path="path/chromite"
          revision="refs/heads/chromite-branch"/>
  <project name="chromiumos/not-branchable"
          path="path/not-branchable"
          revision="refs/heads/master">
    <annotation name="branch-mode" value="tot"/>
  </project>
</manifest>
  '''
  BRANCH_NAME = 'branch'

  def CreateInstance(self):
    return Branch('fake-kind', self.BRANCH_NAME)

  def setUp(self):
    self.rc_mock = cros_test_lib.RunCommandMock()
    self.rc_mock.SetDefaultCmdResult()
    self.StartPatcher(self.rc_mock)

    self.PatchObject(
        repo_util.Repository,
        'Manifest',
        return_value=repo_manifest.Manifest.FromString(self.MANIFEST))
    self.PatchObject(
        VersionInfo,
        'from_repo',
        return_value=VersionInfo(self.VERSION, self.MILESTONE))

    # Instance must be created last for patches to be applied.
    self.inst = self.CreateInstance()

  def testCreateLocal(self):
    self.inst.Create(self.VERSION)

    # Assert that local checkout was synced to correct version.
    self.rc_mock.assertCommandContains([
        partial_mock.ListRegex('.*/repo_sync_manifest'), '--version',
        self.VERSION
    ])

    # Assert that all branchable projects were branched.
    for project in ('manifest', 'chromite'):
      self.rc_mock.assertCommandContains(
          [
              'git', 'checkout', '-B', self.BRANCH_NAME,
              'refs/heads/%s-branch' % project
          ],
          cwd=partial_mock.ListRegex('.*/path/%s' % project))


class ReleaseBranchTest(BranchTest):
  """Tests and config specifically for ReleaseBranch."""
  BRANCH_NAME = 'release-R12-3.B'

  def CreateInstance(self):
    return ReleaseBranch()


class FactoryBranchTest(BranchTest):
  """Tests and config specifically for FactoryBranch."""
  BRANCH_NAME = 'factory-3.B'

  def CreateInstance(self):
    return FactoryBranch()


class FirmwareBranchTest(BranchTest):
  """Tests and config specifically for FirmwareBranch."""
  BRANCH_NAME = 'firmware-3.B'

  def CreateInstance(self):
    return FirmwareBranch()


class StabilizeBranchTest(BranchTest):
  """Tests and config specifically for StabilizeBranch."""
  BRANCH_NAME = 'stabilize-3.B'

  def CreateInstance(self):
    return StabilizeBranch()


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
  VERSION = '3.4.5'
  BRANCH_NAME = 'branch'

  def SetUpCommandMock(self, args):
    cmd = MockBranchCommand(args)
    self.StartPatcher(cmd)
    return cmd

  def TestCreateCommandParsesFor(self, cls, kind_flag):
    cmd = self.SetUpCommandMock(['create', self.VERSION, kind_flag])

    self.assertIs(cmd.inst.options.cls, cls)

    # It's important that `cros branch` does not perform dangerous
    # operations by default.
    self.assertIsNone(cmd.inst.options.name)
    self.assertFalse(cmd.inst.options.force)
    self.assertFalse(cmd.inst.options.push)

  def testCreateReleaseCommandParses(self):
    self.TestCreateCommandParsesFor(ReleaseBranch, '--release')

  def testCreateFactoryCommandParses(self):
    self.TestCreateCommandParsesFor(FactoryBranch, '--factory')

  def testCreateFirmwareCommandParses(self):
    self.TestCreateCommandParsesFor(FirmwareBranch, '--firmware')

  def testCreateStabilizeCommandParses(self):
    self.TestCreateCommandParsesFor(StabilizeBranch, '--stabilize')
