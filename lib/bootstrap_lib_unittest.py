# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for the project_sdk library."""

from __future__ import print_function

import mock
import os

from chromite.cbuildbot import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import bootstrap_lib
from chromite.lib import project_sdk
from chromite.lib import workspace_lib

# pylint: disable=protected-access

class ProjectSdkTest(cros_test_lib.WorkspaceTestCase):
  """Unittest for bootstrap_lib.py"""

  def setUp(self):
    self.version = '1.2.3'

    # Don't use "CreateBootstrap" since it mocks out the method we are testing.
    self.bootstrap_path = os.path.join(self.tempdir, 'bootstrap')
    self.CreateWorkspace()


  @mock.patch.object(project_sdk, 'FindRepoRoot')
  @mock.patch.object(cros_build_lib, 'IsInsideChroot')
  def _RunFindBootstrapPath(self, env, repo, chroot,
                            expected_path, expected_env,
                            mock_chroot, mock_repo):

    orig_env = os.environ.copy()

    try:
      # Setup the ENV as requested.
      if env is not None:
        os.environ[bootstrap_lib.BOOTSTRAP_PATH_ENV] = env
      else:
        os.environ.pop(bootstrap_lib.BOOTSTRAP_PATH_ENV, None)

      # Setup mocks, as requested.
      mock_repo.return_value = repo
      mock_chroot.return_value = chroot

      # Verify that ENV is modified, if save is False.
      self.assertEqual(bootstrap_lib.FindBootstrapPath(), expected_path)
      self.assertEqual(os.environ.get(bootstrap_lib.BOOTSTRAP_PATH_ENV), env)

      # The test environment is fully setup, run the test.
      self.assertEqual(bootstrap_lib.FindBootstrapPath(True), expected_path)
      self.assertEqual(os.environ.get(bootstrap_lib.BOOTSTRAP_PATH_ENV),
                       expected_env)

    finally:
      # Restore the ENV.
      osutils.SetEnvironment(orig_env)


  def testFindBootstrapPath(self):
    real_result = constants.CHROMITE_DIR

    # Test first call in a bootstrap env. Exact results not verified.
    self._RunFindBootstrapPath(None, None, False,
                               real_result, real_result)

    # Test first call after bootstrap outside an SDK. Not an expected env.
    self._RunFindBootstrapPath('/foo', None, False,
                               '/foo', '/foo')

    # Test first call after bootstrap inside an SDK.
    self._RunFindBootstrapPath('/foo', '/', False,
                               '/foo', '/foo')

    # Test first call without bootstrap inside an SDK. Error Case.
    self._RunFindBootstrapPath(None, '/', False,
                               None, None)

    # Test all InsideChroot Cases.
    self._RunFindBootstrapPath(None, None, True,
                               None, None)
    self._RunFindBootstrapPath('/foo', None, True,
                               None, '/foo')
    self._RunFindBootstrapPath('/foo', '/', True,
                               None, '/foo')
    self._RunFindBootstrapPath(None, '/', True,
                               None, None)

  def testComputeSdkPath(self):
    # Try to compute path, with no valid bootstrap path.
    self.assertEqual(None, bootstrap_lib.ComputeSdkPath(None, '1.2.3'))

    self.assertEqual(
        '/foo/bootstrap/sdk_checkouts/1.2.3',
        bootstrap_lib.ComputeSdkPath('/foo/bootstrap', '1.2.3'))

  def testGetActiveSdkPath(self):
    # Try to find SDK Path with no valid bootstrap path.
    sdk_dir = bootstrap_lib.GetActiveSdkPath(None,
                                             self.workspace_path)
    self.assertEqual(None, sdk_dir)

    # Try to find SDK Path of workspace with no active SDK.
    sdk_dir = bootstrap_lib.GetActiveSdkPath(self.bootstrap_path,
                                             self.workspace_path)
    self.assertEqual(None, sdk_dir)

    # Try to find SDK Path of workspace with active SDK, but SDK doesn't exist.
    workspace_lib.SetActiveSdkVersion(self.workspace_path, self.version)
    sdk_dir = bootstrap_lib.GetActiveSdkPath(self.bootstrap_path,
                                             self.workspace_path)
    self.assertEqual(None, sdk_dir)

    # 'Create' the active SDK.
    expected_sdk_root = bootstrap_lib.ComputeSdkPath(self.bootstrap_path,
                                                     self.version)
    osutils.SafeMakedirs(expected_sdk_root)

    # Verify that we can Find it now.
    sdk_dir = bootstrap_lib.GetActiveSdkPath(self.bootstrap_path,
                                             self.workspace_path)
    self.assertEqual(expected_sdk_root, sdk_dir)
