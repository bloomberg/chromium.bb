# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for the project_sdk library."""

from __future__ import print_function

import os

from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import bootstrap_lib
from chromite.lib import workspace_lib

# pylint: disable=protected-access

class ProjectSdkTest(cros_test_lib.WorkspaceTestCase):
  """Unittest for bootstrap_lib.py"""

  def setUp(self):
    self.version = '1.2.3'

    # Don't use "CreateBootstrap" since it mocks out the method we are testing.
    self.bootstrap_path = os.path.join(self.tempdir, 'bootstrap')
    self.CreateWorkspace()

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
