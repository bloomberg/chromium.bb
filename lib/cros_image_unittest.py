# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test the cros_image module."""

from __future__ import print_function

import gc
import glob

from chromite.lib import cros_build_lib_unittest
from chromite.lib import cros_image
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import partial_mock


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
    with cros_image.LoopbackPartitions(FAKE_PATH) as lb:
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
    lb = cros_image.LoopbackPartitions(FAKE_PATH)
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
    lb = cros_image.LoopbackPartitions(FAKE_PATH)
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
