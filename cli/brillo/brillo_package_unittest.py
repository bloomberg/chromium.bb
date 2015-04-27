# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for brillo_package."""

from __future__ import print_function

import os

from chromite.cli import command_unittest
from chromite.cli.brillo import brillo_package
from chromite.lib import brick_lib
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils


class MockPackageCommand(command_unittest.MockCommand):
  """Mock out the `brillo package` command."""
  TARGET = 'chromite.cli.brillo.brillo_package.PackageCommand'
  TARGET_CLASS = brillo_package.PackageCommand
  COMMAND = 'package'


class PackageCommandTest(cros_test_lib.OutputTestCase,
                         cros_test_lib.WorkspaceTestCase):
  """Test the flow of PackageCommand.Run()."""

  _PACKAGE_CATEGORY = 'foo'
  _PACKAGE_NAME = 'bar'
  _PACKAGE = '%s/%s' % (_PACKAGE_CATEGORY, _PACKAGE_NAME)

  def SetupCommandMock(self, cmd_args, create_cwd_brick=True):
    """Sets up the MockPackageCommand.

    Args:
      cmd_args: List of args to pass to `cros package`.
      create_cwd_brick: True to create an implicit CWD brick to use.
    """
    if create_cwd_brick:
      self.cwd_brick = self.CreateBrick()
      self.PatchObject(brick_lib, 'FindBrickInPath',
                       return_value=self.cwd_brick)
      # Save some long paths that are required by several tests.
      self.cwd_brick_categories_file = os.path.join(
          self.cwd_brick.OverlayDir(), 'profiles', 'categories')
      self.cwd_brick_ebuild_file = os.path.join(
          self.cwd_brick.OverlayDir(), self._PACKAGE_CATEGORY,
          self._PACKAGE_NAME, '%s-9999.ebuild' % self._PACKAGE_NAME)
    self.cmd_mock = MockPackageCommand(cmd_args)
    self.StartPatcher(self.cmd_mock)

  def RunCommandMock(self, *args, **kwargs):
    """Sets up and runs the MockPackageCommand.

    Convenience function to call SetupCommandMock() and run the
    mock in one call.

    Args:
      args: Args to pass to SetupCommandMock().
      kwargs: Keyword args to pass to SetupCommandMock().
    """
    self.SetupCommandMock(*args, **kwargs)
    self.cmd_mock.inst.Run()

  def setUp(self):
    """Patches objects."""
    self.CreateWorkspace()
    self.PatchObject(commandline, 'RunInsideChroot')
    # These objects are valid after SetupCommandMock() has been called.
    self.cmd_mock = None
    self.cwd_brick = None
    self.cwd_brick_categories_file = None
    self.cwd_brick_ebuild_file = None

  def testCwdBrick(self):
    """Tests no-op using the CWD brick."""
    self.RunCommandMock([self._PACKAGE])
    self.assertEqual(self.cwd_brick.brick_locator,
                     self.cmd_mock.inst.brick.brick_locator)

  def testExplicitBrick(self):
    """Tests the --brick argument takes priority over CWD."""
    brick_locator = '//bricks/mybrick'
    self.CreateBrick(brick_locator)
    self.RunCommandMock(['--brick', brick_locator, self._PACKAGE])
    self.assertEqual(brick_locator, self.cmd_mock.inst.brick.brick_locator)

  def testBrickPathNormalization(self):
    """Tests brick path normalization."""
    brick_locator = '//bricks/mybrick'
    short_path = 'mybrick'
    self.CreateBrick(brick_locator)
    self.RunCommandMock(['--brick', short_path, self._PACKAGE])
    self.assertEqual(brick_locator, self.cmd_mock.inst.brick.brick_locator)

  def testNoBrick(self):
    """Tests that execution fails without a brick."""
    with self.assertRaises(cros_build_lib.DieSystemExit):
      self.RunCommandMock([self._PACKAGE], create_cwd_brick=False)

  def testInvalidBrick(self):
    """Tests that execution fails with a non-existent brick."""
    with self.assertRaises(cros_build_lib.DieSystemExit):
      self.RunCommandMock(['--brick', '//not/a/brick', self._PACKAGE])

  def testEnableLatest(self):
    """Tests --enable latest."""
    self.RunCommandMock(['--enable', 'latest', self._PACKAGE])
    self.cmd_mock.rc_mock.assertCommandContains(
        ['cros_workon', 'start', '--brick=%s' % self.cwd_brick.brick_locator,
         self._PACKAGE])

  def testEnableStable(self):
    """Tests --enable stable."""
    self.RunCommandMock(['--enable', 'stable', self._PACKAGE])
    self.cmd_mock.rc_mock.assertCommandContains(
        ['cros_workon', 'stop', '--brick=%s' % self.cwd_brick.brick_locator,
         self._PACKAGE])

  def testHasLatest(self):
    """Tests @has-latest."""
    self.RunCommandMock(['--enable', 'latest', '@has-latest'])
    self.cmd_mock.rc_mock.assertCommandContains(
        ['cros_workon', 'start', '--brick=%s' % self.cwd_brick.brick_locator,
         '--all'])

  def testOnlyLatest(self):
    """Tests @only-latest."""
    self.RunCommandMock(['--enable', 'latest', '@only-latest'])
    self.cmd_mock.rc_mock.assertCommandContains(
        ['cros_workon', 'start', '--brick=%s' % self.cwd_brick.brick_locator,
         '--workon_only'])

  def testCreateSource(self):
    """Tests --create-source."""
    self.RunCommandMock(['--create-source', '.', self._PACKAGE])
    # Consider it successful if the ebuild file was created and our package
    # category was written to the categories file.
    self.assertExists(self.cwd_brick_ebuild_file)
    self.assertEqual('%s\n' % self._PACKAGE_CATEGORY,
                     osutils.ReadFile(self.cwd_brick_categories_file))

  def testCreateSourceMissingDirectory(self):
    """Tests --create-source with a missing source directory."""
    with self.assertRaises(cros_build_lib.DieSystemExit):
      self.RunCommandMock(['--create-source', 'no/source/dir', self._PACKAGE])

  def testCreateSourceMissingDirectoryForce(self):
    """Tests --create-source --force with a missing source directory."""
    self.RunCommandMock(['--create-source', 'no/source/dir', self._PACKAGE,
                         '--force'])

  def testCreateSourceAddCategory(self):
    """Tests --create-source when the category file already exists."""
    existing_category = 'existing_package_category'
    self.SetupCommandMock(['--create-source', '.', self._PACKAGE])
    osutils.WriteFile(self.cwd_brick_categories_file,
                      '%s\n' % existing_category, makedirs=True)
    self.cmd_mock.inst.Run()
    # Our new package category should be appended to the existing file.
    self.assertEqual('%s\n%s\n' % (existing_category, self._PACKAGE_CATEGORY),
                     osutils.ReadFile(self.cwd_brick_categories_file))

  def testCreateSourceEbuildExists(self):
    """Tests --create-source exits when the ebuild already exists."""
    existing_file_contents = 'existing ebuild file'
    self.SetupCommandMock(['--create-source', '.', self._PACKAGE])
    osutils.WriteFile(self.cwd_brick_ebuild_file, existing_file_contents,
                      makedirs=True)
    with self.assertRaises(cros_build_lib.DieSystemExit):
      self.cmd_mock.inst.Run()
    # The existing file should not have been touched.
    self.assertEqual(existing_file_contents,
                     osutils.ReadFile(self.cwd_brick_ebuild_file))
