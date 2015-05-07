# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module tests the cros image command."""

from __future__ import print_function

import multiprocessing
import os

from chromite.cbuildbot import constants
from chromite.cli import command
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


class ImageCommandTest(cros_test_lib.WorkspaceTestCase):
  """Test class for our ImageCommand class."""

  def SetupCommandMock(self, cmd_args):
    """Sets up the `cros image` command mock."""
    self.cmd_mock = MockImageCommand(cmd_args)

  def setUp(self):
    self.cmd_mock = None
    self.CreateWorkspace()

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
    self.CreateBlueprint(name='foo.json', bricks=['//brick1', '//brick2'],
                         bsp='//bsp1')

    args = ['--blueprint=//foo.json']
    self.SetupCommandMock(args)
    self.cmd_mock.inst.Run()

    expected_args = [os.path.join(constants.CROSUTILS_DIR, 'build_image'),
                     '--extra_packages=brick/foo brick/bar bsp/baz',
                     '--board=foo.json', '--noenable_bootcache',
                     '--enable_rootfs_verification', '--loglevel=7']
    self.rc_mock.assertCommandContains(expected_args)

  def testOutputRootInWorkspace(self):
    """Tests running the image command with an output root in the workspace."""
    self.CreateWorkspace()
    self.CreateBrick(name='brick1', main_package='brick/foo')
    self.CreateBrick(name='bsp1', main_package='bsp/baz')
    self.CreateBlueprint(name='bar.json', bricks=['//brick1'], bsp='//bsp1')

    args = ['--blueprint=//bar.json', '--output_root=//images']
    self.SetupCommandMock(args)
    self.cmd_mock.inst.Run()

    expected_args = [os.path.join(constants.CROSUTILS_DIR, 'build_image'),
                     '--extra_packages=brick/foo bsp/baz',
                     '--board=bar.json', '--noenable_bootcache',
                     '--enable_rootfs_verification',
                     '--output_root=%s' % os.path.join(self.workspace_path,
                                                       'images'),
                     '--loglevel=7']
    self.rc_mock.assertCommandContains(expected_args)

  def testProgressBar(self):
    """Test that RunCommand is called with --progress_bar."""
    self.PatchObject(command, 'UseProgressBar', return_value=True)
    op = self.PatchObject(cros_image.BrilloImageOperation, 'Run')
    self.SetupCommandMock([])
    self.cmd_mock.inst.Run()

    # Test that BrilloImageOperation.Run is called with the correct arguments.
    self.assertEqual(1, op.call_count)
    self.assertTrue('--progress_bar' in op.call_args[0][1])


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
    self.assertEqual(instance.options.output_root, '//build/images')
    self.assertEqual(instance.options.disk_layout, None)
    self.assertEqual(instance.options.enable_serial, None)
    self.assertEqual(instance.options.kernel_log_level, 7)
    self.assertEqual(instance.options.image_types, ['test'])

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
                                      '--output_root=//build/images',
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
    self.assertEqual(instance.options.output_root, '//build/images')
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


class BrilloImageOperationFake(cros_image.BrilloImageOperation):
  """Fake of BrilloImageOperation,"""

  def __init__(self, queue):
    super(BrilloImageOperationFake, self).__init__()
    self._queue = queue

  def ParseOutput(self, output=None):
    super(BrilloImageOperationFake, self).ParseOutput(output)
    self._queue.put('advance')


# TODO(ralphnathan): Inherit from cros_test_lib.ProgressBarTestCase.
# Implemented in CL:267026
class BrilloImageOperationTest(cros_test_lib.ProgressBarTestCase,
                               cros_test_lib.LoggingTestCase):
  """Test class for cros_image.BrilloImageOperation."""

  def BrilloImageFake(self, events, queue):
    """Test function to emulate brillo image."""
    for event in events:
      queue.get()
      print(event)

  def testParseOutputBaseImageStage(self):
    """Test Base Image Creation Stage."""
    events = ['operation: creating base image',
              'Total: 1 packages',
              'Fetched ',
              'Completed ',
              'operation: done creating base image',
              'operation: creating developer image',
              'operation: done creating developer image',
              'operation: creating test image',
              'operation: done creating test image']

    queue = multiprocessing.Queue()
    op = BrilloImageOperationFake(queue)
    with cros_test_lib.LoggingCapturer() as logs:
      with self.OutputCapturer():
        op.Run(self.BrilloImageFake, events, queue)

    # Check that output display progress bars for 1 package being built.
    self.AssertProgressBarAllEvents(2)

    # Check the logs to make sure only the base image creation is logged.
    self.AssertLogsContain(logs, 'Creating disk layout')
    self.AssertLogsContain(logs, 'Building base image')
    self.AssertLogsContain(logs, 'Building developer image', inverted=True)
    self.AssertLogsContain(logs, 'Building test image', inverted=True)

  def testParseOutputTestImageStage(self):
    """Test Test Image Creation Stage."""
    events = ['operation: creating base image',
              'operation: done creating base image',
              'operation: creating developer image',
              'operation: done creating developer image',
              'operation: creating test image',
              'Total: 2 packages',
              'Fetched ',
              'Fetched ',
              'Completed ',
              'operation: done creating test image']

    queue = multiprocessing.Queue()
    op = BrilloImageOperationFake(queue)
    with cros_test_lib.LoggingCapturer() as logs:
      with self.OutputCapturer():
        op.Run(self.BrilloImageFake, events, queue)

    # Check that output display progress bars for 2 packages.
    self.AssertProgressBarAllEvents(4)

    # Check the logs to make sure only the base image creation is logged.
    self.AssertLogsContain(logs, 'Creating disk layout')
    self.AssertLogsContain(logs, 'Building base image', inverted=True)
    self.AssertLogsContain(logs, 'Building developer image', inverted=True)
    self.AssertLogsContain(logs, 'Building test image')

  def testParseOutputDeveloperImageStage(self):
    """Test Developer Image Creation Stage."""
    events = ['operation: creating base image',
              'operation: done creating base image',
              'operation: creating developer image',
              'Total: 2 packages',
              'Fetched ',
              'Fetched ',
              'Completed ',
              'Completed ',
              'operation: done creating developer image',
              'operation: creating test image',
              'operation: done creating test image']

    queue = multiprocessing.Queue()
    op = BrilloImageOperationFake(queue)
    with cros_test_lib.LoggingCapturer() as logs:
      with self.OutputCapturer():
        op.Run(self.BrilloImageFake, events, queue)

    # Check that output display progress bars for 2 packages.
    self.AssertProgressBarAllEvents(4)

    # Check the logs to make sure only the base image creation is logged.
    self.AssertLogsContain(logs, 'Creating disk layout')
    self.AssertLogsContain(logs, 'Building base image', inverted=True)
    self.AssertLogsContain(logs, 'Building developer image')
    self.AssertLogsContain(logs, 'Building test image', inverted=True)
