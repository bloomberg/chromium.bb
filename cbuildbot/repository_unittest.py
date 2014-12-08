# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for repository.py."""

from __future__ import print_function

import os

from chromite.cbuildbot import constants
from chromite.cbuildbot import repository
from chromite.lib import cros_build_lib_unittest
from chromite.lib import cros_test_lib


class RepositoryTests(cros_build_lib_unittest.RunCommandTestCase):
  """Test cases related to repository checkout methods."""

  def testExternalRepoCheckout(self):
    """Test we detect external checkouts properly."""
    tests = [
        'https://chromium.googlesource.com/chromiumos/manifest.git',
        'test@abcdef.bla.com:39291/bla/manifest.git',
        'test@abcdef.bla.com:39291/bla/manifest',
        'test@abcdef.bla.com:39291/bla/Manifest-internal',
    ]

    for test in tests:
      self.rc.SetDefaultCmdResult(output=test)
      self.assertFalse(repository.IsInternalRepoCheckout('.'))

  def testInternalRepoCheckout(self):
    """Test we detect internal checkouts properly."""
    tests = [
        'https://chrome-internal.googlesource.com/chromeos/manifest-internal',
        'test@abcdef.bla.com:39291/bla/manifest-internal.git',
    ]

    for test in tests:
      self.rc.SetDefaultCmdResult(output=test)
      self.assertTrue(repository.IsInternalRepoCheckout('.'))


class RepoInitTests(cros_test_lib.TempDirTestCase):
  """Test cases related to repository initialization."""

  def _Initialize(self, branch='master'):
    repo = repository.RepoRepository(constants.MANIFEST_URL, self.tempdir,
                                     branch=branch)
    repo.Initialize()

  @cros_test_lib.NetworkTest()
  def testReInitialization(self):
    """Test ability to switch between branches."""
    self._Initialize('release-R19-2046.B')
    self._Initialize('master')

    # Test that a failed re-init due to bad branch doesn't leave repo in bad
    # state.
    self.assertRaises(Exception, self._Initialize, 'monkey')
    self._Initialize('release-R20-2268.B')


class RepoInitChromeBotTests(RepoInitTests):
  """Test that Re-init works with the chrome-bot account.

  In testing, repo init behavior on the buildbots is different from a
  local run, because there is some logic in 'repo' that filters changes based on
  GIT_COMMITTER_IDENT.  So for sanity's sake, try to emulate running on the
  buildbots.
  """
  def setUp(self):
    os.putenv('GIT_COMMITTER_EMAIL', 'chrome-bot@chromium.org')
    os.putenv('GIT_AUTHOR_EMAIL', 'chrome-bot@chromium.org')
