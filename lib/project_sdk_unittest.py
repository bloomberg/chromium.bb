# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for the project_sdk library."""

from __future__ import print_function

import os

from chromite.cbuildbot import constants
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import project_sdk

# pylint: disable=protected-access

class ProjectSdkTest(cros_test_lib.TempDirTestCase):
  """Unittest for project_sdk.py"""

  def setUp(self):
    self.version = '1.2.3'

    # Define assorted paths to test against.
    self.bogus_dir = os.path.join(self.tempdir, 'bogus')
    self.repo_dir = os.path.join(self.tempdir, 'repo')
    self.nested_dir = os.path.join(self.repo_dir, 'foo', 'bar')

    # Create fake repo with sub-dirs.
    osutils.SafeMakedirs(os.path.join(self.repo_dir, '.repo'))
    osutils.SafeMakedirs(self.nested_dir)

    version_file = project_sdk.VersionFile(self.repo_dir)
    osutils.WriteFile(version_file, self.version)

  def testFindRepoRootCurrentRepo(self):
    """Test FindRepoRoot with default of CWD."""
    self.assertEqual(constants.SOURCE_ROOT, project_sdk.FindRepoRoot())

  def testFindRepoRootSpecifiedBogus(self):
    """Test FindRepoRoot with non-existent directory outside the repo."""
    self.assertIsNone(project_sdk.FindRepoRoot(self.bogus_dir))

  def testFindRepoRootSpecifiedRoot(self):
    """Test FindRepoRoot with top level of repo tree."""
    self.assertEqual(self.repo_dir, project_sdk.FindRepoRoot(self.repo_dir))

  def testFindRepoRootSpecifiedNested(self):
    """Test FindRepoRoot with nested inside repo tree."""
    self.assertEqual(self.repo_dir, project_sdk.FindRepoRoot(self.nested_dir))

  def testFindRepoRootSpecifiedNonexistent(self):
    """Test FindRepoRoot refuses to scan a nonexistent path."""
    self.assertIsNone(
        project_sdk.FindRepoRoot(os.path.join(self.nested_dir, 'not_there')))

  def testVersionFile(self):
    self.assertEqual('/foo/SDK_VERSION', project_sdk.VersionFile('/foo'))

  def testFindVersionDefault(self):
    """Test FindVersion with default of CWD."""
    # Expected results are undefined, just ensure we don't crash.
    project_sdk.FindVersion()

  def testFindVersionBogus(self):
    """Test FindVersion with non-existent directory outside the repo."""
    self.assertIsNone(project_sdk.FindVersion(self.bogus_dir))

  def testFindVersionSpecifiedRoot(self):
    """Test FindVersion with top level of repo tree."""
    self.assertEqual(self.version, project_sdk.FindVersion(self.repo_dir))

  def testFindVersionSpecifiedNested(self):
    """Test FindVersion with nested inside repo tree."""
    self.assertEqual(self.version, project_sdk.FindVersion(self.nested_dir))
