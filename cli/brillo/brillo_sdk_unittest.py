# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module tests the brillo sdk command."""

from __future__ import print_function

import mock
import os

from chromite.cbuildbot import constants
from chromite.cbuildbot import repository
from chromite.cli import command_unittest
from chromite.cli.brillo import brillo_sdk
from chromite.lib import bootstrap_lib
from chromite.lib import commandline
from chromite.lib import cros_build_lib_unittest
from chromite.lib import cros_test_lib
from chromite.lib import gs
from chromite.lib import gs_unittest
from chromite.lib import osutils
from chromite.lib import project_sdk
from chromite.lib import workspace_lib

# Unittests often access internals.
# pylint: disable=protected-access


class MockSdkCommand(command_unittest.MockCommand):
  """Mock out the `brillo sdk` command."""
  TARGET = 'chromite.cli.brillo.brillo_sdk.SdkCommand'
  TARGET_CLASS = brillo_sdk.SdkCommand
  COMMAND = 'sdk'

  def __init__(self, *args, **kwargs):
    command_unittest.MockCommand.__init__(self, *args, **kwargs)

  def Run(self, inst):
    return command_unittest.MockCommand.Run(self, inst)


class BrilloSdkTest(cros_test_lib.MockTempDirTestCase):
  """Test class for our BuildCommand class."""

  def setUp(self):
    # Avoid self-updating for most tests.
    os.environ[brillo_sdk._BRILLO_SDK_NO_UPDATE] = '1'

    self.bootstrap_path = os.path.join(self.tempdir, 'bootstrap')
    self.sdk_path = os.path.join(self.tempdir, 'sdk')
    self.workspace_path = os.path.join(self.tempdir, 'workspace')
    osutils.SafeMakedirs(self.workspace_path)

    def fakeRepoRoot(d):
      bootstrap_checkouts = os.path.join(self.bootstrap_path,
                                         bootstrap_lib.SDK_CHECKOUTS)
      if d.startswith(self.sdk_path) or d.startswith(bootstrap_checkouts):
        return d
      return None

    self.PatchObject(project_sdk, 'FindRepoRoot', side_effect=fakeRepoRoot)

    # Prevent actually downloading a repository.
    self.mock_repo = self.PatchObject(repository, 'RepoRepository')

    # Prevent actual GS interaction.
    self.gs_mock = self.StartPatcher(gs_unittest.GSContextMock())
    self.gs_mock.SetDefaultCmdResult()

    # Looking up the 'latest' version from GS will return 4.5.6
    self.latest_version = '4.5.6'
    self.gs_mock.AddCmdResult(
        ['cat', constants.BRILLO_LATEST_RELEASE_URL],
        output=self.latest_version)

  def testResolveLatest(self):
    result = brillo_sdk._ResolveLatest(gs.GSContext())
    self.assertEqual(self.latest_version, result)

  def testUpdateWorkspaceSdk(self):
    brillo_sdk._UpdateWorkspaceSdk(
        gs.GSContext(), self.bootstrap_path, self.workspace_path, '1.2.3')

    # Given the explicit path and version, sync what we expect, and where.
    expected = [mock.call(mock.ANY,
                          os.path.join(self.tempdir,
                                       'bootstrap/sdk_checkouts/1.2.3'),
                          depth=1),
                mock.call().Sync()]

    self.assertEqual(expected, self.mock_repo.mock_calls)

  def testUpdateSdk(self):
    brillo_sdk._UpdateSdk(gs.GSContext(), self.sdk_path, '1.2.3')

    # Given the explicit path and version, sync what we expect, and where.
    expected = [mock.call(mock.ANY,
                          self.sdk_path,
                          depth=1),
                mock.call().Sync()]

    self.assertEqual(expected, self.mock_repo.mock_calls)

    # Verify that the right version number was written out.
    sdk_version_file = project_sdk.VersionFile(self.sdk_path)
    self.assertEqual('1.2.3', osutils.ReadFile(sdk_version_file))


  def testHandleExplicitUpdate(self):
    brillo_sdk._HandleUpdate(
        self.bootstrap_path, None, self.sdk_path, '1.2.3')

    # Given the explicit path and version, sync what we expect, and where.
    expected = [mock.call(mock.ANY,
                          self.sdk_path,
                          depth=1),
                mock.call().Sync()]

    self.assertEqual(expected, self.mock_repo.mock_calls)

  def testHandleLatestUpdate(self):
    brillo_sdk._HandleUpdate(
        self.bootstrap_path, None, self.sdk_path, 'latest')

    # Given the explicit path and version, sync what we expect, and where.
    expected = [mock.call(mock.ANY,
                          self.sdk_path,
                          depth=1),
                mock.call().Sync()]

    self.assertEqual(expected, self.mock_repo.mock_calls)

  def testHandleTotUpdate(self):
    brillo_sdk._HandleUpdate(
        self.bootstrap_path, None, self.sdk_path, 'tot')

    # Given the explicit path and version, sync what we expect, and where.
    expected = [mock.call(constants.MANIFEST_URL,
                          self.sdk_path,
                          manifest=constants.PROJECT_MANIFEST),
                mock.call().Sync()]

    self.assertEqual(expected, self.mock_repo.mock_calls)

  def testHandleWorkspaceUpdate(self):
    brillo_sdk._HandleUpdate(
        self.bootstrap_path, self.workspace_path, None, '1.2.3')

    # Given the explicit path and version, sync what we expect, and where.
    expected = [mock.call(mock.ANY,
                          os.path.join(self.tempdir,
                                       'bootstrap/sdk_checkouts/1.2.3'),
                          depth=1),
                mock.call().Sync()]

    self.assertEqual(expected, self.mock_repo.mock_calls)

  def testFindVersion(self):
    # Fake out the workspace version.
    workspace_version = 'work_version'
    self.PatchObject(workspace_lib, 'GetActiveSdkVersion',
                     return_value=workspace_version)

    # Fake out the SDK version.
    sdk_version = 'sdk version'
    osutils.SafeMakedirs(self.sdk_path)
    sdk_version_file = project_sdk.VersionFile(self.sdk_path)
    osutils.WriteFile(sdk_version_file, sdk_version)

    # If we only know the workspace, use the workspace version.
    self.assertEqual(
        workspace_version, brillo_sdk._FindVersion(self.workspace_path, None))

    # If we have an explicit sdk path, use it's version.
    self.assertEqual(
        sdk_version,
        brillo_sdk._FindVersion(None, self.sdk_path))

    self.assertEqual(
        sdk_version,
        brillo_sdk._FindVersion(self.workspace_path, self.sdk_path))

class BrilloSdkTestSelfUpdate(cros_test_lib.MockTempDirTestCase):
  """Test the self-update functionality of brillo_sdk.

  This is a new class, to avoid mocks interferring with each other.
  """
  def setUp(self):
    if brillo_sdk._BRILLO_SDK_NO_UPDATE in os.environ:
      del os.environ[brillo_sdk._BRILLO_SDK_NO_UPDATE]

    self.bootstrap_path = os.path.join(self.tempdir, 'bootstrap')

    self.rc_mock = self.StartPatcher(cros_build_lib_unittest.RunCommandMock())
    self.rc_mock.SetDefaultCmdResult()

  def testSelfUpdate(self):
    with self.assertRaises(commandline.ExecRequiredError):
      brillo_sdk._SelfUpdate(self.bootstrap_path)

    # Test we did the git pull before raising....
    self.rc_mock.assertCommandContains(
        ['git', 'pull'], cwd=self.bootstrap_path)

    # Test we updated our env before raising....
    self.assertIn(brillo_sdk._BRILLO_SDK_NO_UPDATE, os.environ)

  def testSelfAlreadyUpdated(self):
    # Mark that we already updated.
    os.environ[brillo_sdk._BRILLO_SDK_NO_UPDATE] = '1'

    # Try to update again (no exception raised).
    brillo_sdk._SelfUpdate(self.bootstrap_path)

    # Test we didn't run a git pull.
    self.assertEquals(0, self.rc_mock.call_count)


class BrilloSdkCommandTest(cros_test_lib.MockTempDirTestCase):
  """Test class for our BuildCommand class."""

  def setUp(self):
    if brillo_sdk._BRILLO_SDK_NO_UPDATE in os.environ:
      del os.environ[brillo_sdk._BRILLO_SDK_NO_UPDATE]

    self.cmd_mock = None

    # Workspace is supposed to exist in advance.
    self.sdk_path = os.path.join(self.tempdir, 'sdk')

    # Fake it out, so the SDK appears to be a repo, but bootstrap doesn't.
    self.PatchObject(
        project_sdk, 'FindRepoRoot',
        side_effect=lambda d: d if d.startswith(self.sdk_path) else None)

    # Prevent actually downloading a repository.
    self.mock_repo = self.PatchObject(repository, 'RepoRepository')

  def SetupCommandMock(self, cmd_args):
    """Sets up the command mock."""
    self.cmd_mock = MockSdkCommand(cmd_args)
    self.StartPatcher(self.cmd_mock)

  def testHandleSelfUpdateAndRestart(self):
    self.SetupCommandMock(['--update', 'latest', '--sdk-dir', self.sdk_path])

    with self.assertRaises(commandline.ExecRequiredError):
      self.cmd_mock.inst.Run()

  def testHandleVersionOnlyNoUpdate(self):
    self.SetupCommandMock(['--sdk-dir', self.sdk_path])

    # Write out a version to find.
    osutils.SafeMakedirs(self.sdk_path)
    sdk_version_file = project_sdk.VersionFile(self.sdk_path)
    osutils.WriteFile(sdk_version_file, 'test version')

    # Make sure no exec assertion is raised if we aren't updating.
    self.cmd_mock.inst.Run()

  def testHandleSelfUpdateAfterRestart(self):
    os.environ[brillo_sdk._BRILLO_SDK_NO_UPDATE] = '1'
    self.SetupCommandMock(['--update', 'latest', '--sdk-dir', self.sdk_path])

    # Make sure no exec assertion is raised.
    self.cmd_mock.inst.Run()
