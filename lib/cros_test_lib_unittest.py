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


if __name__ == '__main__':
  cros_test_lib.main()
