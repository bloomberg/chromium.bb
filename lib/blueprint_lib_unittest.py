# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for the blueprint library."""

from __future__ import print_function

from chromite.lib import blueprint_lib
from chromite.lib import brick_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import workspace_lib


class ExpandBlueprintPathTest(cros_test_lib.TestCase):
  """Tests ExpandBlueprintPath()."""

  def testBasicName(self):
    """Tests a basic name."""
    self.assertEqual('//blueprints/foo.json',
                     blueprint_lib.ExpandBlueprintPath('foo'))

  def testLocator(self):
    """Tests a locator name."""
    self.assertEqual('//foo/bar.json',
                     blueprint_lib.ExpandBlueprintPath('//foo/bar'))

  def testCurrentDirectory(self):
    """Tests a path to the current directory."""
    self.assertEqual('./foo.json',
                     blueprint_lib.ExpandBlueprintPath('./foo'))

  def testJson(self):
    """Tests that .json isn't added twice."""
    self.assertEqual('//blueprints/foo.json',
                     blueprint_lib.ExpandBlueprintPath('foo.json'))

  def testTxt(self):
    """Tests that .txt extension still gets .json appended."""
    self.assertEqual('//blueprints/foo.txt.json',
                     blueprint_lib.ExpandBlueprintPath('foo.txt'))


class BlueprintLibTest(cros_test_lib.WorkspaceTestCase):
  """Unittest for blueprint_lib.py"""

  def setUp(self):
    self.CreateWorkspace()

  def testBlueprint(self):
    """Tests getting the basic blueprint getters."""
    bricks = ['//foo', '//bar', '//baz']
    for brick in bricks:
      self.CreateBrick(brick)
    self.CreateBrick('//bsp')
    blueprint = self.CreateBlueprint(bricks=bricks, bsp='//bsp')
    self.assertEqual(blueprint.GetBricks(), bricks)
    self.assertEqual(blueprint.GetBSP(), '//bsp')

  def testBlueprintNoBricks(self):
    """Tests that blueprints without bricks return reasonable defaults."""
    self.CreateBrick('//bsp2')
    blueprint = self.CreateBlueprint(bsp='//bsp2')
    self.assertEqual(blueprint.GetBricks(), [])
    self.assertEqual(blueprint.GetBSP(), '//bsp2')

  def testGetUsedBricks(self):
    """Tests that we can list all the bricks used."""
    brick_lib.Brick('//a', initial_config={'name':'a'})
    brick_b = brick_lib.Brick('//b', initial_config={'name':'b'})
    brick_c = brick_lib.Brick('//c',
                              initial_config={'name':'c',
                                              'dependencies': ['//b']})

    blueprint = self.CreateBlueprint(blueprint_name='foo.json',
                                     bsp='//a', bricks=[brick_c.brick_locator])
    self.assertEqual(3, len(blueprint.GetUsedBricks()))

    # We sort out duplicates: c depends on b and b is explicitly listed in
    # bricks too.
    blueprint = self.CreateBlueprint(blueprint_name='bar.json',
                                     bsp='//a', bricks=[brick_c.brick_locator,
                                                        brick_b.brick_locator])
    self.assertEqual(3, len(blueprint.GetUsedBricks()))

  def testBlueprintAlreadyExists(self):
    """Tests creating a blueprint where one already exists."""
    self.CreateBrick('//foo')
    self.CreateBrick('//bar')
    self.CreateBlueprint(blueprint_name='//my_blueprint', bricks=['//foo'])
    with self.assertRaises(blueprint_lib.BlueprintCreationError):
      self.CreateBlueprint(blueprint_name='//my_blueprint', bricks=['//bar'])
    # Make sure the original blueprint is untouched.
    self.assertEqual(['//foo'],
                     blueprint_lib.Blueprint('//my_blueprint').GetBricks())

  def testBlueprintBrickNotFound(self):
    """Tests creating a blueprint with a non-existent brick fails."""
    with self.assertRaises(blueprint_lib.BlueprintCreationError):
      self.CreateBlueprint(blueprint_name='//my_blueprint', bricks=['//none'])

  def testBlueprintBSPNotFound(self):
    """Tests creating a blueprint with a non-existent BSP fails."""
    with self.assertRaises(blueprint_lib.BlueprintCreationError):
      self.CreateBlueprint(blueprint_name='//my_blueprint', bsp='//none')

  def testBlueprintNotFound(self):
    """Tests loading a non-existent blueprint file."""
    with self.assertRaises(blueprint_lib.BlueprintNotFoundError):
      blueprint_lib.Blueprint('//not/a/blueprint')

  def testInvalidBlueprint(self):
    """Tests loading an invalid blueprint file."""
    path = workspace_lib.LocatorToPath('//invalid_file')
    osutils.WriteFile(path, 'invalid contents')
    with self.assertRaises(workspace_lib.ConfigFileError):
      blueprint_lib.Blueprint(path)
