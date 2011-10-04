#!/usr/bin/python

# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for commands.  Needs to be run inside of chroot for mox."""

import glob
import mox
import os
import re
import sys
import unittest

import constants
sys.path.insert(0, constants.SOURCE_ROOT)
from chromite.lib import cros_test_lib as test_lib
from chromite.buildbot import remote_try

# pylint: disable=W0212,R0904
class RemoteTryTests(mox.MoxTestBase):

  @test_lib.tempdir_decorator
  def testSimpleTryJob(self):
    """Test that a tryjob spec file is created and pushed properly."""
    PATCHES = ['5555', '6666']
    BOTS = ['x86-generic-pre-flight-queue', 'arm-generic-pre-flight-queue']
    options = mox.MockAnything()
    options.gerrit_patches = PATCHES
    self.mox.ReplayAll()
    job = remote_try.RemoteTryJob(options, BOTS)
    job.Submit(workdir=self.tempdir, dryrun=True)
    # Get the file that was just created.
    created_file = sorted(glob.glob(os.path.join(self.tempdir,
                          job.user + '*')), reverse=True)[0]
    with open(created_file, 'r') as job_desc_file:
      contents = job_desc_file.read()

    self.assertFalse(re.search('\@google\.com', contents) is None and
                     re.search('\@chromium\.org', contents) is None)

    self.assertFalse(re.search(PATCHES[0], contents) is None or
                     re.search(PATCHES[1], contents) is None)

    self.assertFalse(re.search(BOTS[0], contents) is None or
                     re.search(BOTS[1], contents) is None)

if __name__ == '__main__':
  unittest.main()
