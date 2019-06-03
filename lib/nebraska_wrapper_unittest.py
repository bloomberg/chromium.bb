# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for nebraska_wrapper."""

from __future__ import print_function

import multiprocessing

from chromite.lib import cros_test_lib
from chromite.lib import cros_build_lib
from chromite.lib import nebraska_wrapper
from chromite.lib import remote_access
from chromite.lib import timeout_util

# We access a ton of private members.
# pylint: disable=protected-access

class RemoteNebraskaWrapperTest(cros_test_lib.MockTestCase):
  """A class for testing RemoteNebraskaWrapper."""

  def setUp(self):
    """Sets up the objects needed for testing."""
    remote_device_mock = self.PatchObject(remote_access, 'RemoteDevice')
    self._nebraska = nebraska_wrapper.RemoteNebraskaWrapper(remote_device_mock)

  def _PatchRemoteCommand(self, return_code=0, output=None, side_effect=None):
    """Creates a RunCommand mock.

    Args:
      return_code: Look at cros_build_lib.RunCommand.
      output: Look at cros_build_lib.RunCommand.
      side_effect: Lookt at mock.side_effect.
    """
    return self.PatchObject(
        nebraska_wrapper.RemoteNebraskaWrapper, '_RemoteCommand',
        return_value=cros_build_lib.CommandResult(returncode=return_code,
                                                  output=output),
        side_effect=side_effect)

  def testIsReady(self):
    """Tests IsReady."""
    # If the running thread is not started, an exception should be raised.
    with self.assertRaises(nebraska_wrapper.NebraskaStartupError):
      self._nebraska.IsReady()

    # If the health check call returns failure, it is not ready.
    self._PatchRemoteCommand(return_code=1)
    self.PatchObject(multiprocessing.Process, 'is_alive', return_value=True)
    self._nebraska._port = 10
    self.assertFalse(self._nebraska.IsReady())

    # The success case.
    run_command_mock = self._PatchRemoteCommand()
    self.assertTrue(self._nebraska.IsReady())
    run_command_mock.assert_called_once_with(
        ['curl', 'http://127.0.0.1:10/check_health', '-o', '/dev/null'],
        error_code_ok=True)

  def test_ReadPortNumber(self):
    """Tests ReadPortNumber."""
    # If the running thread is not started, we can't get the port file.
    with self.assertRaises(nebraska_wrapper.NebraskaStartupError):
      self._nebraska._ReadPortNumber()

    # Tests that after retries the port file didn't come to fruitation.
    self.PatchObject(multiprocessing.Process, 'is_alive', return_value=True)
    self.PatchObject(timeout_util, 'WaitForReturnTrue',
                     side_effect=timeout_util.TimeoutError)
    self.PatchObject(multiprocessing.Process, 'terminate')
    with self.assertRaises(nebraska_wrapper.NebraskaStartupError):
      self._nebraska._ReadPortNumber()

    # The success case.
    self.PatchObject(timeout_util, 'WaitForReturnTrue')
    run_command_mock = self._PatchRemoteCommand(output='10')
    self._nebraska._ReadPortNumber()
    run_command_mock.assert_called_once_with(['cat', '/run/nebraska/port'],
                                             capture_output=True)
    self.assertEqual(self._nebraska._port, 10)

  def test_PortFileExists(self):
    """Tests _PortFileExists."""

    # Now check the correct command is run to get the port.
    run_command_mock = self._PatchRemoteCommand()
    self.assertTrue(self._nebraska._PortFileExists())
    run_command_mock.assert_called_once_with(
        ['test', '-f', '/run/nebraska/port'],
        error_code_ok=True)

    # Failure to run the command case.
    run_command_mock = self._PatchRemoteCommand(return_code=1)
    self.assertFalse(self._nebraska._PortFileExists())

  def test_WaitUntilStarted(self):
    """Tests _WaitUntilStarted."""
    read_port_number_mock = self.PatchObject(
        nebraska_wrapper.RemoteNebraskaWrapper, '_ReadPortNumber')

    # It should fail if we couldn't read the PID file.
    self.PatchObject(timeout_util, 'WaitForReturnTrue',
                     side_effect=timeout_util.TimeoutError)
    with self.assertRaises(nebraska_wrapper.NebraskaStartupError):
      self._nebraska._WaitUntilStarted()

    # The success case.
    self.PatchObject(timeout_util, 'WaitForReturnTrue')
    run_command_mock = self._PatchRemoteCommand(output='10')
    self._nebraska._WaitUntilStarted()
    read_port_number_mock.assert_called_once()
    self.assertEqual(self._nebraska._pid, 10)
    run_command_mock.assert_called_once_with(['cat', '/run/nebraska/pid'],
                                             capture_output=True)

  def testPrintLog(self):
    """Tests PrintLog."""
    run_command_mock = self._PatchRemoteCommand(output='helloworld',
                                                return_code=1)
    log = self._nebraska.PrintLog()
    run_command_mock.assert_called_once_with(
        ['test', '-f', '/tmp/nebraska.log'], error_code_ok=True)
    self.assertIsNone(log)

    run_command_mock = self._PatchRemoteCommand(output='blah blah')
    log = self._nebraska.PrintLog()
    self.assertTrue('blah blah' in log)

  def testCollectRequestLogs(self):
    """Tests CollectRequestLogs."""
    self.PatchObject(multiprocessing.Process, 'is_alive', return_value=True)
    run_command_mock = self._PatchRemoteCommand()
    self._nebraska._port = 10

    # The success case.
    self._nebraska.CollectRequestLogs('/path/to/file')
    run_command_mock.assert_called_once_with(
        ['curl', 'http://127.0.0.1:10/requestlog', '-o',
         nebraska_wrapper.RemoteNebraskaWrapper.REQUEST_LOG_FILE_PATH])

  def testStart(self):
    """Tests Start."""
    # Since the run() function runs in a different thread, its exception doesn't
    # get raised on this thread, so we call the run() directly instead of
    # Start().
    run_command_mock = self._PatchRemoteCommand()
    with self.assertRaises(nebraska_wrapper.NebraskaStartupError):
      self._nebraska.run()

    self._nebraska._update_metadata_dir = '/path/to/dir'
    self._nebraska.run()
    run_command_mock.assert_called_once_with(
        ['python', nebraska_wrapper.RemoteNebraskaWrapper.NEBRASKA_PATH,
         '--update-metadata', '/path/to/dir'],
        redirect_stdout=True, combine_stdout_stderr=True)

  def testGetURL(self):
    """Tests different configurations of the GetURL function."""
    self._nebraska._port = 10
    self.assertEqual(self._nebraska.GetURL(ip='ip'), 'http://ip:10/update/')
    self.assertEqual(self._nebraska.GetURL(critical_update=True),
                     'http://127.0.0.1:10/update/?critical_update=True')
    self._nebraska._port = 11
    self.assertEqual(self._nebraska.GetURL(no_update=True),
                     'http://127.0.0.1:11/update/?no_update=True')
