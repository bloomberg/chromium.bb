#!/usr/bin/python

# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import functools
import mock
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.realpath(__file__)),
                                '..', '..'))
from chromite.lib import cros_test_lib
from chromite.lib import cros_build_lib_unittest
from chromite.lib import partial_mock

# pylint: disable=W0212,W0233

# Convenience alias
Dir = cros_test_lib.Directory


class VerifyTarballTest(cros_test_lib.TempDirTestCase):
  """Test tarball verification functionality."""

  TARBALL = 'fake_tarball'

  def setUp(self):
    self.rc_mock = cros_build_lib_unittest.RunCommandMock()
    self.rc_mock.Start()

  def tearDown(self):
    self.rc_mock.Stop()

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


class SafeRunTest(cros_test_lib.TestCase):
  """Tests SafeRunTest functionality."""

  def _raise_exception(self, e):
    raise e

  def testRunsSafely(self):
    """Verify that we are robust to exceptions."""
    def append_val(value):
      call_list.append(value)

    call_list = []
    f_list = [functools.partial(append_val, 1),
              functools.partial(self._raise_exception,
                                Exception('testRunsSafely exception.')),
              functools.partial(append_val, 2)]
    self.assertRaises(Exception, cros_test_lib.SafeRun, f_list)
    self.assertEquals(call_list, [1, 2])

  def testRaisesFirstException(self):
    """Verify we raise the first exception when multiple are encountered."""
    class E1(Exception):
      pass

    class E2(Exception):
      pass

    f_list = [functools.partial(self._raise_exception, e) for e in [E1, E2]]
    self.assertRaises(E1, cros_test_lib.SafeRun, f_list)

  def testCombinedRaise(self):
    """Raises a RuntimeError with exceptions combined."""
    f_list = [functools.partial(self._raise_exception, Exception())] * 3
    self.assertRaises(RuntimeError, cros_test_lib.SafeRun, f_list,
                      combine_exceptions=True)


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
