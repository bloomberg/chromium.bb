# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for the blueprint library."""

from __future__ import print_function

from chromite.lib import brick_lib
from chromite.lib import cros_test_lib


class BlueprintLibTest(cros_test_lib.MockTempDirTestCase,
                       cros_test_lib.BlueprintTestCase,
                       cros_test_lib.WorkspaceTestCase):
  """Unittest for blueprint_lib.py"""

  def setUp(self):
    self.SetupFakeWorkspace()

  def testBlueprint(self):
    """Tests getting the basic blueprint getters."""
    bricks = ['//foo', '//bar', '//baz']
    blueprint = self.CreateBlueprint(bricks=bricks, bsp='//bsp',
                                     main_package='virtual/target-os')
    self.assertEqual(blueprint.GetBricks(), bricks)
    self.assertEqual(blueprint.GetBSP(), '//bsp')
    self.assertEqual(blueprint.GetMainPackage(), 'virtual/target-os')

  def testBlueprintNoBricks(self):
    """Tests that blueprints without bricks return reasonable defaults."""
    blueprint = self.CreateBlueprint(bsp='//bsp2',
                                     main_package='virtual/target-os-dev')
    self.assertEqual(blueprint.GetBricks(), [])
    self.assertEqual(blueprint.GetBSP(), '//bsp2')
    self.assertEqual(blueprint.GetMainPackage(), 'virtual/target-os-dev')

  def testGetUsedBricks(self):
    """Tests that we can list all the bricks used."""
    brick_lib.Brick('//a', initial_config={'name':'a'})
    brick_b = brick_lib.Brick('//b', initial_config={'name':'b'})
    brick_c = brick_lib.Brick('//c',
                              initial_config={'name':'c',
                                              'dependencies': ['//b']})

    blueprint = self.CreateBlueprint(bsp='//a', bricks=[brick_c.brick_locator])
    self.assertEqual(3, len(blueprint.GetUsedBricks()))

    # We sort out duplicates: c depends on b and b is explicitly listed in
    # bricks too.
    blueprint = self.CreateBlueprint(bsp='//a', bricks=[brick_c.brick_locator,
                                                        brick_b.brick_locator])
    self.assertEqual(3, len(blueprint.GetUsedBricks()))
