# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for chroot management functions."""

from __future__ import print_function

import os

from chromite.cbuildbot import chroot_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils


class TestChrootManager(cros_test_lib.TempDirTestCase):
  """Class that tests the ChrootManager."""

  sudo_cleanup = True

  def setUp(self):
    self.chroot_manager = chroot_lib.ChrootManager(self.tempdir)

  def testGetChrootVersionWithNoChroot(self):
    """If there's no chroot, GetChrootVersion returns None."""
    self.assertIsNone(self.chroot_manager.GetChrootVersion('foo'))

  def testSetChrootVersionWithNoChroot(self):
    """If there's no chroot, SetChrootVersion does nothing."""
    self.chroot_manager.SetChrootVersion('foo')
    self.assertIsNone(self.chroot_manager.GetChrootVersion())

  def testSetChrootVersionWithChroot(self):
    """SetChrootVersion sets the chroot version."""
    osutils.SafeMakedirs(os.path.join(self.tempdir, 'chroot', 'etc'))
    self.chroot_manager.SetChrootVersion('foo')
    self.assertEquals('foo', self.chroot_manager.GetChrootVersion())

  def testClearChrootVersion(self):
    """SetChrootVersion sets the chroot version."""
    osutils.SafeMakedirs(os.path.join(self.tempdir, 'chroot', 'etc'))
    self.chroot_manager.SetChrootVersion('foo')
    self.assertEquals('foo', self.chroot_manager.GetChrootVersion())
    self.chroot_manager.ClearChrootVersion()
    self.assertIsNone(self.chroot_manager.GetChrootVersion())

  def testUseExistingChroot(self):
    """Tests that EnsureChrootAtVersion succeeds with valid chroot."""
    chroot = os.path.join(self.tempdir, 'chroot')
    osutils.SafeMakedirs(os.path.join(chroot, 'etc'))
    self.chroot_manager.SetChrootVersion('foo')
    self.chroot_manager.EnsureChrootAtVersion('foo')
    self.assertEquals(self.chroot_manager.GetChrootVersion(chroot), 'foo')

  def testUseFreshChroot(self):
    """Tests that EnsureChrootAtVersion succeeds with invalid chroot."""
    chroot = os.path.join(self.tempdir, 'chroot')
    self.chroot_manager.EnsureChrootAtVersion('foo')
    self.assertEquals(self.chroot_manager.GetChrootVersion(chroot), None)
