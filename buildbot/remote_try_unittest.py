#!/usr/bin/python

# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import mox
import os
import re
import sys
import time
import unittest

import constants
sys.path.insert(0, constants.SOURCE_ROOT)
from chromite.lib import cros_build_lib as cros_lib
from chromite.lib import osutils
from chromite.buildbot import remote_try
from chromite.buildbot import repository
from chromite.scripts import cbuildbot

# pylint: disable=W0212,R0904,E1101
class RemoteTryTests(mox.MoxTestBase):

  PATCHES = ('5555', '6666')
  BOTS = ('x86-generic-paladin', 'arm-generic-paladin')

  def setUp(self):
    mox.MoxTestBase.setUp(self)
    self.parser = cbuildbot._CreateParser()
    args = ['-r', '/tmp/test_build1', '-g', '5555', '-g',
            '6666', '--remote']
    args.extend(self.BOTS)
    self.options, args = cbuildbot._ParseCommandLine(self.parser, args)

  @osutils.TempDirDecorator
  def testSimpleTryJob(self):
    """Test that a tryjob spec file is created and pushed properly."""
    self.mox.StubOutWithMock(repository, 'IsARepoRoot')
    repository.IsARepoRoot(mox.IgnoreArg()).AndReturn(True)
    self.mox.StubOutWithMock(repository, 'IsInternalRepoCheckout')
    repository.IsInternalRepoCheckout(mox.IgnoreArg()).AndReturn(False)

    self.mox.StubOutWithMock(time, 'time')
    time.time().AndReturn(0)
    self.mox.ReplayAll()
    job = remote_try.RemoteTryJob(self.options, self.BOTS, [])
    job.Submit(workdir=self.tempdir, dryrun=True)
    # Get the file that was just created.
    created_file = os.path.join(self.tempdir, job.user, '%s.0' % job.user)
    with open(created_file, 'rb') as job_desc_file:
      values = json.load(job_desc_file)

    self.assertFalse(re.search('\@google\.com', values['email'][0]) is None and
                     re.search('\@chromium\.org', values['email'][0]) is None)

    for patch in self.PATCHES:
      self.assertTrue(patch in values['extra_args'],
                      msg="expected patch %s in args %s" %
                          (patch, values['extra_args']))

    self.assertTrue(set(self.BOTS).issubset(values['bot']))

    remote_url = cros_lib.RunCommand(['git', 'config', 'remote.origin.url'],
                                     redirect_stdout=True,
                                     cwd=self.tempdir).output.strip()
    self.assertTrue(remote_url == remote_try.RemoteTryJob.EXT_SSH_URL)

  @osutils.TempDirDecorator
  def testInternalTryJob(self):
    """Verify internal tryjobs are pushed properly."""
    self.mox.StubOutWithMock(repository, 'IsARepoRoot')
    repository.IsARepoRoot(mox.IgnoreArg()).AndReturn(True)
    self.mox.StubOutWithMock(repository, 'IsInternalRepoCheckout')
    repository.IsInternalRepoCheckout(mox.IgnoreArg()).AndReturn(True)

    self.mox.ReplayAll()
    job = remote_try.RemoteTryJob(self.options, self.BOTS, [])
    job.Submit(workdir=self.tempdir, dryrun=True)

    remote_url = cros_lib.RunCommand(['git', 'config', 'remote.origin.url'],
                                     redirect_stdout=True,
                                     cwd=self.tempdir).output.strip()
    self.assertTrue(remote_url == remote_try.RemoteTryJob.INT_SSH_URL)

  def testBareTryJob(self):
    """Verify submitting a tryjob from just a chromite checkout works."""
    self.mox.StubOutWithMock(repository, 'IsARepoRoot')
    repository.IsARepoRoot(mox.IgnoreArg()).AndReturn(False)
    self.mox.StubOutWithMock(repository, 'IsInternalRepoCheckout')

    self.mox.ReplayAll()
    job = remote_try.RemoteTryJob(self.options, self.BOTS, [])
    self.assertTrue(job.ssh_url == remote_try.RemoteTryJob.EXT_SSH_URL)


if __name__ == '__main__':
  unittest.main()
