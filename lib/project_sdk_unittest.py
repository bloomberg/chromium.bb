# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for the project_sdk library."""

from __future__ import print_function

import os

from chromite.cbuildbot import constants
from chromite.lib import cros_build_lib_unittest
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


class ProjectSdkVerifyFake(cros_test_lib.MockTempDirTestCase):
  """Test VerifyEnvironment with mocks."""

  def setUp(self):
    self.rc_mock = self.StartPatcher(cros_build_lib_unittest.RunCommandMock())

  def testGTrusty(self):
    """Test with mock of 'gTrusty' distribution."""
    self.rc_mock.AddCmdResult(['/bin/bash', '--version'])
    self.rc_mock.AddCmdResult(['git', '--version'],
                              output='git version 2.2.0.rc0.207.ga3a616c')
    self.rc_mock.AddCmdResult(['curl', '--version'])

    self.assertTrue(project_sdk.VerifyEnvironment(self.tempdir))

  def testUbuntu14(self):
    """Test with mock of 'Ubuntu LTS 14' distribution."""
    self.rc_mock.AddCmdResult(['/bin/bash', '--version'])
    self.rc_mock.AddCmdResult(['git', '--version'],
                              output='git version 2.1.0')
    self.rc_mock.AddCmdResult(['curl', '--version'])

    self.assertTrue(project_sdk.VerifyEnvironment(self.tempdir))

  def testGitNewEnough(self):
    """Test with mock of git 1.8."""
    self.rc_mock.AddCmdResult(['/bin/bash', '--version'])
    self.rc_mock.AddCmdResult(['git', '--version'],
                              output='git version 1.8.3.1')
    self.rc_mock.AddCmdResult(['curl', '--version'])

    self.assertTrue(project_sdk.VerifyEnvironment(self.tempdir))

  def testFailNoBash(self):
    """Test with mock of no bash present."""
    self.rc_mock.AddCmdResult(['/bin/bash', '--version'], returncode=127)
    self.rc_mock.AddCmdResult(['git', '--version'],
                              output='git version 2.2.0.rc0.207.ga3a616c')
    self.rc_mock.AddCmdResult(['curl', '--version'])

    self.assertFalse(project_sdk.VerifyEnvironment(self.tempdir))

  def testFailGitTooOld(self):
    """Test with mock of git too old to use."""
    self.rc_mock.AddCmdResult(['/bin/bash', '--version'])
    self.rc_mock.AddCmdResult(['git', '--version'],
                              output='git version 1.7.10.4')
    self.rc_mock.AddCmdResult(['curl', '--version'])

    self.assertFalse(project_sdk.VerifyEnvironment(self.tempdir))

  def testFailNoCurl(self):
    """Test with mock of no curl present."""
    self.rc_mock.AddCmdResult(['/bin/bash', '--version'])
    self.rc_mock.AddCmdResult(['git', '--version'],
                              output='git version 2.2.0.rc0.207.ga3a616c')
    self.rc_mock.AddCmdResult(['curl', '--version'], returncode=127)

    self.assertFalse(project_sdk.VerifyEnvironment(self.tempdir))

  def testFailSuid(self):
    """Test with SUID is disabled."""
    self.rc_mock.AddCmdResult(['/bin/bash', '--version'])
    self.rc_mock.AddCmdResult(['git', '--version'],
                              output='git version 2.2.0.rc0.207.ga3a616c')
    self.rc_mock.AddCmdResult(['curl', '--version'])

    mock_stat = self.PatchObject(os, 'statvfs', autospec=True)

    # The os.ST_NOSUID constant wasn't added until python-3.2.
    mock_stat.return_value.f_flag = 0x2

    self.assertFalse(project_sdk.VerifyEnvironment(self.tempdir))

class ProjectSdkVerifyReal(cros_test_lib.TempDirTestCase):
  """Test VerifyEnvironment for real."""

  def testVerifyEnvironment(self):
    """Test, assuming production environment is valid."""
    self.assertTrue(project_sdk.VerifyEnvironment(self.tempdir))

  def testGetExecutableVersionNonExistent(self):
    """Tests _GetExecutableVersion() when the executable doesn't exist."""
    self.assertIsNone(project_sdk._GetExecutableVersion('/not/a/real/program'))
