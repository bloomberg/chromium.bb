# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Image API unittests."""

from __future__ import print_function

import os

from chromite.lib.api import image
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import image_lib


class ImageTestTest(cros_test_lib.RunCommandTempDirTestCase):
  """Image Test tests."""

  def setUp(self):
    """Setup the filesystem."""
    self.board = 'board'
    self.chroot_container = os.path.join(self.tempdir, 'outside')
    self.outside_result_dir = os.path.join(self.chroot_container, 'results')
    self.inside_result_dir_inside = '/inside/results_inside'
    self.inside_result_dir_outside = os.path.join(self.chroot_container,
                                                  'inside/results_inside')
    self.image_dir_inside = '/inside/build/board/latest'
    self.image_dir_outside = os.path.join(self.chroot_container,
                                          'inside/build/board/latest')

    D = cros_test_lib.Directory
    filesystem = (
        D('outside', (
            D('results', ()),
            D('inside', (
                D('results_inside', ()),
                D('build', (
                    D('board', (
                        D('latest', ('%s.bin' % constants.BASE_IMAGE_NAME,)),
                    )),
                )),
            )),
        )),
    )

    cros_test_lib.CreateOnDiskHierarchy(self.tempdir, filesystem)

  def testTestFailsInvalidArguments(self):
    """Test invalid arguments are correctly failed."""
    with self.assertRaises(image.InvalidArgumentError):
      image.Test(None, None)
    with self.assertRaises(image.InvalidArgumentError):
      image.Test('', '')
    with self.assertRaises(image.InvalidArgumentError):
      image.Test(None, self.outside_result_dir)
    with self.assertRaises(image.InvalidArgumentError):
      image.Test(self.board, None)

  def testTestInsideChrootAllProvided(self):
    """Test behavior when inside the chroot and all paths provided."""
    self.PatchObject(cros_build_lib, 'IsInsideChroot', return_value=True)
    image.Test(self.board, self.outside_result_dir,
               image_dir=self.image_dir_inside)

    # Inside chroot shouldn't need to do any path manipulations, so we should
    # see exactly what we called it with.
    self.assertCommandContains(['--board', self.board,
                                '--test_results_root', self.outside_result_dir,
                                self.image_dir_inside])

  def testTestInsideChrootNoImageDir(self):
    """Test image dir generation inside the chroot."""
    mocked_dir = '/foo/bar'
    self.PatchObject(cros_build_lib, 'IsInsideChroot', return_value=True)
    self.PatchObject(image_lib, 'GetLatestImageLink', return_value=mocked_dir)
    image.Test(self.board, self.outside_result_dir)

    self.assertCommandContains(['--board', self.board,
                                '--test_results_root', self.outside_result_dir,
                                mocked_dir])
