#!/usr/bin/python

# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import functools
import os
import sys

import constants
sys.path.insert(0, constants.SOURCE_ROOT)
from chromite.buildbot import repository
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib

# pylint: disable=W0212,R0904,E1101,W0613
class RepositoryTests(cros_test_lib.MoxTestCase):

  def RunCommand_Mock(self, result, *args, **kwargs):
    output = self.mox.CreateMockAnything()
    output.output = result
    return output

  def testExternalRepoCheckout(self):
    """Test we detect external checkouts properly."""
    self.mox.StubOutWithMock(cros_build_lib, 'RunCommand')
    tests = [
        'http//git.chromium.org/chromiumos/manifest.git',
        'ssh://gerrit-int.chromium.org:29419/chromeos/manifest.git',
        'test@abcdef.bla.com:39291/bla/manifest.git',
        'test@abcdef.bla.com:39291/bla/manifest',
        'test@abcdef.bla.com:39291/bla/Manifest-internal',
     ]

    for test in tests:
      cros_build_lib.RunCommand = functools.partial(self.RunCommand_Mock, test)
      self.assertFalse(repository.IsInternalRepoCheckout('.'))

  def testInternalRepoCheckout(self):
    """Test we detect internal checkouts properly."""
    self.mox.StubOutWithMock(cros_build_lib, 'RunCommand')
    tests = [
        'ssh://gerrit-int.chromium.org:29419/chromeos/manifest-internal.git',
        'ssh://gerrit-int.chromium.org:29419/chromeos/manifest-internal',
        'ssh://gerrit.chromium.org:29418/chromeos/manifest-internal',
        'test@abcdef.bla.com:39291/bla/manifest-internal.git',
    ]

    for test in tests:
      cros_build_lib.RunCommand = functools.partial(self.RunCommand_Mock, test)
      self.assertTrue(repository.IsInternalRepoCheckout('.'))


class RepoInitTests(cros_test_lib.MoxTempDirTestCase):

  def _Initialize(self, branch='master'):
    repo = repository.RepoRepository(constants.MANIFEST_URL, self.tempdir,
                                     branch=branch)
    repo.Initialize()

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


if __name__ == '__main__':
  cros_test_lib.main()
