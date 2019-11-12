# -*- coding: utf-8 -*-
# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for verifying prebuilts."""

from __future__ import print_function

from chromite.lib import cros_test_lib


class PrebuiltCompatibilityTest(cros_test_lib.TestCase):
  """Ensure that prebuilts are present for all builders and are compatible."""

  # TODO:(https://crbug.com/1021398) get rid of this test as chrome PFQ no
  # longer runs in legacy and any such tests would need to exist in the new
  # PCQ world.
