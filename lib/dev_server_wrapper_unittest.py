#!/usr/bin/python
#
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for the dev_server_wrapper.py."""

import mock
import os
import sys
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))

from chromite.buildbot import constants
from chromite.lib import cros_test_lib
from chromite.lib import git
from chromite.lib import dev_server_wrapper


class TestChrootPathHelpers(cros_test_lib.TestCase):
  """Verify we correctly reinterpret paths to be used inside/outside chroot."""

  @mock.patch('chromite.lib.cros_build_lib.IsInsideChroot', return_value=True)
  def testToChrootPathInChroot(self, _inchroot_mock):
    """Test we return the original path to be used in chroot while in chroot."""
    path = '/foo/woo/bar'
    self.assertEqual(dev_server_wrapper.ToChrootPath(path), path)

  @mock.patch('chromite.lib.cros_build_lib.IsInsideChroot', return_value=False)
  def testToChrootPathOutChroot(self, _inchroot_mock):
    """Test we convert the path to be used in chroot while outside chroot."""
    subpath = 'bar/haa/ooo'
    path = os.path.join(constants.SOURCE_ROOT, subpath)
    chroot_path = git.ReinterpretPathForChroot(path)
    self.assertEqual(dev_server_wrapper.ToChrootPath(path), chroot_path)

  @mock.patch('chromite.lib.cros_build_lib.IsInsideChroot', return_value=True)
  def testFromChrootInChroot(self, _inchroot_mock):
    """Test we return the original chroot path while in chroot."""
    path = '/foo/woo/bar'
    self.assertEqual(dev_server_wrapper.FromChrootPath(path), path)

  @mock.patch('chromite.lib.cros_build_lib.IsInsideChroot', return_value=False)
  def testFromChrootOutChroot(self, _inchroot_mock):
    """Test we convert the chroot path to be used outside chroot."""
    # Test that chroot source root has been replaced in the path.
    subpath = 'foo/woo/bar'
    chroot_path = os.path.join(constants.CHROOT_SOURCE_ROOT, subpath)
    path = os.path.join(constants.SOURCE_ROOT, subpath)
    self.assertEqual(dev_server_wrapper.FromChrootPath(chroot_path), path)

    # Test that a chroot path has been converted.
    chroot_path = '/foo/woo/bar'
    path = os.path.join(constants.SOURCE_ROOT,
                        constants.DEFAULT_CHROOT_DIR,
                        chroot_path.strip(os.path.sep))
    self.assertEqual(dev_server_wrapper.FromChrootPath(chroot_path), path)


if __name__ == '__main__':
  cros_test_lib.main()
