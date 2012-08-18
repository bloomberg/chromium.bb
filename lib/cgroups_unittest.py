#!/usr/bin/python
#
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import unittest
import sys
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))

from chromite.buildbot import cbuildbot_background as bg
from chromite.lib import cgroups
from chromite.lib import cros_build_lib
from chromite.lib import sudo


class TestCreateGroups(unittest.TestCase):

  def _CrosSdk(self):
    cmd = ['cros_sdk', '--', 'sleep', '0.001']
    cros_build_lib.RunCommand(cmd)

  def testCreateGroups(self):
    """Run many cros_sdk processes in parallel to test for race conditions."""
    with sudo.SudoKeepAlive():
      with cgroups.SimpleContainChildren('example', sigterm_timeout=5):
        bg.RunTasksInProcessPool(self._CrosSdk, [[]] * 50, 10)


if __name__ == '__main__':
  cros_build_lib.SetupBasicLogging()
  unittest.main()
