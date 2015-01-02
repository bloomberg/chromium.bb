# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests the `cros shell` command."""

from __future__ import print_function

from chromite.cros.commands import cros_shell
from chromite.cros.commands import init_unittest
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import remote_access


class MockShellCommand(init_unittest.MockCommand):
  """Mock out the `cros shell` command."""
  TARGET = 'chromite.cros.commands.cros_shell.ShellCommand'
  TARGET_CLASS = cros_shell.ShellCommand
  COMMAND = 'shell'

  def __init__(self, *args, **kwargs):
    init_unittest.MockCommand.__init__(self, *args, **kwargs)

  def Run(self, inst):
    init_unittest.MockCommand.Run(self, inst)


def _SshConnectError():
  """Returns an error indicating a general SSH error."""
  return remote_access.SSHConnectionError('(test) SSH Error')


def _KeyMismatchError():
  """Returns an error indicating an SSH host key mismatch."""
  return remote_access.SSHConnectionError(
      '(test) REMOTE HOST IDENTIFICATION HAS CHANGED')


class ShellTest(cros_test_lib.MockTempDirTestCase):
  """Test the flow of ShellCommand.run with the SSH methods mocked out."""

  DEVICE = '1.1.1.1'

  def SetupCommandMock(self, cmd_args):
    """Sets up the `cros shell` command mock."""
    self.cmd_mock = MockShellCommand(
        cmd_args, base_args=['--cache-dir', self.tempdir])
    self.StartPatcher(self.cmd_mock)

  def setUp(self):
    """Patches objects."""
    self.cmd_mock = None
    # Patch any functions we want to control that may get called by a test.
    self.remove_known_host_function = self.PatchObject(remote_access,
                                                       'RemoveKnownHost')
    self.prompt_function = self.PatchObject(cros_build_lib, 'BooleanPrompt')
    # Patch the remote_access.RemoteAccess object, then drill down to the
    # RemoteSh() function which would do the actual SSH call in order to
    # easily set and check test conditions.
    self.patched_remote_access = self.PatchObject(remote_access, 'RemoteAccess')
    self.remote_access_instance = self.patched_remote_access.return_value
    self.remote_sh_function = self.remote_access_instance.RemoteSh

  def testSshSuccess(self):
    """Tests flow for a basic connection.

    User should not be prompted for input, and SSH should be attempted
    once.
    """
    self.SetupCommandMock([self.DEVICE])
    self.cmd_mock.inst.Run()
    self.assertEqual(self.remote_sh_function.call_count, 1)
    self.assertFalse(self.prompt_function.called)
    # Make sure that RemoteSh() started an interactive session (cmd is None).
    self.assertIs(self.remote_sh_function.call_args[0][0], None)

  def testSshKeyChangeOK(self):
    """Tests a host SSH key changing but the user giving it the OK.

    User should be prompted, SSH should be attempted twice, and host
    keys should be removed.
    """
    self.SetupCommandMock([self.DEVICE])
    # RemoteSh() gives a key mismatch error the first time only.
    self.remote_sh_function.side_effect = [_KeyMismatchError(), None]
    # User chooses to continue.
    self.prompt_function.return_value = True
    self.cmd_mock.inst.Run()
    self.assertTrue(self.prompt_function.called)
    self.assertEqual(self.remote_sh_function.call_count, 2)
    self.assertTrue(self.remove_known_host_function.called)

  def testSshKeyChangeAbort(self):
    """Tests a host SSH key changing and the user canceling.

    User should be prompted, but SSH should only be attempted once, and
    no host keys should be removed.
    """
    self.SetupCommandMock([self.DEVICE])
    self.remote_sh_function.side_effect = _KeyMismatchError()
    # User chooses to abort.
    self.prompt_function.return_value = False
    self.cmd_mock.inst.Run()
    self.assertTrue(self.prompt_function.called)
    self.assertEqual(self.remote_sh_function.call_count, 1)
    self.assertFalse(self.remove_known_host_function.called)

  def testSshConnectError(self):
    """Tests an SSH error other than a host key mismatch.

    User should not be prompted, SSH should only be attempted once, and
    no host keys should be removed.
    """
    self.SetupCommandMock([self.DEVICE])
    self.remote_sh_function.side_effect = _SshConnectError()
    self.cmd_mock.inst.Run()
    self.assertFalse(self.prompt_function.called)
    self.assertEqual(self.remote_sh_function.call_count, 1)
    self.assertFalse(self.remove_known_host_function.called)
