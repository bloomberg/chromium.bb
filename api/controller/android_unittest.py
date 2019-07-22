# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for Android operations."""

from __future__ import print_function

from chromite.api import api_config
from chromite.api.controller import android
from chromite.api.gen.chromite.api import android_pb2
from chromite.api.gen.chromiumos import common_pb2
# TODO(crbug/904939): implement service/android.py
from chromite.cbuildbot import commands
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils

class MarkStableTest(cros_test_lib.MockTestCase, api_config.ApiConfigMixin):
  """Unittests for MarkStable."""

  def setUp(self):
    self.command = self.PatchObject(commands, 'MarkAndroidAsStable')

    self.input_proto = android_pb2.MarkStableRequest()
    self.input_proto.tracking_branch = 'tracking-branch'
    self.input_proto.package_name = 'android-package-name'
    self.input_proto.android_build_branch = 'android_build_branch'
    self.board1 = self.input_proto.boards.add()
    self.board1.name = 'board1'
    self.board2 = self.input_proto.boards.add()
    self.board2.name = 'board2'

    self.response = android_pb2.MarkStableResponse()

  def testValidateOnly(self):
    """Sanity check that a validate only call does not execute any logic."""
    patch = self.PatchObject(commands, 'MarkAndroidAsStable')

    android.MarkStable(self.input_proto, self.response,
                       self.validate_only_config)
    patch.assert_not_called()

  def testFailsIfTrackingBranchMissing(self):
    """Fails if tracking_branch is missing."""
    self.input_proto.tracking_branch = ''
    with self.assertRaises(cros_build_lib.DieSystemExit):
      android.MarkStable(self.input_proto, self.response, self.api_config)
    self.command.assert_not_called()

  def testFailsIfPackageNameMissing(self):
    """Fails if package_name is missing."""
    self.input_proto.package_name = ''
    with self.assertRaises(cros_build_lib.DieSystemExit):
      android.MarkStable(self.input_proto, self.response, self.api_config)
    self.command.assert_not_called()

  def testFailsIfAndroidBuildBranchMissing(self):
    """Fails if android_build_branch is missing."""
    self.input_proto.android_build_branch = ''
    with self.assertRaises(cros_build_lib.DieSystemExit):
      android.MarkStable(self.input_proto, self.response, self.api_config)
    self.command.assert_not_called()

  def testCallsCommandCorrectly(self):
    """Test that commands.MarkAndroidAsStable is called correctly."""
    self.input_proto.android_version = 'android-version'
    self.input_proto.android_gts_build_branch = 'gts-branch'
    self.command.return_value = 'cat/android-1.2.3'
    atom = common_pb2.PackageInfo()
    atom.category = 'cat'
    atom.package_name = 'android'
    atom.version = '1.2.3'
    android.MarkStable(self.input_proto, self.response, self.api_config)
    self.command.assert_called_once_with(
        buildroot=self.input_proto.buildroot,
        tracking_branch=self.input_proto.tracking_branch,
        android_package=self.input_proto.package_name,
        android_build_branch=self.input_proto.android_build_branch,
        boards=self.input_proto.boards,
        android_version=self.input_proto.android_version,
        android_gts_build_branch=self.input_proto.android_gts_build_branch)
    self.assertEqual(self.response.android_atom, atom)
    self.assertEqual(self.response.status,
                     android_pb2.MARK_STABLE_STATUS_SUCCESS)

  def testHandlesEarlyExit(self):
    """Test that early exit is handled correctly."""
    self.input_proto.android_version = 'android-version'
    self.input_proto.android_gts_build_branch = 'gts-branch'
    self.command.return_value = ''
    android.MarkStable(self.input_proto, self.response, self.api_config)
    self.command.assert_called_once_with(
        buildroot=self.input_proto.buildroot,
        tracking_branch=self.input_proto.tracking_branch,
        android_package=self.input_proto.package_name,
        android_build_branch=self.input_proto.android_build_branch,
        boards=self.input_proto.boards,
        android_version=self.input_proto.android_version,
        android_gts_build_branch=self.input_proto.android_gts_build_branch)
    self.assertEqual(self.response.status,
                     android_pb2.MARK_STABLE_STATUS_EARLY_EXIT)

  def testHandlesPinnedUprevError(self):
    """Test that pinned error is handled correctly."""
    self.input_proto.android_version = 'android-version'
    self.input_proto.android_gts_build_branch = 'gts-branch'
    self.command.side_effect = commands.AndroidIsPinnedUprevError('pin/xx-1.1')
    atom = common_pb2.PackageInfo()
    atom.category = 'pin'
    atom.package_name = 'xx'
    atom.version = '1.1'
    android.MarkStable(self.input_proto, self.response, self.api_config)
    self.command.assert_called_once_with(
        buildroot=self.input_proto.buildroot,
        tracking_branch=self.input_proto.tracking_branch,
        android_package=self.input_proto.package_name,
        android_build_branch=self.input_proto.android_build_branch,
        boards=self.input_proto.boards,
        android_version=self.input_proto.android_version,
        android_gts_build_branch=self.input_proto.android_gts_build_branch)
    self.assertEqual(self.response.android_atom, atom)
    self.assertEqual(self.response.status,
                     android_pb2.MARK_STABLE_STATUS_PINNED)


class UnpinVersionTest(cros_test_lib.MockTestCase, api_config.ApiConfigMixin):
  """Unittests for UnpinVersion."""

  def testCallsUnlink(self):
    """SetAndroid calls service with correct args."""
    safeunlink = self.PatchObject(osutils, 'SafeUnlink')
    self.PatchObject(constants, '_FindSourceRoot', return_value='SRCROOT')

    # This has the side effect of making sure that input and output proto are
    # not actually used.
    android.UnpinVersion(None, None, self.api_config)
    safeunlink.assert_called_once_with(android.ANDROIDPIN_MASK_PATH)

  def testValidateOnly(self):
    """Sanity check that a validate only call does not execute any logic."""
    safeunlink = self.PatchObject(osutils, 'SafeUnlink')

    android.UnpinVersion(None, None, self.validate_only_config)
    safeunlink.assert_not_called()
