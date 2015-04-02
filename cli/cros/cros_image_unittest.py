# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module tests the cros image command."""

from __future__ import print_function

import os

from chromite.cbuildbot import constants
from chromite.cli import command_unittest
from chromite.cli.cros import cros_image
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_build_lib_unittest
from chromite.lib import cros_test_lib


class MockImageCommand(command_unittest.MockCommand):
  """Mock out the image command."""
  TARGET = 'chromite.cli.cros.cros_image.ImageCommand'
  TARGET_CLASS = cros_image.ImageCommand
  COMMAND = 'image'

  def __init__(self, *args, **kwargs):
    super(MockImageCommand, self).__init__(*args, **kwargs)

  def Run(self, inst):
    return super(MockImageCommand, self).Run(inst)


class ImageCommandTest(cros_test_lib.MockTempDirTestCase,
                       cros_test_lib.BrickTestCase,
                       cros_test_lib.BlueprintTestCase,
                       cros_test_lib.WorkspaceTestCase):
  """Test class for our ImageCommand class."""

  def SetupCommandMock(self, cmd_args):
    """Sets up the `cros image` command mock."""
    self.cmd_mock = MockImageCommand(cmd_args)

  def setUp(self):
    self.cmd_mock = None
    self.SetupFakeWorkspace()

    # Since the build_image command is mocked out and the workspace directory
    # is patched, fake being in the chroot to allow running the unittest
    # outside the chroot.
    self.PatchObject(cros_build_lib, 'IsInsideChroot', return_value=True)

    self.rc_mock = self.StartPatcher(cros_build_lib_unittest.RunCommandMock())
    self.rc_mock.SetDefaultCmdResult()

  def testBlueprint(self):
    """Tests running the full image command with a blueprint."""
    self.CreateBrick(name='brick1', main_package='brick/foo')
    self.CreateBrick(name='brick2', main_package='brick/bar')
    self.CreateBrick(name='bsp1', main_package='bsp/baz')
    self.CreateBlueprint(blueprint_name='foo.json',
                         bricks=['//brick1', '//brick2'], bsp='//bsp1',
                         main_package='virtual/target-os-test')

    args = ['--blueprint=//foo.json']
    self.SetupCommandMock(args)
    self.cmd_mock.inst.Run()

    expected_args = [os.path.join(constants.CROSUTILS_DIR, 'build_image'),
                     '--extra_packages=brick/foo brick/bar bsp/baz',
                     '--board=brick1', '--noenable_bootcache',
                     '--enable_rootfs_verification', '--loglevel=7']
    self.rc_mock.assertCommandContains(expected_args)


class ImageCommandParserTest(cros_test_lib.TestCase):
  """Test class for our ImageCommand's parser."""

  def testParserDefaults(self):
    """Tests that the parser's default values are correct."""
    fake_parser = commandline.ArgumentParser()
    fake_subparser = fake_parser.add_subparsers()
    image_parser = fake_subparser.add_parser('image')
    cros_image.ImageCommand.AddParser(image_parser)

    options = fake_parser.parse_args(['image'])
    instance = options.command_class(options)
    self.assertEqual(instance.options.adjust_part, None)
    self.assertEqual(instance.options.board, None)
    self.assertEqual(instance.options.boot_args, None)
    self.assertFalse(instance.options.enable_bootcache, False)
    self.assertTrue(instance.options.enable_rootfs_verification, True)
    self.assertEqual(instance.options.output_root, None)
    self.assertEqual(instance.options.disk_layout, None)
    self.assertEqual(instance.options.enable_serial, None)
    self.assertEqual(instance.options.kernel_log_level, 7)
    self.assertEqual(instance.options.image_types, [])

  def testParserSetValues(self):
    """Tests that the parser reads in values correctly."""
    fake_parser = commandline.ArgumentParser()
    fake_subparser = fake_parser.add_subparsers()
    image_parser = fake_subparser.add_parser('image')
    cros_image.ImageCommand.AddParser(image_parser)

    options = fake_parser.parse_args(['image',
                                      '--adjust_part=PARTX',
                                      '--board=fooboard',
                                      '--boot_args=bar',
                                      '--enable_bootcache=True',
                                      '--enable_rootfs_verification=False',
                                      '--output_root=/foo/bar/baz',
                                      '--disk_layout=fooboard/layout.conf',
                                      '--enable_serial=ttyS0',
                                      '--kernel_log_level=2',
                                      'dev', 'test'])
    instance = options.command_class(options)
    self.assertEqual(instance.options.adjust_part, 'PARTX')
    self.assertEqual(instance.options.board, 'fooboard')
    self.assertEqual(instance.options.boot_args, 'bar')
    self.assertTrue(instance.options.enable_bootcache)
    self.assertFalse(instance.options.enable_rootfs_verification)
    self.assertEqual(instance.options.output_root, '/foo/bar/baz')
    self.assertEqual(instance.options.disk_layout, 'fooboard/layout.conf')
    self.assertEqual(instance.options.enable_serial, 'ttyS0')
    self.assertEqual(instance.options.kernel_log_level, 2)
    self.assertEqual(instance.options.image_types, ['dev', 'test'])

  def testParserSetValuesDefaults(self):
    """Tests that the parser reads in values correctly when set to defaults."""
    fake_parser = commandline.ArgumentParser()
    fake_subparser = fake_parser.add_subparsers()
    image_parser = fake_subparser.add_parser('image')
    cros_image.ImageCommand.AddParser(image_parser)

    options = fake_parser.parse_args(['image',
                                      '--adjust_part=PARTX',
                                      '--board=fooboard',
                                      '--boot_args=bar',
                                      '--enable_bootcache=False',
                                      '--enable_rootfs_verification=True',
                                      '--output_root=/foo/bar/baz',
                                      '--disk_layout=fooboard/layout.conf',
                                      '--enable_serial=ttyS0',
                                      '--kernel_log_level=7',
                                      'dev', 'test'])
    instance = options.command_class(options)
    self.assertEqual(instance.options.adjust_part, 'PARTX')
    self.assertEqual(instance.options.board, 'fooboard')
    self.assertEqual(instance.options.boot_args, 'bar')
    self.assertFalse(instance.options.enable_bootcache)
    self.assertTrue(instance.options.enable_rootfs_verification)
    self.assertEqual(instance.options.output_root, '/foo/bar/baz')
    self.assertEqual(instance.options.disk_layout, 'fooboard/layout.conf')
    self.assertEqual(instance.options.enable_serial, 'ttyS0')
    self.assertEqual(instance.options.kernel_log_level, 7)
    self.assertEqual(instance.options.image_types, ['dev', 'test'])

  def testParserSetBooleansTrue(self):
    """Tests that the parser reads in booleans correctly when set to True."""
    fake_parser = commandline.ArgumentParser()
    fake_subparser = fake_parser.add_subparsers()
    image_parser = fake_subparser.add_parser('image')
    cros_image.ImageCommand.AddParser(image_parser)

    options = fake_parser.parse_args(['image',
                                      '--board=fooboard',
                                      '--enable_bootcache=True',
                                      '--enable_rootfs_verification=True',
                                      'dev', 'test'])
    instance = options.command_class(options)
    self.assertEqual(instance.options.board, 'fooboard')
    self.assertTrue(instance.options.enable_bootcache)
    self.assertTrue(instance.options.enable_rootfs_verification)
    self.assertEqual(instance.options.image_types, ['dev', 'test'])

  def testParserSetBooleansFalse(self):
    """Tests that the parser reads in booleans correctly when set to False."""
    fake_parser = commandline.ArgumentParser()
    fake_subparser = fake_parser.add_subparsers()
    image_parser = fake_subparser.add_parser('image')
    cros_image.ImageCommand.AddParser(image_parser)

    options = fake_parser.parse_args(['image',
                                      '--board=fooboard',
                                      '--enable_bootcache=False',
                                      '--enable_rootfs_verification=False',
                                      'dev', 'test'])
    instance = options.command_class(options)
    self.assertEqual(instance.options.board, 'fooboard')
    self.assertFalse(instance.options.enable_bootcache)
    self.assertFalse(instance.options.enable_rootfs_verification)
    self.assertEqual(instance.options.image_types, ['dev', 'test'])

  def testParserBooleanDefaults(self):
    """Tests that the parser sets booleans to defaults when they are omitted."""
    fake_parser = commandline.ArgumentParser()
    fake_subparser = fake_parser.add_subparsers()
    image_parser = fake_subparser.add_parser('image')
    cros_image.ImageCommand.AddParser(image_parser)

    options = fake_parser.parse_args(['image',
                                      '--board=fooboard',
                                      'dev', 'test'])
    instance = options.command_class(options)
    self.assertEqual(instance.options.board, 'fooboard')
    self.assertFalse(instance.options.enable_bootcache)
    self.assertTrue(instance.options.enable_rootfs_verification)
    self.assertEqual(instance.options.image_types, ['dev', 'test'])
