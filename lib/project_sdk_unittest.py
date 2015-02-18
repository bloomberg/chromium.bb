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

    # Setup manifest with symlink pointer.
    real_manifest_path = os.path.join(
        self.repo_dir, '.repo', 'sub-dir', '%s.xml' % self.version)
    sym_manifest_path = os.path.join(self.repo_dir, '.repo', 'manifest.xml')

    osutils.Touch(real_manifest_path, makedirs=True)
    os.symlink(real_manifest_path, sym_manifest_path)

    # Build a repo with a malformed version.
    self.malformed_version = 'foobar'
    self.malformed_repo_dir = os.path.join(self.tempdir, 'malformed_repo')
    osutils.SafeMakedirs(os.path.join(self.malformed_repo_dir, '.repo'))
    real_manifest_path = os.path.join(
        self.malformed_repo_dir, '.repo', 'sub-dir',
        '%s.xml' % self.malformed_version)
    sym_manifest_path = os.path.join(self.malformed_repo_dir, '.repo',
                                     'manifest.xml')
    osutils.Touch(real_manifest_path, makedirs=True)
    os.symlink(real_manifest_path, sym_manifest_path)

  def testFindSourceRootCurrentRepo(self):
    """Test FindSourceRoot with default of CWD."""
    self.assertEqual(constants.SOURCE_ROOT, project_sdk.FindSourceRoot())

  def testFindSourceRootSpecifiedBogus(self):
    """Test FindSourceRoot with non-existent directory outside the repo."""
    self.assertIsNone(project_sdk.FindSourceRoot(self.bogus_dir))

  def testFindSourceRootSpecifiedRoot(self):
    """Test FindSourceRoot with top level of repo tree."""
    self.assertEqual(self.repo_dir, project_sdk.FindSourceRoot(self.repo_dir))

  def testFindSourceRootSpecifiedNested(self):
    """Test FindSourceRoot with nested inside repo tree."""
    self.assertEqual(self.repo_dir, project_sdk.FindSourceRoot(self.nested_dir))

  def testFindSourceRootSpecifiedNonexistent(self):
    """Test FindSourceRoot refuses to scan a nonexistent path."""
    self.assertIsNone(
        project_sdk.FindSourceRoot(os.path.join(self.nested_dir, 'not_there')))

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

  def testFindVersionMalformed(self):
    """Test FindVersion with malformed version string."""
    self.assertIsNone(project_sdk.FindVersion(self.malformed_repo_dir))
