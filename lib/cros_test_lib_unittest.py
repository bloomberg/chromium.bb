#!/usr/bin/python

# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.realpath(__file__)),
                                '..', '..'))

from chromite.lib import cros_test_lib
from chromite.lib import cros_build_lib_unittest
from chromite.lib import partial_mock

# TODO(build): Finish test wrapper (http://crosbug.com/37517).
# Until then, this has to be after the chromite imports.
import mock

# pylint: disable=W0212,W0233

# Convenience alias
Dir = cros_test_lib.Directory


class VerifyTarballTest(cros_test_lib.MockTempDirTestCase):
  """Test tarball verification functionality."""

  TARBALL = 'fake_tarball'

  def setUp(self):
    self.rc_mock = self.StartPatcher(cros_build_lib_unittest.RunCommandMock())

  def _MockTarList(self, files):
    """Mock out tarball content list call.

    Arguments:
      files: A list of contents to return.
    """
    self.rc_mock.AddCmdResult(
        partial_mock.ListRegex('tar -tf'), output='\n'.join(files))

  def testNormPath(self):
    """Test path normalization."""
    tar_contents = ['./', './foo/', './foo/./a', './foo/./b']
    dir_struct = [Dir('.', []), Dir('foo', ['a', 'b'])]
    self._MockTarList(tar_contents)
    cros_test_lib.VerifyTarball(self.TARBALL, dir_struct)

  def testDuplicate(self):
    """Test duplicate detection."""
    tar_contents = ['a', 'b', 'a']
    dir_struct = ['a', 'b']
    self._MockTarList(tar_contents)
    self.assertRaises(AssertionError, cros_test_lib.VerifyTarball, self.TARBALL,
                      dir_struct)


class MockTestCaseTest(cros_test_lib.TestCase):
  """Tests MockTestCase functionality."""

  class MyMockTestCase(cros_test_lib.MockTestCase):
    def testIt(self):
      pass

  class Mockable(object):
    TO_BE_MOCKED = 0
    TO_BE_MOCKED2 = 10
    TO_BE_MOCKED3 = 20

  def GetPatcher(self, attr, val):
    return mock.patch('__main__.MockTestCaseTest.Mockable.%s' % attr,
                      new=val)

  def testPatchRemovalError(self):
    """Verify that patch removal during tearDown is robust to Exceptions."""
    tc = self.MyMockTestCase('testIt')
    patcher = self.GetPatcher('TO_BE_MOCKED', -100)
    patcher2 = self.GetPatcher('TO_BE_MOCKED2', -200)
    patcher3 = self.GetPatcher('TO_BE_MOCKED3', -300)
    patcher3.start()
    tc.setUp()
    tc.StartPatcher(patcher)
    tc.StartPatcher(patcher2)
    patcher.stop()
    self.assertEquals(self.Mockable.TO_BE_MOCKED2, -200)
    self.assertEquals(self.Mockable.TO_BE_MOCKED3, -300)
    self.assertRaises(RuntimeError, tc.tearDown)
    # Make sure that even though exception is raised for stopping 'patcher', we
    # continue to stop 'patcher2', and run patcher.stopall().
    self.assertEquals(self.Mockable.TO_BE_MOCKED2, 10)
    self.assertEquals(self.Mockable.TO_BE_MOCKED3, 20)


if __name__ == '__main__':
  cros_test_lib.main()
