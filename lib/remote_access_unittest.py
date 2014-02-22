#!/usr/bin/python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test the remote_access module."""

import os
import re
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.realpath(__file__)),
                                '..', '..'))
from chromite.lib import cros_build_lib
from chromite.lib import cros_build_lib_unittest
from chromite.lib import cros_test_lib
from chromite.lib import partial_mock
from chromite.lib import remote_access

# pylint: disable=W0212


class TestNormalizePort(cros_test_lib.TestCase):
  """Verifies we normalize port."""

  def testNormalizePortStrOK(self):
    """Tests that string will be converted to integer."""
    self.assertEqual(remote_access.NormalizePort('123'), 123)

  def testNormalizePortStrNotOK(self):
    """Tests that error is raised if port is string and str_ok=False."""
    self.assertRaises(
        ValueError, remote_access.NormalizePort, '123', str_ok=False)

  def testNormalizePortOutOfRange(self):
    """Tests that error is rasied when port is out of range."""
    self.assertRaises(ValueError, remote_access.NormalizePort, '-1')
    self.assertRaises(ValueError, remote_access.NormalizePort, 99999)


class RemoteShMock(partial_mock.PartialCmdMock):
  """Mocks the RemoteSh function."""
  TARGET = 'chromite.lib.remote_access.RemoteAccess'
  ATTRS = ('RemoteSh',)
  DEFAULT_ATTR =  'RemoteSh'

  def RemoteSh(self, inst, cmd, *args, **kwargs):
    """Simulates a RemoteSh invocation."""
    result = self._results['RemoteSh'].LookupResult(
        (cmd,), hook_args=(inst, cmd,) + args, hook_kwargs=kwargs)

    # Run the real RemoteSh with RunCommand mocked out.
    rc_mock = cros_build_lib_unittest.RunCommandMock()
    rc_mock.AddCmdResult(
        partial_mock.Ignore(), result.returncode, result.output,
        result.error)

    with rc_mock:
      return self.backup['RemoteSh'](inst, cmd, *args, **kwargs)


class RemoteAccessTest(cros_test_lib.MockTempDirTestCase):
  """Base class with RemoteSh mocked out for testing RemoteAccess."""
  def setUp(self):
    self.rsh_mock = self.StartPatcher(RemoteShMock())
    self.host = remote_access.RemoteAccess('foon', self.tempdir)


class RemoteShTest(RemoteAccessTest):
  """Tests of basic RemoteSh functions"""
  TEST_CMD = 'ls'
  RETURN_CODE = 0
  OUTPUT = 'witty'
  ERROR = 'error'

  def assertRemoteShRaises(self, **kwargs):
    """Asserts that RunCommandError is rasied when running TEST_CMD."""
    self.assertRaises(cros_build_lib.RunCommandError, self.host.RemoteSh,
                      self.TEST_CMD, **kwargs)

  def assertRemoteShRaisesSSHConnectionError(self, **kwargs):
    """Asserts that SSHConnectionError is rasied when running TEST_CMD."""
    self.assertRaises(remote_access.SSHConnectionError, self.host.RemoteSh,
                      self.TEST_CMD, **kwargs)

  def SetRemoteShResult(self, returncode=RETURN_CODE, output=OUTPUT,
                        error=ERROR):
    """Sets the RemoteSh command results."""
    self.rsh_mock.AddCmdResult(self.TEST_CMD, returncode=returncode,
                               output=output, error=error)

  def testNormal(self):
    """Test normal functionality."""
    self.SetRemoteShResult()
    result = self.host.RemoteSh(self.TEST_CMD)
    self.assertEquals(result.returncode, self.RETURN_CODE)
    self.assertEquals(result.output.strip(), self.OUTPUT)
    self.assertEquals(result.error.strip(), self.ERROR)

  def testRemoteCmdFailure(self):
    """Test failure in remote cmd."""
    self.SetRemoteShResult(returncode=1)
    self.assertRemoteShRaises()
    self.assertRemoteShRaises(ssh_error_ok=True)
    self.host.RemoteSh(self.TEST_CMD, error_code_ok=True)
    self.host.RemoteSh(self.TEST_CMD, ssh_error_ok=True, error_code_ok=True)

  def testSshFailure(self):
    """Test failure in ssh commad."""
    self.SetRemoteShResult(returncode=remote_access.SSH_ERROR_CODE)
    self.assertRemoteShRaisesSSHConnectionError()
    self.assertRemoteShRaisesSSHConnectionError(error_code_ok=True)
    self.host.RemoteSh(self.TEST_CMD, ssh_error_ok=True)
    self.host.RemoteSh(self.TEST_CMD, ssh_error_ok=True, error_code_ok=True)


class CheckIfRebootedTest(RemoteAccessTest):
  """Tests of the _CheckIfRebooted function."""

  def MockCheckReboot(self, returncode):
    self.rsh_mock.AddCmdResult(
        partial_mock.Regex('.*%s.*' % re.escape(remote_access.REBOOT_MARKER)),
        returncode)

  def testSuccess(self):
    """Test the case of successful reboot."""
    self.MockCheckReboot(0)
    self.assertTrue(self.host._CheckIfRebooted())

  def testRemoteFailure(self):
    """Test case of reboot pending."""
    self.MockCheckReboot(1)
    self.assertFalse(self.host._CheckIfRebooted())

  def testSshFailure(self):
    """Test case of connection down."""
    self.MockCheckReboot(remote_access.SSH_ERROR_CODE)
    self.assertFalse(self.host._CheckIfRebooted())

  def testInvalidErrorCode(self):
    """Test case of bad error code returned."""
    self.MockCheckReboot(2)
    self.assertRaises(Exception, self.host._CheckIfRebooted)


if __name__ == '__main__':
  cros_test_lib.main()
