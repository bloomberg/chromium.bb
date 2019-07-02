# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""chroot_lib unit tests."""

from __future__ import print_function

import os

from chromite.lib import chroot_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils


class ChrootTest(cros_test_lib.TempDirTestCase):
  """Chroot class tests."""

  def testGetEnterArgsEmpty(self):
    """Test empty instance behavior."""
    chroot = chroot_lib.Chroot()
    self.assertFalse(chroot.GetEnterArgs())

  def testGetEnterArgsAll(self):
    """Test complete instance behavior."""
    path = '/chroot/path'
    cache_dir = '/cache/dir'
    chrome_root = '/chrome/root'
    expected = ['--chroot', path, '--cache-dir', cache_dir,
                '--chrome-root', chrome_root]
    chroot = chroot_lib.Chroot(path=path, cache_dir=cache_dir,
                               chrome_root=chrome_root)

    self.assertItemsEqual(expected, chroot.GetEnterArgs())

  def testEnv(self):
    """Test the env handling."""
    env = {'VAR': 'val'}
    chroot = chroot_lib.Chroot(env=env)
    self.assertEqual(env, chroot.env)

  def testTempdir(self):
    """Test the tempdir functionality."""
    chroot = chroot_lib.Chroot(path=self.tempdir)
    osutils.SafeMakedirs(chroot.tmp)

    self.assertEqual(os.path.join(self.tempdir, 'tmp'), chroot.tmp)

    with chroot.tempdir() as tempdir:
      self.assertStartsWith(tempdir, chroot.tmp)

    self.assertNotExists(tempdir)

  def testExists(self):
    """Test chroot exists."""
    chroot = chroot_lib.Chroot(self.tempdir)
    self.assertTrue(chroot.exists())

    chroot = chroot_lib.Chroot(os.path.join(self.tempdir, 'DOES_NOT_EXIST'))
    self.assertFalse(chroot.exists())

  def testFullPath(self):
    """Test full_path functionality."""
    chroot = chroot_lib.Chroot(self.tempdir)
    path1 = 'some/path'
    path2 = '/some/path'

    # Make sure it's building out the path in the chroot.
    self.assertEqual(os.path.join(self.tempdir, path1), chroot.full_path(path1))
    # Make sure it can handle absolute paths.
    self.assertEqual(chroot.full_path(path1), chroot.full_path(path2))

  def testHasPathSuccess(self):
    """Test has path for a valid path."""
    path = 'some/file.txt'
    tempdir_path = os.path.join(self.tempdir, path)
    osutils.Touch(tempdir_path, makedirs=True)

    chroot = chroot_lib.Chroot(self.tempdir)
    self.assertTrue(chroot.has_path(path))

  def testHasPathInvalidPath(self):
    """Test has path for a non-existent path."""
    chroot = chroot_lib.Chroot(self.tempdir)
    self.assertFalse(chroot.has_path('/does/not/exist'))
