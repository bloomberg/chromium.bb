# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module tests the cros image command."""

from __future__ import print_function

from chromite.cli.cros import cros_image
from chromite.lib import commandline
from chromite.lib import cros_test_lib


class ImageCommandTest(cros_test_lib.TestCase):
  """Test class for our ImageCommand class."""

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
