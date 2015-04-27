# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module tests the brillo brick command."""

from __future__ import print_function

from chromite.cli import command_unittest
from chromite.cli.brillo import brillo_brick
from chromite.lib import brick_lib
from chromite.lib import cros_test_lib


class MockBrickCommand(command_unittest.MockCommand):
  """Mock out the `brillo brick` command."""
  TARGET = 'chromite.cli.brillo.brillo_brick.BrickCommand'
  TARGET_CLASS = brillo_brick.BrickCommand
  COMMAND = 'brick'


class BrickCommandTest(cros_test_lib.OutputTestCase,
                       cros_test_lib.MockTempDirTestCase):
  """Test the flow of BrickCommand.Run()."""

  def SetupCommandMock(self, cmd_args):
    """Sets up the MockBrickCommand."""
    self.cmd_mock = MockBrickCommand(cmd_args)
    self.StartPatcher(self.cmd_mock)

  def setUp(self):
    """Patches objects."""
    self.cmd_mock = None
    self.brick_mock = self.PatchObject(brick_lib, 'Brick')

  def _VerifyBrickCreation(self, locator, dependencies=None):
    """Verifies that a brick was created with |dependencies|.

    Args:
      locator: brick locator to check.
      dependencies: brick dependencies to check; None to skip.
    """
    positional_args, kwargs = self.brick_mock.call_args
    self.assertEqual(locator, positional_args[0])
    if dependencies is not None:
      self.assertListEqual(dependencies,
                           kwargs['initial_config']['dependencies'])

  def testCreateBrick(self):
    """Tests brick creation."""
    self.SetupCommandMock(['create', '//bricks/foo'])
    self.cmd_mock.inst.Run()
    self._VerifyBrickCreation('//bricks/foo', dependencies=[])

  def testCreateBrickDependencies(self):
    """Tests brick creation with dependencies."""
    self.SetupCommandMock(['create', '//bricks/foo', '-d//bricks/dep1',
                           '--dependency', '//bricks/dep2'])
    self.cmd_mock.inst.Run()
    self._VerifyBrickCreation('//bricks/foo',
                              dependencies=['//bricks/dep1', '//bricks/dep2'])

  def testCreateBrickPathNormalization(self):
    """Tests path normalization for bricks."""
    self.SetupCommandMock(['create', 'foo', '-ddep1', '-ddep2'])
    self.cmd_mock.inst.Run()
    self._VerifyBrickCreation('//bricks/foo',
                              dependencies=['//bricks/dep1', '//bricks/dep2'])

  def testCreateBrickFailure(self):
    """Tests that on failure the error message is printed."""
    self.SetupCommandMock(['create', '//bricks/foo'])
    error_message = 'foo error message'
    self.brick_mock.side_effect = brick_lib.BrickCreationFailed(error_message)
    with self.OutputCapturer():
      with self.assertRaises(SystemExit):
        self.cmd_mock.inst.Run()
    self.AssertOutputContainsError(error_message, check_stderr=True)

  def testBrickInvalidCommand(self):
    """Tests that `brillo brick` without `create` prints usage."""
    with self.OutputCapturer():
      with self.assertRaises(SystemExit):
        self.SetupCommandMock([])
        self.cmd_mock.inst.Run()
    self.AssertOutputContainsLine('usage:', check_stderr=True)
