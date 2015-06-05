# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test the image_lib module."""

from __future__ import print_function

import gc
import glob
import multiprocessing
import os
import sys

from chromite.cbuildbot import constants
from chromite.lib import cros_build_lib_unittest
from chromite.lib import cros_test_lib
from chromite.lib import image_lib
from chromite.lib import osutils
from chromite.lib import parallel
from chromite.lib import partial_mock


class FakeException(Exception):
  """Fake exception used for testing exception handling."""


class LoopbackPartitions(object):
  """Mocked loopback partition class to use in unit tests.

  Args:
    path: Path to the image file.
    dev: Path for the base loopback device.
    part_count: How many partition device files to make up.
    part_overrides: A dict which is used to update self.parts.
  """
  # pylint: disable=dangerous-default-value
  def __init__(self, path='/dev/loop9999', dev=None,
               part_count=None, part_overrides={}):
    self.path = path
    self.dev = dev
    self.parts = {}
    for i in xrange(part_count):
      self.parts[i + 1] = path + 'p' + str(i + 1)
    self.parts.update(part_overrides)

  def close(self):
    pass

  def __enter__(self):
    return self

  def __exit__(self, exc_type, exc, tb):
    pass


FAKE_PATH = '/imaginary/file'
LOOP_DEV = '/dev/loop9999'
LOOP_PARTS_DICT = {num: '%sp%d' % (LOOP_DEV, num) for num in range(1, 13)}
LOOP_PARTS_LIST = LOOP_PARTS_DICT.values()

class LoopbackPartitionsTest(cros_test_lib.MockTestCase):
  """Test the loopback partitions class"""

  def setUp(self):
    self.rc_mock = cros_build_lib_unittest.RunCommandMock()
    self.StartPatcher(self.rc_mock)
    self.rc_mock.SetDefaultCmdResult()

    self.PatchObject(glob, 'glob', return_value=LOOP_PARTS_LIST)
    def fake_which(val, *_arg, **_kwargs):
      return val
    self.PatchObject(osutils, 'Which', side_effect=fake_which)

  def testContextManager(self):
    """Test using the loopback class as a context manager."""
    self.rc_mock.AddCmdResult(partial_mock.In('--show'), output=LOOP_DEV)
    with image_lib.LoopbackPartitions(FAKE_PATH) as lb:
      self.rc_mock.assertCommandContains(['losetup', '--show', '-f', FAKE_PATH])
      self.rc_mock.assertCommandContains(['partx', '-d', LOOP_DEV])
      self.rc_mock.assertCommandContains(['partx', '-a', LOOP_DEV])
      self.rc_mock.assertCommandContains(['losetup', '--detach', LOOP_DEV],
                                         expected=False)
      self.assertEquals(lb.parts, LOOP_PARTS_DICT)
    self.rc_mock.assertCommandContains(['partx', '-d', LOOP_DEV])
    self.rc_mock.assertCommandContains(['losetup', '--detach', LOOP_DEV])

  def testManual(self):
    """Test using the loopback class closed manually."""
    self.rc_mock.AddCmdResult(partial_mock.In('--show'), output=LOOP_DEV)
    lb = image_lib.LoopbackPartitions(FAKE_PATH)
    self.rc_mock.assertCommandContains(['losetup', '--show', '-f', FAKE_PATH])
    self.rc_mock.assertCommandContains(['partx', '-d', LOOP_DEV])
    self.rc_mock.assertCommandContains(['partx', '-a', LOOP_DEV])
    self.rc_mock.assertCommandContains(['losetup', '--detach', LOOP_DEV],
                                       expected=False)
    self.assertEquals(lb.parts, LOOP_PARTS_DICT)
    lb.close()
    self.rc_mock.assertCommandContains(['partx', '-d', LOOP_DEV])
    self.rc_mock.assertCommandContains(['losetup', '--detach', LOOP_DEV])

  def gcFunc(self):
    """This function isolates a local variable so it'll be garbage collected."""
    self.rc_mock.AddCmdResult(partial_mock.In('--show'), output=LOOP_DEV)
    lb = image_lib.LoopbackPartitions(FAKE_PATH)
    self.rc_mock.assertCommandContains(['losetup', '--show', '-f', FAKE_PATH])
    self.rc_mock.assertCommandContains(['partx', '-d', LOOP_DEV])
    self.rc_mock.assertCommandContains(['partx', '-a', LOOP_DEV])
    self.rc_mock.assertCommandContains(['losetup', '--detach', LOOP_DEV],
                                       expected=False)
    self.assertEquals(lb.parts, LOOP_PARTS_DICT)

  def testGarbageCollected(self):
    """Test using the loopback class closed by garbage collection."""
    self.gcFunc()
    # Force garbage collection in case python didn't already clean up the
    # loopback object.
    gc.collect()
    self.rc_mock.assertCommandContains(['partx', '-d', LOOP_DEV])
    self.rc_mock.assertCommandContains(['losetup', '--detach', LOOP_DEV])


class AppIdTest(cros_test_lib.TestCase):
  """Tests APP_ID utilities."""

  def testAppIdVerification(self):
    """Tests image_lib.VerifyAppId(..)."""
    # Valid APP ID.
    self.assertTrue(
        image_lib.VerifyAppId('{01234567-89AB-CDEF-0123-456789ABCDEF}'))

    # Test invalid non-hex character.
    self.assertFalse(
        image_lib.VerifyAppId('{01234567-89AB-CGEF-0123-456789ABCDEF}'))

    # Test invalid number of characters.
    self.assertFalse(
        image_lib.VerifyAppId('{0123456-89AB-CDEF-0123-456789ABCDEF}'))

    # Test no brackets.
    self.assertFalse(
        image_lib.VerifyAppId('01234567-89AB-CDEF-0123-456789ABCDEF'))

    # Test blank string.
    self.assertFalse(image_lib.VerifyAppId(''))


class LsbUtilsTest(cros_test_lib.MockTempDirTestCase):
  """Tests the various LSB utilities."""

  def setUp(self):
    # Patch os.getuid(..) to pretend running as root, so reading/writing the
    # lsb-release file doesn't require escalated privileges and the test can
    # clean itself up correctly.
    self.PatchObject(os, 'getuid', return_value=0)

  def testWriteLsbRelease(self):
    """Tests writing out the lsb_release file using WriteLsbRelease(..)."""
    fields = {'x': '1', 'y': '2', 'foo': 'bar'}
    image_lib.WriteLsbRelease(self.tempdir, fields)
    lsb_release_file = os.path.join(self.tempdir, 'etc', 'lsb-release')
    expected_content = 'y=2\nx=1\nfoo=bar\n'
    self.assertFileContents(lsb_release_file, expected_content)

    # Test that WriteLsbRelease(..) correctly handles an existing file.
    fields = {'newkey1': 'value1', 'newkey2': 'value2', 'a': '3', 'b': '4'}
    image_lib.WriteLsbRelease(self.tempdir, fields)
    expected_content = ('y=2\nx=1\nfoo=bar\nnewkey2=value2\na=3\n'
                        'newkey1=value1\nb=4\n')
    self.assertFileContents(lsb_release_file, expected_content)


class BuildImageTest(cros_test_lib.MockTestCase):
  """Test the BuildImage function."""

  def setUp(self):
    self.rc_mock = self.StartPatcher(cros_build_lib_unittest.RunCommandMock())
    self.rc_mock.SetDefaultCmdResult()

  def testBuildImageNoOptionalArguments(self):
    """Tests the BuildImage function calls with no optional arguments."""
    image_lib.BuildImage('fooboard')

    expected_args = [os.path.join(constants.CROSUTILS_DIR, 'build_image'),
                     '--board=fooboard',
                     '--noenable_bootcache', '--enable_rootfs_verification']
    self.rc_mock.assertCommandContains(expected_args)

  def testBuildImage(self):
    """Tests the BuildImage function makes the correct build_image call."""
    args = {'adjust_part': 'partx',
            'boot_args': 'bootx',
            'enable_bootcache': True,
            'enable_rootfs_verification': False,
            'output_root': 'rootx',
            'disk_layout': 'layoutx',
            'enable_serial': 'ttyx',
            'kernel_log_level': 5,
            'packages': ['cat1/foo', 'cat2/bar'],
            'image_types': ['base', 'test']}

    image_lib.BuildImage('fooboard', **args)

    expected_args = [os.path.join(constants.CROSUTILS_DIR, 'build_image'),
                     '--board=fooboard',
                     '--extra_packages=cat1/foo cat2/bar',
                     '--adjust_part=partx', '--boot_args=bootx',
                     '--enable_bootcache', '--noenable_rootfs_verification',
                     '--output_root=rootx', '--disk_layout=layoutx',
                     '--enable_serial=ttyx', '--loglevel=5', 'base', 'test']
    self.rc_mock.assertCommandContains(expected_args)

  def testBuildImageInvalidAppId(self):
    """Tests the BuildImage function throws an exception with invalid APP_ID."""
    args = {'adjust_part': 'partx',
            'app_id': '{012304-BADBADBAD}',
            'boot_args': 'bootx',
            'enable_bootcache': True,
            'enable_rootfs_verification': False,
            'output_root': 'rootx',
            'disk_layout': 'layoutx',
            'enable_serial': 'ttyx',
            'kernel_log_level': 5,
            'packages': ['cat1/foo', 'cat2/bar'],
            'image_types': ['base', 'test']}

    with self.assertRaises(image_lib.AppIdError):
      image_lib.BuildImage('fooboard', **args)


class BrilloImageOperationFake(image_lib.BrilloImageOperation):
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
  """Test class for image_lib.BrilloImageOperation."""

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

  def testParseOutputSummarize(self):
    """Test that the summary is logged correctly."""
    events = ['operation: summarize',
              'INFO    : foo',
              'operation: done summarize\nINFO    : bar']

    queue = multiprocessing.Queue()
    op = BrilloImageOperationFake(queue)
    with cros_test_lib.LoggingCapturer() as logs:
      op.Run(self.BrilloImageFake, events, queue)

    # Check that the logs contain the INFO message in func.
    self.AssertLogsContain(logs, 'foo')
    self.AssertLogsContain(logs, 'bar', inverted=True)

  def testExceptionHandling(self):
    """Test exception handling of BrilloImageOperation."""
    def func(queue):
      queue.get()
      print('foo')
      print('bar', file=sys.stderr)
      raise FakeException()

    queue = multiprocessing.Queue()
    op = BrilloImageOperationFake(queue)
    with cros_test_lib.LoggingCapturer() as logs:
      with self.OutputCapturer():
        try:
          op.Run(func, queue)
        except parallel.BackgroundFailure as e:
          if not e.HasFailureType(FakeException):
            raise e

    self.AssertOutputContainsLine('foo')
    self.AssertOutputContainsLine('bar', check_stderr='True')
    self.AssertLogsContain(logs, 'The output directory has been automatically '
                           'deleted. To keep it around, please re-run the '
                           'command with --log-level info.')
