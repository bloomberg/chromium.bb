# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for the workspace_lib library."""

from __future__ import print_function

import mock
import os

from chromite.cbuildbot import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import workspace_lib

# pylint: disable=protected-access

class WorkspaceLibTest(cros_test_lib.TempDirTestCase):
  """Unittests for workspace_lib.py"""

  def setUp(self):
    # Define assorted paths to test against.
    self.bogus_dir = os.path.join(self.tempdir, 'bogus')

    self.workspace_dir = os.path.join(self.tempdir, 'workspace')
    self.workspace_config = os.path.join(self.workspace_dir,
                                         workspace_lib.WORKSPACE_CONFIG)
    self.workspace_nested = os.path.join(self.workspace_dir, 'foo', 'bar')
    # Create workspace directories and files.
    osutils.Touch(self.workspace_config, makedirs=True)
    osutils.SafeMakedirs(self.workspace_nested)

  @mock.patch('os.getcwd')
  @mock.patch.object(cros_build_lib, 'IsInsideChroot', return_value=False)
  def testWorkspacePathOutsideChroot(self, _mock_inside, mock_cwd):
    # Set default to a dir outside the workspace.
    mock_cwd.return_value = self.bogus_dir

    # Inside the workspace, specified dir.
    self.assertEqual(self.workspace_dir,
                     workspace_lib.WorkspacePath(self.workspace_dir))
    self.assertEqual(self.workspace_dir,
                     workspace_lib.WorkspacePath(self.workspace_nested))

    # Outside the workspace, specified dir.
    self.assertEqual(None, workspace_lib.WorkspacePath(self.tempdir))
    self.assertEqual(None, workspace_lib.WorkspacePath(self.bogus_dir))

    # Inside the workspace, default dir.
    mock_cwd.return_value = self.workspace_dir
    self.assertEqual(self.workspace_dir, workspace_lib.WorkspacePath())

    mock_cwd.return_value = self.workspace_nested
    self.assertEqual(self.workspace_dir, workspace_lib.WorkspacePath())

    # Outside the workspace, default dir.
    mock_cwd.return_value = self.tempdir
    self.assertEqual(None, workspace_lib.WorkspacePath())

    mock_cwd.return_value = self.bogus_dir
    self.assertEqual(None, workspace_lib.WorkspacePath())

  @mock.patch.object(cros_build_lib, 'IsInsideChroot', return_value=True)
  def testWorkspacePathInsideChroot(self, _mock_inside):
    orig_root = constants.CHROOT_WORKSPACE_ROOT
    try:
      # Set default to a dir outside the workspace.
      constants.CHROOT_WORKSPACE_ROOT = self.bogus_dir

      # Inside the workspace, specified dir.
      self.assertEqual(self.workspace_dir,
                       workspace_lib.WorkspacePath(self.workspace_dir))
      self.assertEqual(self.workspace_dir,
                       workspace_lib.WorkspacePath(self.workspace_nested))

      # Outside the workspace, specified dir.
      self.assertEqual(None, workspace_lib.WorkspacePath(self.tempdir))
      self.assertEqual(None, workspace_lib.WorkspacePath(self.bogus_dir))

      # Inside the workspace, default dir.
      constants.CHROOT_WORKSPACE_ROOT = self.workspace_dir
      self.assertEqual(self.workspace_dir, workspace_lib.WorkspacePath())

      constants.CHROOT_WORKSPACE_ROOT = self.workspace_nested
      self.assertEqual(self.workspace_dir, workspace_lib.WorkspacePath())

      # Outside the workspace, default dir.
      constants.CHROOT_WORKSPACE_ROOT = self.tempdir
      self.assertEqual(None, workspace_lib.WorkspacePath())

      constants.CHROOT_WORKSPACE_ROOT = self.bogus_dir
      self.assertEqual(None, workspace_lib.WorkspacePath())

    finally:
      # Restore our constant to it's real value.
      constants.CHROOT_WORKSPACE_ROOT = orig_root

  def testChrootPath(self):
    # Check the default value.
    self.assertEqual(os.path.join(self.workspace_dir, '.chroot'),
                     workspace_lib.ChrootPath(self.workspace_dir))

    # Set a new absolute value, check that we get it back.
    workspace_lib.SetChrootDir(self.workspace_dir, self.bogus_dir)
    self.assertEqual(self.bogus_dir,
                     workspace_lib.ChrootPath(self.workspace_dir))

    # Set a new relative path, check that it is properly appended to the
    # workspace path.
    workspace_lib.SetChrootDir(self.workspace_dir, 'some/path')
    self.assertEqual(os.path.join(self.workspace_dir, 'some/path'),
                     workspace_lib.ChrootPath(self.workspace_dir))

  @mock.patch.object(osutils, 'IsInsideVm', return_value=True)
  def testChrootPathUnderVm(self, _mock_inside_vm):
    """Make sure that inside the VM, chroot dir is under /chroots/..."""
    self.assertEqual(
        os.path.join(workspace_lib.MAIN_CHROOT_DIR_IN_VM,
                     os.path.basename(self.workspace_dir)),
        workspace_lib.ChrootPath(self.workspace_dir)
    )

  def testReadWriteLocalConfig(self):
    # Non-existent config should read as an empty dictionary.
    config = workspace_lib._ReadLocalConfig(self.workspace_dir)
    self.assertEqual({}, config)

    # Write out an empty dict, and make sure we can read it back.
    workspace_lib._WriteLocalConfig(self.workspace_dir, {})
    config = workspace_lib._ReadLocalConfig(self.workspace_dir)
    self.assertEqual({}, config)

    # Write out a value, and verify we can read it.
    workspace_lib._WriteLocalConfig(self.workspace_dir, {'version': 'foo'})
    config = workspace_lib._ReadLocalConfig(self.workspace_dir)
    self.assertEqual({'version': 'foo'}, config)

    # Overwrite value, and verify we can read it.
    workspace_lib._WriteLocalConfig(self.workspace_dir, {'version': 'bar'})
    config = workspace_lib._ReadLocalConfig(self.workspace_dir)
    self.assertEqual({'version': 'bar'}, config)

  def testReadWriteActiveSdkVersion(self):
    # If no version is set, value should be None.
    version = workspace_lib.GetActiveSdkVersion(self.workspace_dir)
    self.assertEqual(None, version)

    # Set value, and make sure we can read it.
    workspace_lib.SetActiveSdkVersion(self.workspace_dir, 'foo')
    version = workspace_lib.GetActiveSdkVersion(self.workspace_dir)
    self.assertEqual('foo', version)

    # Set different value, and make sure we can read it.
    workspace_lib.SetActiveSdkVersion(self.workspace_dir, 'bar')
    version = workspace_lib.GetActiveSdkVersion(self.workspace_dir)
    self.assertEqual('bar', version)

    # Create config with unrelated values, should be same as no config.
    workspace_lib._WriteLocalConfig(self.workspace_dir, {'foo': 'bar'})
    version = workspace_lib.GetActiveSdkVersion(self.workspace_dir)
    self.assertEqual(None, version)

    # Set version, and make sure it works.
    workspace_lib.SetActiveSdkVersion(self.workspace_dir, '1.2.3')
    version = workspace_lib.GetActiveSdkVersion(self.workspace_dir)
    self.assertEqual('1.2.3', version)

    # Ensure all of config is there afterwords.
    config = workspace_lib._ReadLocalConfig(self.workspace_dir)
    self.assertEqual({'version': '1.2.3', 'foo': 'bar'}, config)

  @mock.patch('os.getcwd')
  @mock.patch.object(cros_build_lib, 'IsInsideChroot', return_value=False)
  def testPathToLocator(self, _mock_inside, mock_cwd):
    """Tests the path to locator conversion."""
    ws = self.workspace_dir
    mock_cwd.return_value = ws

    foo_path = workspace_lib.PathToLocator(os.path.join(ws, 'foo'))
    baz_path = workspace_lib.PathToLocator(os.path.join(ws, 'bar', 'foo',
                                                        'baz'))
    daisy_path = workspace_lib.PathToLocator(os.path.join(constants.SOURCE_ROOT,
                                                          'src', 'overlays',
                                                          'overlay-daisy'))
    some_path = workspace_lib.PathToLocator(os.path.join(constants.SOURCE_ROOT,
                                                         'srcs', 'bar'))

    self.assertEqual('//foo', foo_path)
    self.assertEqual('//bar/foo/baz', baz_path)
    self.assertEqual('board:daisy', daisy_path)
    self.assertEqual(None, some_path)

    def assertReversible(loc):
      path = workspace_lib.LocatorToPath(loc)
      self.assertEqual(loc, workspace_lib.PathToLocator(path))

    assertReversible('//foo')
    assertReversible('//foo/bar/baz')
    assertReversible('board:gizmo')


class ConfigurationTest(cros_test_lib.TempDirTestCase):
  """Test WriteConfigFile() and ReadConfigFile()."""

  def testWriteReadConfigFile(self):
    """Tests WriteConfigFile() then ReadConfigFile()."""
    path = os.path.join(self.tempdir, 'foo.json')
    config = {'foo': 1, 'bar': 2}

    workspace_lib.WriteConfigFile(path, config)
    self.assertDictEqual(config, workspace_lib.ReadConfigFile(path))

  def testWriteConfigFileInvalid(self):
    """Tests writing an invalid configuration file."""
    path = os.path.join(self.tempdir, 'foo.json')
    config = Exception()

    with self.assertRaises(workspace_lib.ConfigFileError):
      workspace_lib.WriteConfigFile(path, config)

  def testReadConfigFileInvalid(self):
    """Tests reading an invalid configuration file."""
    path = os.path.join(self.tempdir, 'foo.json')
    osutils.WriteFile(path, 'invalid contents')

    with self.assertRaises(workspace_lib.ConfigFileError):
      workspace_lib.ReadConfigFile(path)
