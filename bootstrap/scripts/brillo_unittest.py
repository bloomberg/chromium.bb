# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test the bootstrap brillo command."""

from __future__ import print_function

import mock
import os

from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import git

from chromite.bootstrap.scripts import brillo


class TestBootstrapBrilloCmd(cros_test_lib.WorkspaceTestCase):
  """Tests for the bootstrap brillo command."""

  def setUp(self):
    # Make certain we never exec anything.
    self.mock_exec = self.PatchObject(os, 'execv', autospec=True)

    self.mock_repo_root = self.PatchObject(
        git, 'FindRepoCheckoutRoot', autospec=True)

  def _verifyLocateBrilloCommand(self, expected):
    self.assertEqual(expected,
                     brillo.LocateBrilloCommand(['flash']))
    self.assertEqual(expected,
                     brillo.LocateBrilloCommand(['flash', '--help']))

  def _verifyLocateBrilloCommandSdkHandling(self, expected):
    self.assertEqual(expected,
                     brillo.LocateBrilloCommand(['sdk']))
    self.assertEqual(expected,
                     brillo.LocateBrilloCommand(['sdk', '--help']))

  def _verifyLocateBrilloCommandFail(self):
    with self.assertRaises(cros_build_lib.DieSystemExit):
      brillo.LocateBrilloCommand(['flash'])

  def _verifyLocateBrilloCommandSdkFail(self):
    with self.assertRaises(cros_build_lib.DieSystemExit):
      brillo.LocateBrilloCommand(['sdk'])

  def testCommandLookupActiveWorkspace(self):
    """Test that sdk commands are run in the Git Repository."""
    self.CreateBootstrap('1.2.3')
    self.CreateWorkspace('1.2.3')

    sdk_wrapper = os.path.join(
        self.bootstrap_path, 'sdk_checkouts/1.2.3/chromite/bin/brillo')
    bootstrap_wrapper = os.path.join(self.bootstrap_path, 'bin/brillo')

    # We are not inside a repo.
    self.mock_repo_root.return_value = None

    self._verifyLocateBrilloCommand(sdk_wrapper)
    self._verifyLocateBrilloCommandSdkHandling(bootstrap_wrapper)

    # We are inside a repo, shouldn't affect the result.
    self.mock_repo_root.return_value = '/repo'

    self._verifyLocateBrilloCommand(sdk_wrapper)
    self._verifyLocateBrilloCommandSdkHandling(bootstrap_wrapper)

  def testCommandLookupInactiveWorkspace(self):
    """Test that sdk commands are run in the Git Repository."""
    self.CreateBootstrap()
    self.CreateWorkspace()
    self.mock_repo_root.return_value = None

    bootstrap_wrapper = os.path.join(self.bootstrap_path, 'bin/brillo')

    self._verifyLocateBrilloCommandFail()
    self._verifyLocateBrilloCommandSdkHandling(bootstrap_wrapper)

    # Having a repo root shouldn't affect the result.
    self.mock_repo_root.return_value = '/repo'

    self._verifyLocateBrilloCommandFail()
    self._verifyLocateBrilloCommandSdkHandling(bootstrap_wrapper)

  def testCommandLookupRepoFromBootstrap(self):
    """Test that sdk commands are run in the Git Repository."""
    self.CreateBootstrap('1.2.3')
    self.CreateWorkspace()
    self.mock_workspace_path.return_value = None
    self.mock_repo_root.return_value = '/repo'

    bootstrap_wrapper = os.path.join(self.bootstrap_path, 'bin/brillo')
    repo_wrapper = '/repo/chromite/bin/brillo'

    self._verifyLocateBrilloCommand(repo_wrapper)
    self._verifyLocateBrilloCommandSdkHandling(bootstrap_wrapper)

  def testCommandLookupBootstrapOnly(self):
    """Test that sdk commands are run in the Git Repository."""
    self.CreateBootstrap('1.2.3')
    self.CreateWorkspace()
    self.mock_workspace_path.return_value = None
    self.mock_repo_root.return_value = None

    bootstrap_wrapper = os.path.join(self.bootstrap_path, 'bin/brillo')

    self._verifyLocateBrilloCommandFail()
    self._verifyLocateBrilloCommandSdkHandling(bootstrap_wrapper)

  def testCommandLookupRepoOnly(self):
    """Test that sdk commands are run in the Git Repository."""
    self.CreateBootstrap('1.2.3')
    self.CreateWorkspace()
    self.mock_bootstrap_path.return_value = None
    self.mock_workspace_path.return_value = None
    self.mock_repo_root.return_value = '/repo'

    repo_wrapper = '/repo/chromite/bin/brillo'

    self._verifyLocateBrilloCommand(repo_wrapper)
    self._verifyLocateBrilloCommandSdkFail()

  def testMainInActiveWorkspace(self):
    self.CreateBootstrap('1.2.3')
    self.CreateWorkspace('1.2.3')
    self.mock_repo_root.return_value = None

    brillo.main(['flash', '--help'])

    expected_cmd = os.path.join(
        self.bootstrap_path, 'sdk_checkouts/1.2.3/chromite/bin/brillo')

    self.assertEqual(
        [mock.call(expected_cmd, [expected_cmd, 'flash', '--help'])],
        self.mock_exec.call_args_list)

  def testMainInRepo(self):
    self.CreateBootstrap('1.2.3')
    self.CreateWorkspace('1.2.3')
    self.mock_workspace_path.return_value = None
    self.mock_repo_root.return_value = '/repo'

    brillo.main(['flash', '--help'])

    expected_cmd = '/repo/chromite/bin/brillo'

    self.assertEqual(
        [mock.call(expected_cmd, [expected_cmd, 'flash', '--help'])],
        self.mock_exec.call_args_list)

  def testMainNoCmd(self):
    self.CreateBootstrap('1.2.3')
    self.CreateWorkspace('1.2.3')
    self.mock_workspace_path.return_value = None
    self.mock_repo_root.return_value = None

    with self.assertRaises(cros_build_lib.DieSystemExit):
      brillo.main(['flash', '--help'])

    self.assertEqual([], self.mock_exec.call_args_list)

  def testMainSdkCmd(self):
    self.CreateBootstrap('1.2.3')
    self.CreateWorkspace('1.2.3')
    self.mock_workspace_path.return_value = None
    self.mock_repo_root.return_value = None

    brillo.main(['sdk', '--help'])

    expected_cmd = os.path.join(self.bootstrap_path, 'bin/brillo')

    self.assertEqual(
        [mock.call(expected_cmd, [expected_cmd, 'sdk', '--help'])],
        self.mock_exec.call_args_list)
