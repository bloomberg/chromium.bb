# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module tests the brillo blueprint command."""

from __future__ import print_function

from chromite.cli import command_unittest
from chromite.cli.brillo import brillo_blueprint
from chromite.lib import cros_test_lib


class MockBlueprintCommand(command_unittest.MockCommand):
  """Mock out the `brillo blueprint` command."""
  TARGET = 'chromite.cli.brillo.brillo_blueprint.BlueprintCommand'
  TARGET_CLASS = brillo_blueprint.BlueprintCommand
  COMMAND = 'blueprint'


class BlueprintCommandTest(cros_test_lib.OutputTestCase,
                           cros_test_lib.WorkspaceTestCase):
  """Test the flow of BlueprintCommand.Run()."""

  FOO_BRICK = '//bricks/foo'
  BAR_BRICK = '//bricks/bar'
  BSP_BRICK = '//bsps/my_bsp'

  def setUp(self):
    """Patches objects."""
    self.cmd_mock = None
    self.CreateWorkspace()
    # Create some fake bricks for testing.
    for brick in (self.FOO_BRICK, self.BAR_BRICK, self.BSP_BRICK):
      self.CreateBrick(brick)

  def SetupCommandMock(self, cmd_args):
    """Sets up the MockBlueprintCommand."""
    self.cmd_mock = MockBlueprintCommand(cmd_args)
    self.StartPatcher(self.cmd_mock)

  def RunCommandMock(self, *args, **kwargs):
    """Sets up and runs the MockBlueprintCommand."""
    self.SetupCommandMock(*args, **kwargs)
    self.cmd_mock.inst.Run()

  def testCreateBlueprint(self):
    """Tests basic blueprint creation."""
    with self.OutputCapturer():
      self.RunCommandMock(['create', '//blueprints/foo.json'])
    self.AssertBlueprintExists('//blueprints/foo.json')
    # Also make sure we get a warning for the missing BSP.
    self.AssertOutputContainsWarning('No BSP was specified', check_stderr=True)

  def testCreateBlueprintBsp(self):
    """Tests --bsp argument."""
    self.RunCommandMock(['create', 'foo', '--bsp', self.BSP_BRICK])
    self.AssertBlueprintExists('//blueprints/foo.json', bsp=self.BSP_BRICK)

  def testCreateBlueprintBricks(self):
    """Tests --brick and -b arguments."""
    self.RunCommandMock(['create', 'foo', '--brick', self.FOO_BRICK,
                         '-b%s' % self.BAR_BRICK])
    self.AssertBlueprintExists('//blueprints/foo.json',
                               bricks=[self.FOO_BRICK, self.BAR_BRICK])

  def testCreateBlueprintAll(self):
    """Test --brick and --bsp arguments together."""
    self.RunCommandMock(['create', 'foo', '--brick', self.FOO_BRICK,
                         '--bsp', self.BSP_BRICK])
    self.AssertBlueprintExists('//blueprints/foo.json', bsp=self.BSP_BRICK,
                               bricks=[self.FOO_BRICK])

  def testCreateBlueprintPathNormalization(self):
    """Tests path normalization."""
    self.RunCommandMock(['create', 'foo', '--bsp', 'my_bsp', '--brick', 'foo'])
    self.AssertBlueprintExists('//blueprints/foo.json', bsp='//bsps/my_bsp',
                               bricks=['//bricks/foo'])

  def testInvalidCommand(self):
    """Tests that `brillo blueprint` without `create` prints usage."""
    with self.OutputCapturer():
      with self.assertRaises(SystemExit):
        self.RunCommandMock([])
    self.AssertOutputContainsLine('usage:', check_stderr=True)
