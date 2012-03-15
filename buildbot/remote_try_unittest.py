#!/usr/bin/python

# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for commands.  Needs to be run inside of chroot for mox."""

import glob
import json
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
    BOTS = ['x86-generic-paladin', 'arm-generic-paladin']
    options = mox.MockAnything()
    options.gerrit_patches = PATCHES
    self.mox.ReplayAll()
    job = remote_try.RemoteTryJob(options, BOTS)
    job.Submit(workdir=self.tempdir, dryrun=True)
    # Get the file that was just created.
    created_file = sorted(glob.glob(os.path.join(self.tempdir, job.user,
                          job.user + '*')), reverse=True)[0]
    with open(created_file, 'rb') as job_desc_file:
      values = json.load(job_desc_file)

    self.assertFalse(re.search('\@google\.com', values['email'][0]) is None and
                     re.search('\@chromium\.org', values['email'][0]) is None)

    for arg in values['extra_args']:
      if PATCHES[0] in arg and PATCHES[1] in arg:
        break
    else:
      self.assertTrue(False, "Patches couldn't be found in extra_args...")

    self.assertTrue(BOTS[0] in values['bot'] and
                    BOTS[1] in values['bot'])

if __name__ == '__main__':
  unittest.main()
