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

  def testEmptyBlueprintFile(self):
    """Tests that empty blueprints create the basic file structure."""
    blueprint = self.CreateBlueprint()
    file_contents = workspace_lib.ReadConfigFile(blueprint.path)

    self.assertIn(blueprint_lib.BRICKS_FIELD, file_contents)
    self.assertIn(blueprint_lib.BSP_FIELD, file_contents)

  def testGetUsedBricks(self):
    """Tests that we can list all the bricks used."""
    brick_lib.Brick('//a', initial_config={'name':'a'})
    brick_b = brick_lib.Brick('//b', initial_config={'name':'b'})
    brick_c = brick_lib.Brick('//c',
                              initial_config={'name':'c',
                                              'dependencies': ['//b']})

    blueprint = self.CreateBlueprint(name='foo.json',
                                     bsp='//a', bricks=[brick_c.brick_locator])
    self.assertEqual(3, len(blueprint.GetUsedBricks()))

    # We sort out duplicates: c depends on b and b is explicitly listed in
    # bricks too.
    blueprint = self.CreateBlueprint(name='bar.json',
                                     bsp='//a', bricks=[brick_c.brick_locator,
                                                        brick_b.brick_locator])
    self.assertEqual(3, len(blueprint.GetUsedBricks()))

  def testGetPackages(self):
    """Tests that we can get the needed packages for a given blueprint."""
    self.CreateBrick('foo', main_package='app-misc/foopkg')
    self.CreateBrick('bar', main_package='app-misc/barpkg')
    self.CreateBrick('foobar', main_package='app-misc/foobarpkg',
                     dependencies=['//foo', '//bar'])

    self.CreateBrick('hello', main_package='app-misc/hello')

    self.CreateBrick('mybsp', main_package='app-misc/bspbonjour')

    blueprint = self.CreateBlueprint(name='//myblueprint',
                                     bricks=['//hello', '//foobar'],
                                     bsp='//mybsp')
    packages = blueprint.GetPackages(with_implicit=False)
    self.assertEqual(
        set(('app-misc/foobarpkg', 'app-misc/hello', 'app-misc/bspbonjour')),
        set(packages))

    packages = blueprint.GetPackages(with_implicit=True)
    self.assertTrue('virtual/target-os' in packages)
    self.assertTrue('virtual/target-os-dev' in packages)
    self.assertTrue('virtual/target-os-test' in packages)

  def testBlueprintAlreadyExists(self):
    """Tests creating a blueprint where one already exists."""
    self.CreateBrick('//foo')
    self.CreateBrick('//bar')
    self.CreateBlueprint(name='//my_blueprint', bricks=['//foo'])
    with self.assertRaises(blueprint_lib.BlueprintCreationError):
      self.CreateBlueprint(name='//my_blueprint', bricks=['//bar'])
    # Make sure the original blueprint is untouched.
    self.assertEqual(['//foo'],
                     blueprint_lib.Blueprint('//my_blueprint').GetBricks())

  def testBlueprintBrickNotFound(self):
    """Tests creating a blueprint with a non-existent brick fails."""
    with self.assertRaises(blueprint_lib.BlueprintCreationError):
      self.CreateBlueprint(name='//my_blueprint', bricks=['//none'])

  def testBlueprintBSPNotFound(self):
    """Tests creating a blueprint with a non-existent BSP fails."""
    with self.assertRaises(blueprint_lib.BlueprintCreationError):
      self.CreateBlueprint(name='//my_blueprint', bsp='//none')

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
