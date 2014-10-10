# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

# pylint: disable=E0611,F0401
import mojo.embedder
import mojo.system as system


class MojoTestCase(unittest.TestCase):

  def setUp(self):
    mojo.embedder.Init()
    self.loop = system.RunLoop()

  def tearDown(self):
    self.loop = None
