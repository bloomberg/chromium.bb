# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for remote_try.py."""

from __future__ import print_function

import os

from chromite.lib import auth
from chromite.lib import config_lib
from chromite.lib import cros_test_lib
from chromite.lib import remote_try
from chromite.cbuildbot import repository


site_config = config_lib.GetConfig()


class RemoteTryJobMock(remote_try.RemoteTryJob):
  """Helper for Mocking out a RemoteTryJob."""


@cros_test_lib.NetworkTest()
class RemoteTryTests(cros_test_lib.MockTempDirTestCase):
  """Test cases related to remote try jobs."""

  PATCHES = ('5555', '6666')
  BOTS = ('amd64-generic-paladin', 'arm-generic-paladin')

  def setUp(self):
    self.branch = 'test-branch'
    self.patches = ['5555']
    self.pass_through_args = ['funky', 'cold', 'medina']
    self.committer_email = 'test@test.foo'

    self.checkout_dir = os.path.join(self.tempdir, 'test_checkout')
    self.int_mirror, self.ext_mirror = None, None

  def _CreateJob(self):
    description = remote_try.DefaultDescription(self.branch, self.patches)

    return remote_try.RemoteTryJob(
        self.BOTS, [],
        self.pass_through_args,
        description,
        self.committer_email)

  def testSimpleTryJob(self):
    """Test that a tryjob spec file is created and pushed properly."""
    self.PatchObject(repository, 'IsARepoRoot', return_value=True)
    # self.PatchObject(repository, 'IsInternalRepoCheckout', return_value=False)

    try:
      os.environ["GIT_AUTHOR_EMAIL"] = "Elmer Fudd <efudd@google.com>"
      os.environ["GIT_COMMITTER_EMAIL"] = "Elmer Fudd <efudd@google.com>"
      self._CreateJob()
    finally:
      os.environ.pop("GIT_AUTHOR_EMAIL", None)
      os.environ.pop("GIT_COMMITTER_EMAIL", None)


class RemoteTryJobTests(cros_test_lib.MockTempDirTestCase):
  """Tests for RemoteTryJob."""

  # pylint: disable=protected-access
  def testPostConfigsToBuildBucket(self):
    """Check syntax for PostConfigsToBuildBucket."""
    self.PatchObject(auth, 'Login')
    self.PatchObject(auth, 'Token')
    self.PatchObject(remote_try.RemoteTryJob, '_PutConfigToBuildBucket')
    remote_try_job = remote_try.RemoteTryJob(
        'build_config', [], [], self.tempdir, 'description')
    remote_try_job._PostConfigsToBuildBucket(testjob=True, dryrun=True)
