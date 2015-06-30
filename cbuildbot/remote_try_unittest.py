# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for remote_try.py."""

from __future__ import print_function

import json
import mock
import os
import shutil
import time

from chromite.cbuildbot import config_lib
from chromite.cbuildbot import config_lib_unittest
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import git
from chromite.cbuildbot import remote_try
from chromite.cbuildbot import repository
from chromite.scripts import cbuildbot


site_config = config_lib.GetConfig()


class RemoteTryJobMock(remote_try.RemoteTryJob):
  """Helper for Mocking out a RemoteTryJob."""


@cros_test_lib.NetworkTest()
class RemoteTryTests(cros_test_lib.MockTempDirTestCase):
  """Test cases related to remote try jobs."""

  PATCHES = ('5555', '6666')
  BOTS = ('x86-generic-paladin', 'arm-generic-paladin')

  def setUp(self):
    # pylint: disable=protected-access
    self.site_config = config_lib_unittest.MockSiteConfig()
    self.parser = cbuildbot._CreateParser()
    args = ['-r', '/tmp/test_build1', '-g', '5555', '-g',
            '6666', '--remote']
    args.extend(self.BOTS)
    self.options, args = cbuildbot._ParseCommandLine(self.parser, args)
    self.options.cache_dir = self.tempdir
    self.checkout_dir = os.path.join(self.tempdir, 'test_checkout')
    self.int_mirror, self.ext_mirror = None, None

    self.PatchObject(remote_try.RemoteTryJob, '_BuildBucketAuth', mock.Mock())

  def _RunGitSingleOutput(self, cwd, cmd):
    result = git.RunGit(cwd, cmd)
    out_lines = result.output.split()
    self.assertEqual(len(out_lines), 1)
    return out_lines[0]

  def _GetNewestFile(self, dirname, basehash):
    newhash = git.GetGitRepoRevision(dirname)
    self.assertNotEqual(basehash, newhash)
    cmd = ['log', '--format=%H', '%s..' % basehash]
    # Make sure we have a single commit.
    self._RunGitSingleOutput(dirname, cmd)
    cmd = ['diff', '--name-only', 'HEAD^']
    # Make sure only one file per commit.
    return self._RunGitSingleOutput(dirname, cmd)

  def _SubmitJob(self, checkout_dir, job, version=None):
    """Returns the path to the tryjob description."""
    self.assertTrue(isinstance(job, RemoteTryJobMock))
    basehash = git.GetGitRepoRevision(job.repo_url)
    if version is not None:
      self._SetMirrorVersion(version)
    job.Submit(workdir=checkout_dir, dryrun=True)
    # Get the file that was just created.
    created_file = self._GetNewestFile(checkout_dir, basehash)
    return os.path.join(checkout_dir, created_file)

  def _SetupMirrors(self):
    mirror = os.path.join(self.tempdir, 'tryjobs_mirror')
    os.mkdir(mirror)
    url = '%s/%s' % (site_config.params.EXTERNAL_GOB_URL, 'chromiumos/tryjobs')
    repository.CloneGitRepo(mirror, url,
                            bare=True)
    self.ext_mirror = mirror
    mirror = os.path.join(self.tempdir, 'tryjobs_int_mirror')
    os.mkdir(mirror)
    repository.CloneGitRepo(mirror, self.ext_mirror, reference=self.ext_mirror,
                            bare=True)

    self.int_mirror = mirror
    RemoteTryJobMock.EXTERNAL_URL = self.ext_mirror
    RemoteTryJobMock.INTERNAL_URL = self.int_mirror
    self._SetMirrorVersion(remote_try.RemoteTryJob.TRYJOB_FORMAT_VERSION, True)

  def _SetMirrorVersion(self, version, only_if_missing=False):
    for path in (self.ext_mirror, self.int_mirror):
      vpath = os.path.join(path, remote_try.RemoteTryJob.TRYJOB_FORMAT_FILE)
      if os.path.exists(vpath) and only_if_missing:
        continue
      # Get ourselves a working dir.
      tmp_repo = os.path.join(self.tempdir, 'tmp-repo')
      git.RunGit(self.tempdir, ['clone', path, tmp_repo])
      vpath = os.path.join(tmp_repo, remote_try.RemoteTryJob.TRYJOB_FORMAT_FILE)
      with open(vpath, 'w') as f:
        f.write(str(version))
      git.RunGit(tmp_repo, ['add', vpath])
      git.RunGit(tmp_repo, ['commit', '-m', 'setting version to %s' % version])
      git.RunGit(tmp_repo, ['push', path, 'master:master'])
      shutil.rmtree(tmp_repo)

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

    self.PatchObject(repository, 'IsARepoRoot', return_value=False)
    job = self._CreateJob()

    file1 = submit_helper('test1')
    # Tryjob file names are based on timestamp, so delay one second to avoid two
    # jobfiles having the same name.
    time.sleep(1)
    file2 = submit_helper('test2')
    self.assertNotEqual(file1, file2)

  def testSimpleTryJob(self, version=None):
    """Test that a tryjob spec file is created and pushed properly."""
    self.PatchObject(repository, 'IsARepoRoot', return_value=True)
    self.PatchObject(repository, 'IsInternalRepoCheckout', return_value=False)

    try:
      os.environ["GIT_AUTHOR_EMAIL"] = "Elmer Fudd <efudd@google.com>"
      os.environ["GIT_COMMITTER_EMAIL"] = "Elmer Fudd <efudd@google.com>"
      job = self._CreateJob()
    finally:
      os.environ.pop("GIT_AUTHOR_EMAIL", None)
      os.environ.pop("GIT_COMMITTER_EMAIL", None)
    created_file = self._SubmitJob(self.checkout_dir, job, version=version)
    with open(created_file, 'rb') as job_desc_file:
      values = json.load(job_desc_file)

    self.assertTrue('efudd@google.com' in values['email'][0])

    for patch in self.PATCHES:
      self.assertTrue(patch in values['extra_args'],
                      msg=("expected patch %s in args %s" %
                           (patch, values['extra_args'])))

    self.assertTrue(set(self.BOTS).issubset(values['bot']))

    remote_url = cros_build_lib.RunCommand(
        ['git', 'config', 'remote.origin.url'], redirect_stdout=True,
        cwd=self.checkout_dir).output.strip()
    self.assertEqual(remote_url, self.ext_mirror)

  def testClientVersionAwareness(self):
    self.assertRaises(
        remote_try.ChromiteUpgradeNeeded,
        self.testSimpleTryJob,
        version=remote_try.RemoteTryJob.TRYJOB_FORMAT_VERSION + 1)

  def testInternalTryJob(self):
    """Verify internal tryjobs are pushed properly."""
    self.PatchObject(repository, 'IsARepoRoot', return_value=True)
    self.PatchObject(repository, 'IsInternalRepoCheckout', return_value=True)

    job = self._CreateJob()
    self._SubmitJob(self.checkout_dir, job)

    remote_url = cros_build_lib.RunCommand(
        ['git', 'config', 'remote.origin.url'], redirect_stdout=True,
        cwd=self.checkout_dir).output.strip()
    self.assertEqual(remote_url, self.int_mirror)

  def testBareTryJob(self):
    """Verify submitting a tryjob from just a chromite checkout works."""
    self.PatchObject(repository, 'IsARepoRoot', return_value=False)
    self.PatchObject(repository, 'IsInternalRepoCheckout',
                     side_effect=Exception('should not be called'))

    job = self._CreateJob(mirror=False)
    self.assertEqual(job.repo_url, remote_try.RemoteTryJob.EXTERNAL_URL)
