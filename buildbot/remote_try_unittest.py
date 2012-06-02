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
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.buildbot import remote_try
from chromite.buildbot import repository
from chromite.scripts import cbuildbot

class RemoteTryJobMock(remote_try.RemoteTryJob):
  pass

# pylint: disable=W0212,R0904,E1101
class RemoteTryTests(mox.MoxTestBase, cros_test_lib.TempDirMixin):

  PATCHES = ('5555', '6666')
  BOTS = ('x86-generic-paladin', 'arm-generic-paladin')

  def setUp(self):
    cros_test_lib.TempDirMixin.setUp(self)

    self.parser = cbuildbot._CreateParser()
    args = ['-r', '/tmp/test_build1', '-g', '5555', '-g',
            '6666', '--remote']
    args.extend(self.BOTS)
    self.options, args = cbuildbot._ParseCommandLine(self.parser, args)
    self.checkout_dir = os.path.join(self.tempdir, 'test_checkout')

    mox.MoxTestBase.setUp(self)

  def tearDown(self):
    cros_test_lib.TempDirMixin.tearDown(self)
    mox.MoxTestBase.tearDown(self)

  def _RunCommandSingleOutput(self, cmd, cwd):
    result = cros_build_lib.RunCommandCaptureOutput(cmd, cwd=cwd)
    out_lines = result.output.split()
    self.assertEqual(len(out_lines), 1)
    return out_lines[0]

  def _GetNewestFile(self, dirname, basehash):
    newhash = cros_build_lib.GetGitRepoRevision(dirname)
    self.assertNotEqual(basehash, newhash)
    cmd = ['git', 'log', '--format=%H', '%s..' % basehash]
    # Make sure we have a single commit.
    self._RunCommandSingleOutput(cmd, cwd=dirname)
    cmd = ['git', 'diff', '--name-only', 'HEAD^']
    # Make sure only one file per commit.
    return self._RunCommandSingleOutput(cmd, cwd=dirname)

  def _SubmitJob(self, checkout_dir, job):
    """Returns the path to the tryjob description."""
    self.assertTrue(isinstance(job, RemoteTryJobMock))
    basehash = cros_build_lib.GetGitRepoRevision(job.ssh_url)
    job.Submit(workdir=checkout_dir, dryrun=True)
    # Get the file that was just created.
    created_file = self._GetNewestFile(checkout_dir, basehash)
    return os.path.join(checkout_dir, created_file)

  def _SetupMirrors(self):
    mirror = os.path.join(self.tempdir, 'tryjobs_mirror')
    os.mkdir(mirror)
    url = '%s/%s' % (constants.GIT_HTTP_URL, 'chromiumos/tryjobs')
    repository.CloneGitRepo(mirror, url,
                            bare=True)
    self.ext_mirror = mirror
    mirror = os.path.join(self.tempdir, 'tryjobs_int_mirror')
    os.mkdir(mirror)
    repository.CloneGitRepo(mirror, self.ext_mirror, reference=self.ext_mirror,
                            bare=True)
    self.int_mirror = mirror
    RemoteTryJobMock.EXT_SSH_URL = self.ext_mirror
    RemoteTryJobMock.INT_SSH_URL = self.int_mirror

  def _CreateJob(self, mirror=True):
    job_class = remote_try.RemoteTryJob
    if mirror:
      job_class = RemoteTryJobMock
      self._SetupMirrors()

    job = job_class(self.options, self.BOTS, [])
    return job

  def testJobTimestamp(self):
    """Verify jobs have unique names."""
    def submit_helper(dirname):
      work_dir = os.path.join(self.tempdir, dirname)
      return os.path.basename(self._SubmitJob(work_dir, job))

    self.mox.StubOutWithMock(repository, 'IsARepoRoot')
    repository.IsARepoRoot(mox.IgnoreArg()).AndReturn(False)
    self.mox.ReplayAll()
    job = self._CreateJob()

    file1 = submit_helper('test1')
    # Tryjob file names are based on timestamp, so delay one second to avoid two
    # jobfiles having the same name.
    time.sleep(1)
    file2 = submit_helper('test2')
    self.assertNotEqual(file1, file2)

  def testSimpleTryJob(self):
    """Test that a tryjob spec file is created and pushed properly."""
    self.mox.StubOutWithMock(repository, 'IsARepoRoot')
    repository.IsARepoRoot(mox.IgnoreArg()).AndReturn(True)
    self.mox.StubOutWithMock(repository, 'IsInternalRepoCheckout')
    repository.IsInternalRepoCheckout(mox.IgnoreArg()).AndReturn(False)

    self.mox.ReplayAll()
    job = self._CreateJob()
    created_file = self._SubmitJob(self.checkout_dir, job)
    with open(created_file, 'rb') as job_desc_file:
      values = json.load(job_desc_file)

    self.assertFalse(re.search('\@google\.com', values['email'][0]) is None and
                     re.search('\@chromium\.org', values['email'][0]) is None)

    for patch in self.PATCHES:
      self.assertTrue(patch in values['extra_args'],
                      msg="expected patch %s in args %s" %
                          (patch, values['extra_args']))

    self.assertTrue(set(self.BOTS).issubset(values['bot']))

    remote_url = cros_build_lib.RunCommand(
        ['git', 'config', 'remote.origin.url'], redirect_stdout=True,
        cwd=self.checkout_dir).output.strip()
    self.assertEqual(remote_url, self.ext_mirror)

  def testInternalTryJob(self):
    """Verify internal tryjobs are pushed properly."""
    self.mox.StubOutWithMock(repository, 'IsARepoRoot')
    repository.IsARepoRoot(mox.IgnoreArg()).AndReturn(True)
    self.mox.StubOutWithMock(repository, 'IsInternalRepoCheckout')
    repository.IsInternalRepoCheckout(mox.IgnoreArg()).AndReturn(True)

    self.mox.ReplayAll()
    job = self._CreateJob()
    self._SubmitJob(self.checkout_dir, job)

    remote_url = cros_build_lib.RunCommand(
        ['git', 'config', 'remote.origin.url'], redirect_stdout=True,
        cwd=self.checkout_dir).output.strip()
    self.assertEqual(remote_url, self.int_mirror)

  def testBareTryJob(self):
    """Verify submitting a tryjob from just a chromite checkout works."""
    self.mox.StubOutWithMock(repository, 'IsARepoRoot')
    repository.IsARepoRoot(mox.IgnoreArg()).AndReturn(False)
    self.mox.StubOutWithMock(repository, 'IsInternalRepoCheckout')

    self.mox.ReplayAll()
    job = self._CreateJob(mirror=False)
    self.assertEqual(job.ssh_url, remote_try.RemoteTryJob.EXT_SSH_URL)


if __name__ == '__main__':
  unittest.main()
