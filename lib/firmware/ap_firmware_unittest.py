# -*- coding: utf-8 -*-
# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for the ap_firmware module."""

from __future__ import print_function

import mock

from chromite.lib import build_target_util
from chromite.lib import cros_test_lib
from chromite.lib import workon_helper
from chromite.lib.firmware import ap_firmware


class BuildTest(cros_test_lib.RunCommandTestCase):
  """Tests for building ap firmware."""

  def test_valid_build_config(self):
    """Test building of the build config object."""
    module = mock.MagicMock(
        BUILD_WORKON_PACKAGES=('pkg1', 'pkg2'), BUILD_PACKAGES=('pkg3', 'pkg4'))

    self.PatchObject(ap_firmware, '_get_config_module', return_value=module)

    # pylint: disable=protected-access
    build_config = ap_firmware._get_build_config(
        build_target_util.BuildTarget('board'))

    self.assertEqual(('pkg1', 'pkg2'), build_config.workon)
    self.assertEqual(('pkg3', 'pkg4'), build_config.build)

  def test_no_workon_config(self):
    """Test building of the build config object with no workon packages."""
    module = mock.MagicMock(
        BUILD_WORKON_PACKAGES=None, BUILD_PACKAGES=('pkg3', 'pkg4'))

    self.PatchObject(ap_firmware, '_get_config_module', return_value=module)

    # pylint: disable=protected-access
    build_config = ap_firmware._get_build_config(
        build_target_util.BuildTarget('board'))

    self.assertFalse(build_config.workon)
    self.assertEqual(('pkg3', 'pkg4'), build_config.build)

  def test_invalid_build_config(self):
    """Test invalid build configs."""
    module = mock.MagicMock(
        BUILD_WORKON_PACKAGES=('pkg1', 'pkg2'), BUILD_PACKAGES=tuple())

    self.PatchObject(ap_firmware, '_get_config_module', return_value=module)

    with self.assertRaises(ap_firmware.InvalidConfigError):
      # pylint: disable=protected-access
      ap_firmware._get_build_config(build_target_util.BuildTarget('board'))

  def test_build(self):
    """Sanity checks the workon and command building functions properly."""
    # Note: The workon helper handles looking up full category/package atom
    # when just given package names.
    build_pkgs = ('build1', 'build2')
    workon_pkgs = ('workon1', 'workon2')
    # Inconsequential pkgs + 1 we need.
    existing_workons = ['cat/pkg1', 'cat/pkg2', 'cat/workon1']
    existing_and_required = existing_workons + ['cat/workon2']
    # Should only stop the ones that weren't previously worked on.
    expected_workon_stop = ['cat/workon2']

    build_config = ap_firmware.BuildConfig(workon=workon_pkgs, build=build_pkgs)
    build_target = build_target_util.BuildTarget('board')

    # Simulate starting the required workon packages. Return first the existing
    # workon packages, then the ones we're starting plus the existing.
    self.PatchObject(
        workon_helper.WorkonHelper,
        'ListAtoms',
        side_effect=[existing_workons, existing_and_required])
    # Start and stop workon patches for verifying calls.
    start_patch = self.PatchObject(workon_helper.WorkonHelper,
                                   'StartWorkingOnPackages')
    stop_patch = self.PatchObject(workon_helper.WorkonHelper,
                                  'StopWorkingOnPackages')

    # Patch in the build config.
    self.PatchObject(
        ap_firmware, '_get_build_config', return_value=build_config)

    ap_firmware.build(build_target)

    # Verify the workon packages. Should be starting all the required workon
    # packages, but only stopping the ones that we started in the command.
    start_patch.assert_called_once_with(workon_pkgs)
    stop_patch.assert_called_once_with(expected_workon_stop)
    # Verify we try to build all the build packages.
    self.rc.assertCommandContains(list(build_pkgs))
