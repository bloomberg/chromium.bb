# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for buildbucket_v2."""

from __future__ import print_function

from chromite.lib import buildbucket_v2
from chromite.lib import cros_test_lib
from chromite.lib.luci.prpc.client import Client


class BuildbucketV2Test(cros_test_lib.MockTestCase):
  """Tests for buildbucket_v2."""

  def testCreatesClient(self):
    ret = buildbucket_v2.GetBuildClient(test_env=True)
    self.assertIsInstance(ret, Client)
