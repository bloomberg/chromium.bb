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
from chromite.lib import cros_build_lib
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


class BrilloSdkTest(cros_test_lib.MockTempDirTestCase):
  """Test class for brillo_sdk module functions."""

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
    """Tests _ResolveLatest()."""
    result = brillo_sdk._ResolveLatest(gs.GSContext())
    self.assertEqual(self.latest_version, result)

  def testUpdateWorkspaceSdk(self):
    """Tests _UpdateWorkspaceSdk() with a numeric version."""
    brillo_sdk._UpdateWorkspaceSdk(
        gs.GSContext(), self.bootstrap_path, self.workspace_path, '1.2.3')

    # Given the explicit path and version, sync what we expect, and where.
    expected = [mock.call(mock.ANY,
                          os.path.join(self.tempdir,
                                       'bootstrap/sdk_checkouts/1.2.3'),
                          depth=1,
                          repo_cmd=mock.ANY),
                mock.call().Sync()]

    self.assertEqual(expected, self.mock_repo.mock_calls)

    # Update a second time, to ensure it does nothing the second time.
    brillo_sdk._UpdateWorkspaceSdk(
        gs.GSContext(), self.bootstrap_path, self.workspace_path, '1.2.3')

    self.assertEqual(expected, self.mock_repo.mock_calls)

  def testUpdateWorkspaceSdkLatest(self):
    """Tests _UpdateWorkspaceSdk() with 'latest'."""
    brillo_sdk._UpdateWorkspaceSdk(
        gs.GSContext(), self.bootstrap_path, self.workspace_path, 'latest')

    # Given the explicit path and version, sync what we expect, and where.
    expected = [mock.call(mock.ANY,
                          os.path.join(self.tempdir,
                                       'bootstrap/sdk_checkouts',
                                       self.latest_version),
                          depth=1,
                          repo_cmd=mock.ANY),
                mock.call().Sync()]

    self.assertEqual(expected, self.mock_repo.mock_calls)

  def testUpdateWorkspaceSdkTot(self):
    """Tests _UpdateWorkspaceSdk() with 'tot'."""
    brillo_sdk._UpdateWorkspaceSdk(
        gs.GSContext(), self.bootstrap_path, self.workspace_path, 'tot')

    # Given the explicit path and version, sync what we expect, and where.
    expected = [mock.call(constants.MANIFEST_URL,
                          os.path.join(self.tempdir,
                                       'bootstrap/sdk_checkouts/tot'),
                          groups='project_sdk',
                          repo_cmd=mock.ANY),
                mock.call().Sync()]

    self.assertEqual(expected, self.mock_repo.mock_calls)

    # Update a second time, to ensure it DOES update.
    brillo_sdk._UpdateWorkspaceSdk(
        gs.GSContext(), self.bootstrap_path, self.workspace_path, 'tot')

    self.assertEqual(2 * expected, self.mock_repo.mock_calls)

  def testDownloadSdk(self):
    """Tests DownloadSdk() with a numeric version."""
    brillo_sdk._DownloadSdk(gs.GSContext(), self.sdk_path, '1.2.3')

    # Given the explicit path and version, sync what we expect, and where.
    expected = [mock.call(mock.ANY,
                          self.sdk_path,
                          depth=1,
                          repo_cmd=mock.ANY),
                mock.call().Sync()]

    self.assertEqual(expected, self.mock_repo.mock_calls)

    # Verify that the right version number was written out.
    sdk_version_file = project_sdk.VersionFile(self.sdk_path)
    self.assertEqual('1.2.3', osutils.ReadFile(sdk_version_file))

  def testDownloadSdkFailureCleanup(self):
    """Tests DownloadSdk() with a numeric version."""

    # Create sdk_path, and put something in it so we confirm it's gone.
    osutils.Touch(os.path.join(self.sdk_path, 'contents'), makedirs=True)

    # Prep for failure via the repo mock.
    class FakeException(Exception):
      """Raised to simulate a failure."""

    self.mock_repo.side_effect = FakeException('Testing a failure.')

    # Run, and fail.
    with self.assertRaises(FakeException):
      brillo_sdk._DownloadSdk(gs.GSContext(), self.sdk_path, '1.2.3')

    # Make sure the SDK dir was cleaned up.
    self.assertFalse(os.path.exists(self.sdk_path))


class SdkVersionExistsTest(cros_test_lib.WorkspaceTestCase):
  """Tests for _SdkVersionExists()."""

  VALID_LOCAL_SDK_VERSION = '1.2.3'
  VALID_GS_SDK_VERSION = '4.5.6'
  VALID_GS_SDK_URL = brillo_sdk._GetSdkManifestUrl(VALID_GS_SDK_VERSION)

  class GSContextFake(object):
    """Fake GSContext class for Exists() functionality."""
    def Exists(self, url, **_kwargs):
      """Only VALID_GS_SDK_URL returns True."""
      return url == SdkVersionExistsTest.VALID_GS_SDK_URL

  def _VerifySdkVersionExists(self, version, expected):
    """Verifies _SdkVersionExists().

    Args:
      version: Version to pass in.
      expected: Expected result.
    """
    result = brillo_sdk._SdkVersionExists(self.GSContextFake(),
                                          self.bootstrap_path, version)
    self.assertEqual(expected, result)


  def testSdkVersionExists(self):
    """Tests the expected behavior."""
    self.CreateBootstrap(sdk_version=self.VALID_LOCAL_SDK_VERSION)

    self._VerifySdkVersionExists('tot', True)
    self._VerifySdkVersionExists('latest', True)
    self._VerifySdkVersionExists(self.VALID_LOCAL_SDK_VERSION, True)
    self._VerifySdkVersionExists(self.VALID_GS_SDK_VERSION, True)

    self._VerifySdkVersionExists('tot2', False)
    self._VerifySdkVersionExists('', False)


class BrilloSdkTestUpdateBootstrap(cros_test_lib.MockTempDirTestCase):
  """Test the bootstrap update functionality of brillo_sdk.

  This is a new class, to avoid mocks interfering with each other.
  """
  def setUp(self):
    if brillo_sdk._BRILLO_SDK_NO_UPDATE in os.environ:
      del os.environ[brillo_sdk._BRILLO_SDK_NO_UPDATE]

    self.bootstrap_path = os.path.join(self.tempdir, 'bootstrap')

    self.rc_mock = self.StartPatcher(cros_build_lib_unittest.RunCommandMock())
    self.rc_mock.SetDefaultCmdResult()

  def testUpdateBootstrap(self):
    """Tests _UpdateBootstrap()."""
    with self.assertRaises(commandline.ExecRequiredError):
      brillo_sdk._UpdateBootstrap(self.bootstrap_path)

    # Test we did the git pull before raising....
    self.rc_mock.assertCommandContains(
        ['git', 'pull'], cwd=self.bootstrap_path)

    # Test we updated our env before raising....
    self.assertIn(brillo_sdk._BRILLO_SDK_NO_UPDATE, os.environ)

  def testBootstrapAlreadyUpdated(self):
    """Tests _UpdateBootstrap() doesn't run if already updated."""
    # Mark that we already updated.
    os.environ[brillo_sdk._BRILLO_SDK_NO_UPDATE] = '1'

    # Try to update again (no exception raised).
    brillo_sdk._UpdateBootstrap(self.bootstrap_path)

    # Test we didn't run a git pull.
    self.assertEquals(0, self.rc_mock.call_count)


class BrilloSdkCommandTest(cros_test_lib.OutputTestCase,
                           cros_test_lib.WorkspaceTestCase):
  """Test class for our SdkCommand class."""

  def setUp(self):
    if brillo_sdk._BRILLO_SDK_NO_UPDATE in os.environ:
      del os.environ[brillo_sdk._BRILLO_SDK_NO_UPDATE]

    self.cmd_mock = None

    # Workspace is supposed to exist in advance.
    self.CreateBootstrap()
    self.CreateWorkspace()

    # Pretend we are outside the chroot, since this command only runs there.
    self.mock_inside = self.PatchObject(cros_build_lib, 'IsInsideChroot',
                                        return_value=False)

    # Need to mock this; since RunCommand() is mocked out, all the environment
    # checks would fail.
    self.verify_environment_mock = self.PatchObject(
        project_sdk, 'VerifyEnvironment', return_value=True)

    # Prevent repo operations.
    self.PatchObject(repository, 'PrepManifestForRepo')
    self.PatchObject(repository, 'RepoRepository')

    # Default to valid SDK version.
    self.sdk_version_exists_mock = self.PatchObject(
        brillo_sdk, '_SdkVersionExists', return_value=True)

  def SetupCommandMock(self, cmd_args):
    """Sets up the command mock."""
    self.cmd_mock = MockSdkCommand(cmd_args)
    self.StartPatcher(self.cmd_mock)

  def testHandleSelfUpdateAndRestart(self):
    """Tests that --update causes a re-exec."""
    self.SetupCommandMock(['--update', 'latest'])
    with self.assertRaises(commandline.ExecRequiredError):
      self.cmd_mock.inst.Run()

  def testHandleVersionOnlyNoUpdate(self):
    """Tests that `cros sdk` logs a version and doesn't re-exec."""
    self.SetupCommandMock([])
    workspace_lib.SetActiveSdkVersion(self.workspace_path, '1.2.3')

    with self.OutputCapturer():
      self.cmd_mock.inst.Run()
    self.AssertOutputContainsLine('1.2.3', check_stderr=True)

  def testHandleSelfUpdateAfterRestart(self):
    """Tests that --update doesn't re-exec a second time."""
    os.environ[brillo_sdk._BRILLO_SDK_NO_UPDATE] = '1'
    self.SetupCommandMock(['--update', 'latest'])
    self.cmd_mock.inst.Run()

  def testVerifyEnvironmentAfterSelfUpdate(self):
    """Tests that environment verification happens after self-update."""
    update_bootstrap_mock = self.PatchObject(brillo_sdk, '_UpdateBootstrap')
    self.verify_environment_mock.side_effect = (
        lambda: update_bootstrap_mock.called)
    self.SetupCommandMock(['--update', 'latest'])
    self.cmd_mock.inst.Run()

  def testInvalidUpdateVersion(self):
    """Tests --update with an invalid SDK version."""
    update_bootstrap_mock = self.PatchObject(brillo_sdk, '_UpdateBootstrap')
    update_workspace_sdk_mock = self.PatchObject(brillo_sdk,
                                                 '_UpdateWorkspaceSdk')
    self.sdk_version_exists_mock.return_value = False

    self.SetupCommandMock(['--update', 'bad_version'])
    with self.assertRaises(cros_build_lib.DieSystemExit):
      self.cmd_mock.inst.Run()

    # None of the update functions should have been called.
    self.assertFalse(update_bootstrap_mock.called)
    self.assertFalse(self.verify_environment_mock.called)
    self.assertFalse(update_workspace_sdk_mock.called)

  def testVersionOption(self):
    """Tests that --version prints to stdout."""
    self.SetupCommandMock(['--version'])
    workspace_lib.SetActiveSdkVersion(self.workspace_path, 'foo')

    with self.OutputCapturer():
      self.cmd_mock.inst.Run()
    self.AssertOutputContainsLine('foo')

  def testVersionOptionSdkNotFound(self):
    """Tests that --version errors out if a version can't be found."""
    self.SetupCommandMock(['--version'])

    with self.OutputCapturer():
      with self.assertRaises(cros_build_lib.DieSystemExit):
        self.cmd_mock.inst.Run()
    self.AssertOutputContainsLine('This workspace does not have an SDK.',
                                  check_stderr=True, check_stdout=False)
