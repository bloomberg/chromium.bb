# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""uprev_lib tests."""

from __future__ import print_function

import mock

from chromite.lib import cros_test_lib
from chromite.lib import parallel
from chromite.lib import uprev_lib
from chromite.lib.build_target_util import BuildTarget
from chromite.lib.chroot_lib import Chroot


class UprevManagerTest(cros_test_lib.MockTestCase):
  """UprevManager tests."""

  def test_clean_stale_packages_no_chroot(self):
    """Test no chroot skip."""
    manager = uprev_lib.UprevOverlayManager([], None)
    patch = self.PatchObject(parallel, 'RunTasksInProcessPool')

    # pylint: disable=protected-access
    manager._clean_stale_packages()

    # Make sure we aren't doing any work.
    patch.assert_not_called()

  def test_clean_stale_packages_chroot_not_exists(self):
    """Cannot run the commands when the chroot does not exist."""
    chroot = Chroot()
    self.PatchObject(chroot, 'exists', return_value=False)
    manager = uprev_lib.UprevOverlayManager([], None, chroot=chroot)
    patch = self.PatchObject(parallel, 'RunTasksInProcessPool')

    # pylint: disable=protected-access
    manager._clean_stale_packages()

    # Make sure we aren't doing any work.
    patch.assert_not_called()

  def test_clean_stale_packages_no_build_targets(self):
    """Make sure it behaves as expected with no build targets provided."""
    chroot = Chroot()
    self.PatchObject(chroot, 'exists', return_value=True)
    manager = uprev_lib.UprevOverlayManager([], None, chroot=chroot)
    patch = self.PatchObject(parallel, 'RunTasksInProcessPool')

    # pylint: disable=protected-access
    manager._clean_stale_packages()

    # Make sure we aren't doing any work.
    patch.assert_called_once_with(mock.ANY, [[None]])

  def test_clean_stale_packages_with_boards(self):
    """Test it cleans all boards as well as the chroot."""
    targets = ['board1', 'board2']
    build_targets = [BuildTarget(t) for t in targets]
    chroot = Chroot()
    self.PatchObject(chroot, 'exists', return_value=True)
    manager = uprev_lib.UprevOverlayManager([], None, chroot=chroot,
                                            build_targets=build_targets)
    patch = self.PatchObject(parallel, 'RunTasksInProcessPool')

    # pylint: disable=protected-access
    manager._clean_stale_packages()

    patch.assert_called_once_with(mock.ANY, [[t] for t in targets + [None]])
