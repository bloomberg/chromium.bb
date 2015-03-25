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
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import project_sdk
from chromite.lib import workspace_lib

# Unittests often access internals.
# pylint: disable=protected-access


class ExecInvoked(Exception):
  """This exception shows that os.execv was invoked."""


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
    self.cmd_mock = None
    self.sdk_version = 'sdk_dir_version'
    self.workspace_version = 'work_version'

    self.bootstrap_path = os.path.join(self.tempdir, 'bootstrap')
    self.sdk_path = os.path.join(self.tempdir, 'sdk')
    self.workspace_path = os.path.join(self.tempdir, 'workspace')

    # Workspace is supposed to exist in advance.
    osutils.SafeMakedirs(self.workspace_path)

    self.PatchObject(bootstrap_lib, 'FindBootstrapPath',
                     return_value=self.bootstrap_path)
    self.PatchObject(project_sdk, 'FindVersion',
                     return_value=self.sdk_version)
    self.PatchObject(workspace_lib, 'GetActiveSdkVersion',
                     return_value=self.workspace_version)

    # Be super paranoid that we don't run this by mistake.
    self.mock_exec = self.PatchObject(
        commandline, 'ReExec', side_effect=ExecInvoked)

    # Prevent actually downloading a repository.
    self.mock_repo = self.PatchObject(repository, 'RepoRepository')

  def tearDown(self):
    if brillo_sdk.SdkCommand.BRILLO_SDK_NO_UPDATE in os.environ:
      del os.environ[brillo_sdk.SdkCommand.BRILLO_SDK_NO_UPDATE]

  def SetupCommandMock(self, cmd_args):
    """Sets up the command mock."""
    self.cmd_mock = MockSdkCommand(cmd_args)
    self.StartPatcher(self.cmd_mock)

  def testFindVersion(self):
    self.SetupCommandMock([])

    # If we only no the workspace, use the workspace version.
    self.assertEqual(
        self.workspace_version,
        self.cmd_mock.inst._FindVersion(self.workspace_path, None))

    # If we have an explicit sdk path, use it's version.
    self.assertEqual(
        self.sdk_version,
        self.cmd_mock.inst._FindVersion(None, self.sdk_path))
    self.assertEqual(
        self.sdk_version,
        self.cmd_mock.inst._FindVersion(self.workspace_path, self.sdk_path))

  def testHandleExplicitUpdate(self):
    self.SetupCommandMock([])

    self.cmd_mock.inst._HandleUpdate(
        self.bootstrap_path, None, self.sdk_path, '1.2.3')

    # Given the explicit path and version, sync what we expect, and where.
    expected = [mock.call(constants.MANIFEST_VERSIONS_GOB_URL,
                          self.sdk_path,
                          depth=1,
                          manifest='project-sdk/1.2.3.xml'),
                mock.call().Sync()]

    self.assertEqual(expected, self.mock_repo.mock_calls)

  def testHandleLatestUpdate(self):
    self.SetupCommandMock([])

    self.cmd_mock.inst._HandleUpdate(
        self.bootstrap_path, None, self.sdk_path, 'latest')

    # Given the explicit path and version, sync what we expect, and where.
    expected = [mock.call(constants.MANIFEST_VERSIONS_GOB_URL,
                          self.sdk_path,
                          depth=1,
                          manifest='project-sdk/latest.xml'),
                mock.call().Sync()]

    self.assertEqual(expected, self.mock_repo.mock_calls)

  def testHandleTotUpdate(self):
    self.SetupCommandMock([])

    self.cmd_mock.inst._HandleUpdate(
        self.bootstrap_path, None, self.sdk_path, 'tot')

    # Given the explicit path and version, sync what we expect, and where.
    expected = [mock.call(constants.MANIFEST_URL,
                          self.sdk_path,
                          depth=None,
                          manifest=constants.PROJECT_MANIFEST),
                mock.call().Sync()]

    self.assertEqual(expected, self.mock_repo.mock_calls)

  def testHandleWorkspaceUpdate(self):
    self.SetupCommandMock([])

    self.cmd_mock.inst._HandleUpdate(
        self.bootstrap_path, self.workspace_path, None, '1.2.3')

    # Given the explicit path and version, sync what we expect, and where.
    expected = [mock.call(constants.MANIFEST_VERSIONS_GOB_URL,
                          os.path.join(self.tempdir,
                                       'bootstrap/sdk_checkouts/1.2.3'),
                          depth=1,
                          manifest='project-sdk/1.2.3.xml'),
                mock.call().Sync()]

    self.assertEqual(expected, self.mock_repo.mock_calls)

  def testHandleSelfUpdateAndRestart(self):
    self.SetupCommandMock(['--update', 'latest', '--sdk-dir', self.tempdir])

    with self.assertRaises(ExecInvoked):
      self.cmd_mock.inst.Run()

  def testHandleNoUpdate(self):
    self.SetupCommandMock(['--sdk-dir', self.tempdir])

    # Make sure no exec assertion is raised if we aren't updating.
    self.cmd_mock.inst.Run()

  def testHandleSelfUpdateAfterRestart(self):
    os.environ[brillo_sdk.SdkCommand.BRILLO_SDK_NO_UPDATE] = '1'

    self.SetupCommandMock(['--update', 'latest', '--sdk-dir', self.tempdir])

    # Make sure no exec assertion is raised.
    self.cmd_mock.inst.Run()
