#!/usr/bin/python
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for chromite.lib.git and helpers for testing that module."""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))

from chromite.lib import cros_build_lib
from chromite.lib import cros_build_lib_unittest
from chromite.lib import cros_test_lib
from chromite.lib import git
from chromite.lib import partial_mock

import mock


class ManifestMock(partial_mock.PartialMock):
  """Partial mock for git.Manifest."""
  TARGET = 'chromite.lib.git.Manifest'
  ATTRS = ('_RunParser',)

  def _RunParser(self, *_args):
    pass


class ManifestCheckoutMock(partial_mock.PartialMock):
  """Partial mock for git.ManifestCheckout."""
  TARGET = 'chromite.lib.git.ManifestCheckout'
  ATTRS = ('_GetManifestsBranch',)

  def _GetManifestsBranch(self, _root):
    return 'default'


class GitPushTest(cros_test_lib.MockTestCase):
  """Tests for git.GitPush function."""

  # Non fast-forward push error message.
  NON_FF_PUSH_ERROR = ('To https://localhost/repo.git\n'
      '! [remote rejected] master -> master (non-fast-forward)\n'
      'error: failed to push some refs to \'https://localhost/repo.git\'\n')

  # List of possible GoB transient errors.
  TRANSIENT_ERRORS = (
      # Error when creating a new branch from SHA1 ref.
      ('remote: Processing changes: (-)To https://localhost/repo.git\n'
       '! [remote rejected] 6c78ca083c3a9d64068c945fd9998eb1e0a3e739 -> '
       'stabilize-4636.B (error in hook)\n'
       'error: failed to push some refs to \'https://localhost/repo.git\'\n'),

      # Error when pushing branch.
      ('remote: Processing changes: (\)To https://localhost/repo.git\n'
       '! [remote rejected] temp_auto_checkin_branch -> '
       'master (error in hook)\n'
       'error: failed to push some refs to \'https://localhost/repo.git\'\n'),

      # Another kind of error when pushing a branch.
      'fatal: remote error: Internal Server Error',
  )

  def setUp(self):
    self.StartPatcher(mock.patch('time.sleep'))

  @staticmethod
  def _RunGitPush():
    """Runs git.GitPush with some default arguments."""
    git.GitPush('some_repo_path', 'local-ref',
                git.RemoteRef('some-remote', 'remote-ref'),
                dryrun=True, retry=True)

  def testPushSuccess(self):
    """Test handling of successful git push."""
    with cros_build_lib_unittest.RunCommandMock() as rc_mock:
      rc_mock.AddCmdResult(partial_mock.In('push'), returncode=0)
      self._RunGitPush()

  def testNonFFPush(self):
    """Non fast-forward push error propagates to the caller."""
    with cros_build_lib_unittest.RunCommandMock() as rc_mock:
      rc_mock.AddCmdResult(partial_mock.In('push'), returncode=128,
                           error=self.NON_FF_PUSH_ERROR)
      self.assertRaises(cros_build_lib.RunCommandError, self._RunGitPush)

  def testPersistentTransientError(self):
    """GitPush fails if transient error occurs multiple times."""
    for error in self.TRANSIENT_ERRORS:
      with cros_build_lib_unittest.RunCommandMock() as rc_mock:
        rc_mock.AddCmdResult(partial_mock.In('push'), returncode=128,
                             error=error)
        self.assertRaises(cros_build_lib.RunCommandError, self._RunGitPush)

  def testOneTimeTransientError(self):
    """GitPush retries transient errors."""
    for error in self.TRANSIENT_ERRORS:
      with cros_build_lib_unittest.RunCommandMock() as rc_mock:
        results = [
            rc_mock.CmdResult(128, '', error),
            rc_mock.CmdResult(0, 'success', ''),
        ]
        side_effect = lambda *_args, **_kwargs: results.pop(0)
        rc_mock.AddCmdResult(partial_mock.In('push'), side_effect=side_effect)
        self._RunGitPush()


if __name__ == '__main__':
  cros_test_lib.main()
