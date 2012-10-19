#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests of hashing tools."""

import os
import unittest

import file_tools
import hashing_tools
import working_directory


class HashChecker(object):
  """Mock hasher which checks two hashing passes match."""

  def __init__(self, test_case):
    """Constructor.

    Args:
      test_case: A TestCase to run assertions against.
    """
    self._items = []
    self._checking = False
    self._test_case = test_case

  def update(self, data):
    """Standard method for a hasher.

    Used in initial mode to program expectations.
    Used in checking mode to check them.
    """
    if self._checking:
      item = self._items.pop(0)
      self._test_case.assertEquals(item, data)
    else:
      self._items.append(data)

  def StartChecking(self):
    """Switch from programming the mock to testing for a match."""
    self._test_case.assertNotEqual(0, len(self._items))
    self._checking = True

  def FinishChecking(self):
    """Confirm the all expected values have been seen."""
    self._test_case.assertEquals(0, len(self._items))


def GenerateTestTree(noise, path):
  """Generate an interesting tree for testing.

  Args:
    noise: A string to inject to vary tree content.
    path: Location to emit tree (should not exist).
  """
  os.mkdir(path)
  b = os.path.join(path, 'b' + noise)
  os.mkdir(b)
  b1 = os.path.join(path, 'b1' + noise)
  os.mkdir(b1)
  b2 = os.path.join(path, 'b2' + noise)
  file_tools.WriteFile('inside b2' + noise, b2)
  c = os.path.join(b, 'c' + noise)
  file_tools.WriteFile('inside c' + noise, c)


class TestHashingTools(unittest.TestCase):

  def test_File(self):
    # Check that one file works.
    with working_directory.TemporaryWorkingDirectory() as work_dir:
      filename1 = os.path.join(work_dir, 'myfile1')
      filename2 = os.path.join(work_dir, 'myfile2')
      file_tools.WriteFile('booga', filename1)
      file_tools.WriteFile('booga', filename2)
      checker = HashChecker(self)
      hashing_tools.StableHashPath(filename1, checker)
      checker.StartChecking()
      hashing_tools.StableHashPath(filename2, checker)
      checker.FinishChecking()

  def test_Directory(self):
    # Check that a directory tree works.
    with working_directory.TemporaryWorkingDirectory() as work_dir:
      a1 = os.path.join(work_dir, 'a1')
      a2 = os.path.join(work_dir, 'a2')
      for path in [a1, a2]:
        GenerateTestTree('gorp', path)
      checker = HashChecker(self)
      hashing_tools.StableHashPath(a1, checker)
      checker.StartChecking()
      hashing_tools.StableHashPath(a2, checker)
      checker.FinishChecking()


if __name__ == '__main__':
  unittest.main()
