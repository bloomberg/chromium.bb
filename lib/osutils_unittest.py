#!/usr/bin/python
#
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for the osutils.py module (imagine that!)."""

import os
import sys
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))

from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils


class TestOsutils(cros_test_lib.TempDirTestCase):

  def testReadWriteFile(self):
    """Verify we can write data to a file, and then read it back."""
    filename = os.path.join(self.tempdir, 'foo')
    data = 'alsdkfjasldkfjaskdlfjasdf'
    self.assertEqual(osutils.WriteFile(filename, data), None)
    self.assertEqual(osutils.ReadFile(filename), data)

  def testSafeUnlink(self):
    """Test unlinking files work (existing or not)."""
    def f(dirname, sudo=False):
      dirname = os.path.join(self.tempdir, dirname)
      path = os.path.join(dirname, 'foon')
      os.makedirs(dirname)
      open(path, 'w').close()
      self.assertTrue(os.path.exists(path))
      if sudo:
        cros_build_lib.SudoRunCommand(
            ['chown', 'root:root', '-R', '--', dirname], print_cmd=False)
        self.assertRaises(EnvironmentError, os.unlink, path)
      self.assertTrue(osutils.SafeUnlink(path, sudo=sudo))
      self.assertFalse(os.path.exists(path))
      self.assertFalse(osutils.SafeUnlink(path))
      self.assertFalse(os.path.exists(path))

    f("nonsudo", False)
    f("sudo", True)

  def testSafeMakedirs(self):
    """Test creating directory trees work (existing or not)."""
    path = os.path.join(self.tempdir, 'a', 'b', 'c', 'd', 'e')
    self.assertTrue(osutils.SafeMakedirs(path))
    self.assertTrue(os.path.exists(path))
    self.assertFalse(osutils.SafeMakedirs(path))
    self.assertTrue(os.path.exists(path))

  def testSafeMakedirs_error(self):
    """Check error paths."""
    self.assertRaises(OSError, osutils.SafeMakedirs, '/foo/bar/cow/moo/wee')
    self.assertRaises(OSError, osutils.SafeMakedirs, '')

  def testSafeMakedirsSudo(self):
    """Test creating directory trees work as root (existing or not)."""
    path = os.path.join(self.tempdir, 'a', 'b', 'c', 'd', 'e')
    self.assertTrue(osutils.SafeMakedirs(path, sudo=True))
    self.assertTrue(os.path.exists(path))
    self.assertFalse(osutils.SafeMakedirs(path, sudo=True))
    self.assertTrue(os.path.exists(path))
    self.assertEqual(os.stat(path).st_uid, 0)
    # Have to manually clean up as a non-root `rm -rf` will fail.
    cros_build_lib.SudoRunCommand(['rm', '-rf', self.tempdir], print_cmd=False)

  def testRmDir(self):
    """Test that removing dirs work."""
    path = os.path.join(self.tempdir, 'a', 'b', 'c', 'd', 'e')

    self.assertRaises(EnvironmentError, osutils.RmDir, path)
    osutils.SafeMakedirs(path)
    osutils.RmDir(path)
    osutils.RmDir(path, ignore_missing=True)
    self.assertRaises(EnvironmentError, osutils.RmDir, path)

    osutils.SafeMakedirs(path)
    osutils.RmDir(path)
    self.assertFalse(os.path.exists(path))

  def testRmDirSudo(self):
    """Test that removing dirs via sudo works."""
    subpath = os.path.join(self.tempdir, 'a')
    path = os.path.join(subpath, 'b', 'c', 'd', 'e')
    self.assertTrue(osutils.SafeMakedirs(path, sudo=True))
    self.assertRaises(OSError, osutils.RmDir, path)
    osutils.RmDir(subpath, sudo=True)
    self.assertRaises(cros_build_lib.RunCommandError,
                      osutils.RmDir, subpath, sudo=True)


if __name__ == '__main__':
  cros_test_lib.main()
