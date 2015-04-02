# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for the brick library."""

from __future__ import print_function

import mock
import os

from chromite.cbuildbot import constants
from chromite.lib import brick_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import workspace_lib


class BrickLibTest(cros_test_lib.MockTempDirTestCase,
                   cros_test_lib.BrickTestCase,
                   cros_test_lib.WorkspaceTestCase):
  """Unittest for brick.py"""

  # pylint: disable=protected-access

  def setUp(self):
    self.brick = None
    self.brick_path = None
    self.SetupFakeWorkspace()

  def SetupLegacyBrick(self, brick_dir=None, brick_name='foo'):
    """Sets up a legacy brick layout."""
    if brick_dir is None:
      brick_dir = self.tempdir
    layout_conf = 'repo-name = %s\n' % brick_name
    osutils.WriteFile(os.path.join(brick_dir, 'metadata', 'layout.conf'),
                      layout_conf, makedirs=True)

  def testLayoutFormat(self):
    """Test that layout.conf is correctly formatted."""
    (self.brick, self.brick_path) = self.CreateBrick()
    content = {'repo-name': 'hello',
               'bar': 'foo'}
    self.brick._WriteLayoutConf(content)

    path = os.path.join(self.brick.OverlayDir(), 'metadata', 'layout.conf')
    layout_conf = osutils.ReadFile(path).split('\n')

    expected_lines = ['repo-name = hello',
                      'bar = foo',
                      'profile-formats = portage-2']
    for line in expected_lines:
      self.assertTrue(line in layout_conf)

  def testWriteParents(self):
    """Test that the parent file is correctly formatted."""
    (self.brick, self.brick_path) = self.CreateBrick()
    parents = ['hello:bonjour',
               'foo:bar']

    self.brick._WriteParents(parents)
    path = os.path.join(self.brick.OverlayDir(), 'profiles', 'base', 'parent')
    parents_content = osutils.ReadFile(path)

    self.assertEqual('hello:bonjour\nfoo:bar\n', parents_content)

  def testConfigurationGenerated(self):
    """Test that portage's files are generated when brick.json changes."""
    (self.brick, self.brick_path) = self.CreateBrick()
    sample_config = {'name': 'hello',
                     'dependencies': []}

    self.brick.UpdateConfig(sample_config)

    self.assertExists(os.path.join(self.brick.OverlayDir(), 'profiles', 'base',
                                   'parent'))
    self.assertExists(self.brick._LayoutConfPath())

  def testFindBrickInPath(self):
    """Test that we can infer the current brick from the current directory."""
    (self.brick, self.brick_path) = self.CreateBrick()
    os.remove(os.path.join(self.brick_path, 'config.json'))
    brick_dir = os.path.join(self.tempdir, 'foo', 'bar', 'project')
    expected_name = 'hello'
    brick_lib.Brick(brick_dir, initial_config={'name': 'hello'})

    with osutils.ChdirContext(self.tempdir):
      self.assertEqual(None, brick_lib.FindBrickInPath())

    with osutils.ChdirContext(brick_dir):
      self.assertEqual(expected_name,
                       brick_lib.FindBrickInPath().config['name'])

    subdir = os.path.join(brick_dir, 'sub', 'directory')
    osutils.SafeMakedirs(subdir)
    with osutils.ChdirContext(subdir):
      self.assertEqual(expected_name,
                       brick_lib.FindBrickInPath().config['name'])

  def testBrickByNameExact(self):
    """Test that we can get the brick for a given name."""
    first = os.path.join(self.tempdir, 'foo')
    osutils.WriteFile(os.path.join(first, 'make.conf'), 'hello', makedirs=True)

    second = os.path.join(self.tempdir, 'bar')
    brick_lib.Brick(second, initial_config={'name': 'bar'})

    hello = os.path.join(self.tempdir, 'hello')
    hello_brick = brick_lib.Brick(hello, initial_config={'name': 'hello'})

    hello_private = os.path.join(self.tempdir, 'hello-private')
    brick_lib.Brick(hello_private, initial_config={'name': 'hello-private'})

    missing_overlay = os.path.join(self.tempdir, 'does', 'not', 'exist')
    with mock.patch('chromite.lib.portage_util.FindOverlays',
                    return_value=[missing_overlay, first, second,
                                  hello_brick.OverlayDir(), hello_private]):
      self.assertEquals(hello, brick_lib.FindBrickByName('hello').brick_dir)

  def testBrickByNamePrivate(self):
    """Test that we can get the brick even if the overlay is private."""
    first = os.path.join(self.tempdir, 'foo')
    osutils.WriteFile(os.path.join(first, 'make.conf'), 'hello', makedirs=True)

    second = os.path.join(self.tempdir, 'bar')
    brick_lib.Brick(second, initial_config={'name': 'bar'})

    # Use a name with 'p' and 'r' at the end. It is tempting to use
    # rstrip('-private') to strip the -private but it will result in the wrong
    # result (hello instead of hellopr in this case.)
    hello_private = os.path.join(self.tempdir, 'hellopr-private')
    brick_lib.Brick(hello_private, initial_config={'name': 'hellopr-private'})

    with mock.patch('chromite.lib.portage_util.FindOverlays',
                    return_value=[first, second, hello_private]):
      self.assertEquals(hello_private,
                        brick_lib.FindBrickByName('hellopr').brick_dir)

  def testBrickCreation(self):
    """Test that brick initialization throws the right errors."""
    (self.brick, self.brick_path) = self.CreateBrick()
    with self.assertRaises(brick_lib.BrickCreationFailed):
      brick_lib.Brick(self.brick_path, initial_config={})

    nonexistingbrick = os.path.join(self.tempdir, 'foo')
    with self.assertRaises(brick_lib.BrickNotFound):
      brick_lib.Brick(nonexistingbrick)

  def testLoadNonExistingBrickFails(self):
    """Tests that trying to load a non-existing brick fails."""
    self.assertRaises(brick_lib.BrickNotFound, brick_lib.Brick, self.tempdir)

  def testLoadExistingNormalBrickSucceeds(self):
    """Tests that loading an existing brick works."""
    (self.brick, self.brick_path) = self.CreateBrick()
    self.brick = brick_lib.Brick(self.brick_path, allow_legacy=False)
    self.assertEquals('thebrickfoo', self.brick.config.get('name'))

  def testLoadExistingLegacyBrickFailsIfNotAllowed(self):
    """Tests that loading a legacy brick fails when not allowed."""
    self.SetupLegacyBrick()
    with self.assertRaises(brick_lib.BrickNotFound):
      brick_lib.Brick(self.tempdir, allow_legacy=False)

  def testLoadExistingLegacyBrickSucceeds(self):
    """Tests that loading a legacy brick fails when not allowed."""
    self.SetupLegacyBrick()
    self.brick = brick_lib.Brick(self.tempdir)
    self.assertEquals('foo', self.brick.config.get('name'))

  def testLegacyBrickUpdateConfigFails(self):
    """Tests that a legacy brick config cannot be updated."""
    self.SetupLegacyBrick()
    self.brick = brick_lib.Brick(self.tempdir)
    with self.assertRaises(brick_lib.BrickFeatureNotSupported):
      self.brick.UpdateConfig({'name': 'bar'})

  def testInherits(self):
    """Tests the containment checking works as intended."""
    saved_root = constants.SOURCE_ROOT

    try:
      # Mock the source root so that we can create fake legacy overlay.
      constants.SOURCE_ROOT = self.tempdir
      legacy = os.path.join(self.tempdir, 'src', 'overlays', 'overlay-foobar')
      self.SetupLegacyBrick(brick_dir=legacy, brick_name='foobar')

      bar_brick = brick_lib.Brick('//bar', initial_config={'name': 'bar'})
      foo_brick = brick_lib.Brick(
          '//foo', initial_config={'name': 'foo',
                                   'dependencies': ['//bar', 'board:foobar']})

      self.assertTrue(bar_brick.Inherits('bar'))
      self.assertTrue(foo_brick.Inherits('bar'))
      self.assertFalse(bar_brick.Inherits('foo'))
      self.assertTrue(foo_brick.Inherits('foobar'))
      self.assertFalse(foo_brick.Inherits('dontexist'))

    finally:
      constants.SOURCE_ROOT = saved_root

  def testOverlayDir(self):
    """Tests that overlay directory is returned correctly."""
    (self.brick, self.brick_path) = self.CreateBrick()
    self.assertExists(os.path.join(self.brick.OverlayDir(), 'profiles'))

  def testOpenUsingLocator(self):
    """Tests that we can open a brick given a locator."""
    brick_lib.Brick(os.path.join(self.tempdir, 'foo'),
                    initial_config={'name': 'foo'})

    brick_lib.Brick('//foo')

    with self.assertRaises(brick_lib.BrickNotFound):
      brick_lib.Brick('//doesnotexist')

  def testCreateUsingLocator(self):
    """Tests that we can create a brick using a locator."""
    brick_lib.Brick('//foobar', initial_config={'name': 'foobar'})
    brick_lib.Brick('//bricks/some/path', initial_config={'name': 'path'})

    brick_lib.Brick('//foobar')
    brick_lib.Brick('//bricks/some/path')

    brick_lib.Brick(os.path.join(self.tempdir, 'foobar'))
    brick_lib.Brick(os.path.join(self.tempdir, 'bricks', 'some', 'path'))

  def testFriendlyName(self):
    """Tests that the friendly name generation works."""
    first = brick_lib.Brick('//foo/bar/test', initial_config={'name': 'test'})
    self.assertEqual('foo.bar.test', first.FriendlyName())

    second = brick_lib.Brick(os.path.join(self.tempdir, 'test', 'foo'),
                             initial_config={'name': 'foo'})
    self.assertEqual('test.foo', second.FriendlyName())

  def testMissingDependency(self):
    """Tests that the brick creation fails when a dependency is missing."""
    with self.assertRaises(brick_lib.BrickCreationFailed):
      brick_lib.Brick('//bar',
                      initial_config={'name':'bar',
                                      'dependencies':['//dont/exist']})

    # If the creation failed, the directory is removed cleanly.
    self.assertFalse(os.path.exists(workspace_lib.LocatorToPath('//bar')))

  def testNormalizedDependencies(self):
    """Tests that dependencies are normalized during brick creation."""
    brick_lib.Brick('//foo/bar', initial_config={'name': 'bar'})
    with osutils.ChdirContext(os.path.join(self.tempdir, 'foo')):
      brick_lib.Brick('//baz', initial_config={'name': 'baz',
                                               'dependencies': ['bar']})

    deps = brick_lib.Brick('//baz').config['dependencies']
    self.assertEqual(1, len(deps))
    self.assertEqual('//foo/bar', deps[0])

  def testBrickStack(self):
    """Tests that the brick stacking is correct."""
    def brick_dep(name, deps):
      config = {'name': os.path.basename(name),
                'dependencies': deps}
      return brick_lib.Brick(name, initial_config=config)

    brick_dep('//first', [])
    brick_dep('//second', ['//first'])
    third = brick_dep('//third', ['//first', '//second'])
    fourth = brick_dep('//fourth', ['//second', '//first'])

    self.assertEqual(['//first', '//second', '//third'],
                     [b.brick_locator for b in third.BrickStack()])

    self.assertEqual(['//first', '//second', '//fourth'],
                     [b.brick_locator for b in fourth.BrickStack()])

  def testProfilesOverride(self):
    """Tests that the 'experimental_profiles' field is respected."""
    profiles = ['chromiumos:base', '../foo']
    config = {'name': 'foo', 'experimental_profiles': profiles}
    b = brick_lib.Brick('//foo', initial_config=config)

    parents = osutils.ReadFile(os.path.join(b.OverlayDir(), 'profiles', 'base',
                                            'parent'))
    self.assertEquals(profiles, parents.splitlines())
