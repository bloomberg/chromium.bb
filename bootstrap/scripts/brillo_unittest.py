# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test the bootstrap brillo command."""

from __future__ import print_function

import mock
import os

from chromite.lib import bootstrap_lib
from chromite.lib import cros_test_lib
from chromite.lib import git
from chromite.lib import workspace_lib

from chromite.bootstrap.scripts import brillo


class TestBootstrapBrilloCmd(cros_test_lib.MockTestCase):
  """Tests for the bootstrap brillo command."""

  def setUp(self):
    # Make certain we never exec anything.
    self.mock_exec = self.PatchObject(os, 'execv', autospec=True)

    # Define some directories that might be looked up.
    self.mock_bootstrap_path = self.PatchObject(
        bootstrap_lib, 'FindBootstrapPath', autospec=True,
        return_value='/bootstrap')

    self.mock_workspace_path = self.PatchObject(
        workspace_lib, 'WorkspacePath', autospec=True)

    self.mock_workspace_active_path = self.PatchObject(
        bootstrap_lib, 'GetActiveSdkPath', autospec=True)

    self.mock_repo_root = self.PatchObject(
        git, 'FindRepoCheckoutRoot', autospec=True)

  def _LocateCmdTester(self, args, expected):
    """Helper for testing what brillo.main execs."""
    result = brillo.LocateBrilloCommand(args)
    self.assertEqual(expected, result)

  def _verifyLocateBrilloCommand(self, expected):
    # We should always use the bootstrap brillo command for 'sdk'.
    self.assertEqual(expected,
                     brillo.LocateBrilloCommand(['flash']))
    self.assertEqual(expected,
                     brillo.LocateBrilloCommand(['flash', '--help']))

  def _verifyLocateBrilloCommandSpecialSdkHandling(self):
    # We should always use the bootstrap brillo command for 'sdk'.
    self.assertEqual('/bootstrap/bin/brillo',
                     brillo.LocateBrilloCommand(['sdk']))
    self.assertEqual('/bootstrap/bin/brillo',
                     brillo.LocateBrilloCommand(['sdk', '--help']))

  def testCommandLookupActiveWorkspace(self):
    """Test that sdk commands are run in the Git Repository."""
    self.mock_workspace_path.return_value = '/workspace'
    self.mock_workspace_active_path.return_value = '/sdk_roots/1.2.3'
    self.mock_repo_root.return_value = None

    self._verifyLocateBrilloCommand('/sdk_roots/1.2.3/chromite/bin/brillo')
    self._verifyLocateBrilloCommandSpecialSdkHandling()

    # Having a repo root shouldn't affect the result.
    self.mock_repo_root.return_value = '/repo'

    self._verifyLocateBrilloCommand('/sdk_roots/1.2.3/chromite/bin/brillo')
    self._verifyLocateBrilloCommandSpecialSdkHandling()

  def testCommandLookupInactiveWorkspace(self):
    """Test that sdk commands are run in the Git Repository."""
    self.mock_workspace_path.return_value = '/workspace'
    self.mock_workspace_active_path.return_value = None
    self.mock_repo_root.return_value = None

    self._verifyLocateBrilloCommand(None)
    self._verifyLocateBrilloCommandSpecialSdkHandling()

    # Having a repo root shouldn't affect the result.
    self.mock_repo_root.return_value = '/repo'

    self._verifyLocateBrilloCommand(None)
    self._verifyLocateBrilloCommandSpecialSdkHandling()

  def testCommandLookupRepo(self):
    """Test that sdk commands are run in the Git Repository."""
    self.mock_workspace_path.return_value = None
    self.mock_workspace_active_path.return_value = None
    self.mock_repo_root.return_value = '/repo'

    self._verifyLocateBrilloCommand('/repo/chromite/bin/brillo')
    self._verifyLocateBrilloCommandSpecialSdkHandling()

  def testCommandLookupBootstrapOnly(self):
    """Test that sdk commands are run in the Git Repository."""
    self.mock_workspace_path.return_value = None
    self.mock_workspace_active_path.return_value = None
    self.mock_repo_root.return_value = None

    self._verifyLocateBrilloCommand(None)
    self._verifyLocateBrilloCommandSpecialSdkHandling()

  def testMainInActiveWorkspace(self):
    self.mock_workspace_path.return_value = '/workspace'
    self.mock_workspace_active_path.return_value = '/sdk_roots/1.2.3'
    self.mock_repo_root.return_value = None

    brillo.main(['flash', '--help'])

    expected_cmd = '/sdk_roots/1.2.3/chromite/bin/brillo'

    self.assertEqual(
        [mock.call(expected_cmd, [expected_cmd, 'flash', '--help'])],
        self.mock_exec.call_args_list)

  def testMainInRepo(self):
    self.mock_workspace_path.return_value = None
    self.mock_workspace_active_path.return_value = None
    self.mock_repo_root.return_value = '/repo'

    brillo.main(['flash', '--help'])

    expected_cmd = '/repo/chromite/bin/brillo'

    self.assertEqual(
        [mock.call(expected_cmd, [expected_cmd, 'flash', '--help'])],
        self.mock_exec.call_args_list)

  def testMainNoCmd(self):
    self.mock_workspace_path.return_value = None
    self.mock_workspace_active_path.return_value = None
    self.mock_repo_root.return_value = None

    self.assertEqual(1, brillo.main(['flash', '--help']))
    self.assertEqual([], self.mock_exec.call_args_list)

  def testMainSdkCmd(self):
    self.mock_workspace_path.return_value = None
    self.mock_workspace_active_path.return_value = None
    self.mock_repo_root.return_value = None

    brillo.main(['sdk', '--help'])

    expected_cmd = '/bootstrap/bin/brillo'

    self.assertEqual(
        [mock.call(expected_cmd, [expected_cmd, 'sdk', '--help'])],
        self.mock_exec.call_args_list)
