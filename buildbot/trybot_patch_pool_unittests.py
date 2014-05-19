#!/usr/bin/python
# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for trybot_patch_pool."""

from __future__ import print_function

import sys

import constants
sys.path.insert(0, constants.SOURCE_ROOT)
from chromite.buildbot import trybot_patch_pool
from chromite.lib import cros_test_lib
from chromite.lib import patch as cros_patch
from chromite.lib import patch_unittest


class FilterTests(patch_unittest.GitRepoPatchTestCase):
  """Tests for all the various filters."""

  patch_kls = cros_patch.LocalPatch

  def testChromiteFilter(self):
    """Make sure the chromite filter works"""
    _, _, patch = self._CommonGitSetup()
    patch.project = constants.CHROMITE_PROJECT
    self.assertTrue(trybot_patch_pool.ChromiteFilter(patch))
    patch.project = 'foooo'
    self.assertFalse(trybot_patch_pool.ChromiteFilter(patch))

  def testManifestFilters(self):
    """Make sure the manifest filters work"""
    _, _, patch = self._CommonGitSetup()

    patch.project = constants.CHROMITE_PROJECT
    self.assertFalse(trybot_patch_pool.ExtManifestFilter(patch))
    self.assertFalse(trybot_patch_pool.IntManifestFilter(patch))
    self.assertFalse(trybot_patch_pool.ManifestFilter(patch))

    patch.project = constants.MANIFEST_PROJECT
    self.assertTrue(trybot_patch_pool.ExtManifestFilter(patch))
    self.assertFalse(trybot_patch_pool.IntManifestFilter(patch))
    self.assertTrue(trybot_patch_pool.ManifestFilter(patch))

    patch.project = constants.MANIFEST_INT_PROJECT
    self.assertFalse(trybot_patch_pool.ExtManifestFilter(patch))
    self.assertTrue(trybot_patch_pool.IntManifestFilter(patch))
    self.assertTrue(trybot_patch_pool.ManifestFilter(patch))

  def testBranchFilter(self):
    """Make sure the branch filter works"""
    _, _, patch = self._CommonGitSetup()

    self.assertFalse(trybot_patch_pool.BranchFilter('/,/asdf', patch))
    self.assertTrue(trybot_patch_pool.BranchFilter(
        patch.tracking_branch, patch))


if __name__ == '__main__':
  cros_test_lib.main()
