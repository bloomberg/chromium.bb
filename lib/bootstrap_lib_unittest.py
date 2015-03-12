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

class ProjectSdkTest(cros_test_lib.TempDirTestCase):
  """Unittest for bootstrap_lib.py"""

  def setUp(self):
    self.version = '1.2.3'

    # Define assorted paths to test against.
    self.bootstrap_dir = os.path.join(self.tempdir, 'bootstrap')

    self.workspace_dir = os.path.join(self.tempdir, 'workspace')
    self.workspace_config = os.path.join(self.workspace_dir,
                                         workspace_lib.WORKSPACE_CONFIG)

    # Create workspace directories and files.
    osutils.Touch(self.workspace_config, makedirs=True)

  def testFindBootstrapPath(self):
    # Test is weak, but so is the method tested.
    _ = bootstrap_lib.FindBootstrapPath()

  def testComputeSdkPath(self):
    self.assertEqual(
        '/foo/bootstrap/sdk_checkouts/1.2.3',
        bootstrap_lib.ComputeSdkPath('/foo/bootstrap', '1.2.3'))

  def testGetActiveSdkPath(self):
    # Try to find SDK Path of workspace with no active SDK.
    sdk_dir = bootstrap_lib.GetActiveSdkPath(self.bootstrap_dir,
                                             self.workspace_dir)
    self.assertEqual(None, sdk_dir)

    # Try to find SDK Path of workspace with active SDK, but SDK doesn't exist.
    workspace_lib.SetActiveSdkVersion(self.workspace_dir, self.version)
    sdk_dir = bootstrap_lib.GetActiveSdkPath(self.bootstrap_dir,
                                             self.workspace_dir)
    self.assertEqual(None, sdk_dir)

    # 'Create' the active SDK.
    expected_sdk_root = bootstrap_lib.ComputeSdkPath(self.bootstrap_dir,
                                                     self.version)
    osutils.SafeMakedirs(expected_sdk_root)

    # Verify that we can Find it now.
    sdk_dir = bootstrap_lib.GetActiveSdkPath(self.bootstrap_dir,
                                             self.workspace_dir)
    self.assertEqual(expected_sdk_root, sdk_dir)
