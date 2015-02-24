# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test the bootstrap brillo command."""

from __future__ import print_function

import mock

from chromite.lib import cros_test_lib
from chromite.lib import git

from chromite.bootstrap.scripts import brillo


class TestBootstrapBrilloCmd(cros_test_lib.TestCase):
  """Tests for the bootstrap brillo command."""

  @mock.patch.object(git, 'FindRepoCheckoutRoot',
                     autospec=True, return_value='/repo')
  @mock.patch('os.execv', autospec=True)
  def _wrapperHelper(self, args, expectedInRepo, mock_exec, _mock_root):
    """Helper for testing what brillo.main execs."""
    brillo.main(args)

    self.assertEqual(len(mock_exec.call_args_list), 1)
    pos_args, kw_args = mock_exec.call_args_list[0]
    binary, arguments = pos_args

    if expectedInRepo:
      self.assertEqual(binary, '/repo/chromite/bin/brillo')
    else:
      self.assertTrue(binary.endswith, '../../bin/brillo')

    self.assertEqual(arguments, [binary] + args)
    self.assertEqual(kw_args, {})

  def testSdkInGitRepo(self):
    """Test that sdk commands are run in the Git Repository."""
    self._wrapperHelper(['sdk'], False)
    self._wrapperHelper(['sdk', '--help'], False)
    self._wrapperHelper(['sdk', 'millium', 'shrimp', 'and', 'hand'], False)

  def testBrilloCommandInRepository(self):
    """Test that non-sdk commands are run in the Repo checkout."""
    self._wrapperHelper([], True)
    self._wrapperHelper(['flash'], True)
    self._wrapperHelper(['flash', '--help'], True)
    self._wrapperHelper(['super', 'long', 'arg', 'list', 'of', 'stuff'], True)
