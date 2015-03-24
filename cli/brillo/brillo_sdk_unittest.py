# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module tests the brillo sdk command."""

from __future__ import print_function

import mock
import os

from chromite.cbuildbot import constants
from chromite.cbuildbot import repository
from chromite.cli.cros import cros_sdk
from chromite.lib import bootstrap_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import project_sdk
from chromite.lib import workspace_lib

# Unittests often access internals.
# pylint: disable=protected-access

class CrosSdkTest(cros_test_lib.MockTempDirTestCase):
  """Test class for our BuildCommand class."""

  def setUp(self):
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

    # Prevent actually downloading a repository.
    self.mock_repo = self.PatchObject(repository, 'RepoRepository')

  def testFindVersion(self):
    # If we only no the workspace, use the workspace version.
    self.assertEqual(
        self.workspace_version,
        cros_sdk.SdkCommand._FindVersion(self.workspace_path, None))

    # If we have an explicit sdk path, use it's version.
    self.assertEqual(
        self.sdk_version,
        cros_sdk.SdkCommand._FindVersion(None, self.sdk_path))
    self.assertEqual(
        self.sdk_version,
        cros_sdk.SdkCommand._FindVersion(self.workspace_path, self.sdk_path))

  def testHandleExplicitUpdate(self):
    cros_sdk.SdkCommand._HandleUpdate(None, self.sdk_path, '1.2.3')

    # Given the explicit path and version, sync what we expect, and where.
    expected = [mock.call(constants.MANIFEST_VERSIONS_GOB_URL,
                          self.sdk_path,
                          depth=1,
                          manifest='project-sdk/1.2.3.xml'),
                mock.call().Sync()]

    self.assertEqual(expected, self.mock_repo.mock_calls)

  def testHandleLatestUpdate(self):
    cros_sdk.SdkCommand._HandleUpdate(None, self.sdk_path, 'latest')

    # Given the explicit path and version, sync what we expect, and where.
    expected = [mock.call(constants.MANIFEST_VERSIONS_GOB_URL,
                          self.sdk_path,
                          depth=1,
                          manifest='project-sdk/latest.xml'),
                mock.call().Sync()]

    self.assertEqual(expected, self.mock_repo.mock_calls)

  def testHandleTotUpdate(self):
    cros_sdk.SdkCommand._HandleUpdate(None, self.sdk_path, 'tot')

    # Given the explicit path and version, sync what we expect, and where.
    expected = [mock.call(constants.MANIFEST_URL,
                          self.sdk_path,
                          depth=None,
                          manifest=constants.PROJECT_MANIFEST),
                mock.call().Sync()]

    self.assertEqual(expected, self.mock_repo.mock_calls)

  def testHandleWorkspaceUpdate(self):
    cros_sdk.SdkCommand._HandleUpdate(self.workspace_path, None, '1.2.3')

    # Given the explicit path and version, sync what we expect, and where.
    expected = [mock.call(constants.MANIFEST_VERSIONS_GOB_URL,
                          os.path.join(self.tempdir,
                                       'bootstrap/sdk_checkouts/1.2.3'),
                          depth=1,
                          manifest='project-sdk/1.2.3.xml'),
                mock.call().Sync()]

    self.assertEqual(expected, self.mock_repo.mock_calls)
