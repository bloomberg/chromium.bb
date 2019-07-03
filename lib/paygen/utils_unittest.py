# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test Utils library."""

from __future__ import print_function

import os

from chromite.lib import cros_test_lib
from chromite.lib import osutils

from chromite.lib.paygen import utils


class TestUtils(cros_test_lib.TempDirTestCase):
  """Test utils methods."""

  def testListdirFullpath(self):
    file_a = os.path.join(self.tempdir, 'a')
    file_b = os.path.join(self.tempdir, 'b')

    osutils.Touch(file_a)
    osutils.Touch(file_b)

    self.assertEqual(sorted(utils.ListdirFullpath(self.tempdir)),
                     [file_a, file_b])

  def testReadLsbRelease(self):
    """Tests that we correctly read the lsb release file."""
    path = os.path.join(self.tempdir, 'etc', 'lsb-release')
    osutils.WriteFile(path, 'key=value\nfoo=bar\n', makedirs=True)

    self.assertEqual(utils.ReadLsbRelease(self.tempdir),
                     {'key': 'value', 'foo': 'bar'})
