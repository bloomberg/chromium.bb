# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module tests the cros debug command."""

from __future__ import print_function

import mock

from chromite.cros.commands import cros_debug
from chromite.cros.commands import init_unittest
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import remote_access


class MockDebugCommand(init_unittest.MockCommand):
  """Mock out the debug command."""
  TARGET = 'chromite.cros.commands.cros_debug.DebugCommand'
  TARGET_CLASS = cros_debug.DebugCommand
  COMMAND = 'debug'
  ATTRS = ('_ListProcesses', '_DebugNewProcess', '_DebugRunningProcess')

  def __init__(self, *args, **kwargs):
    init_unittest.MockCommand.__init__(self, *args, **kwargs)

  def _ListProcesses(self, _inst, *_args, **_kwargs):
    """Mock out _ListProcesses."""

  def _DebugNewProcess(self, _inst, *_args, **_kwargs):
    """Mock out _DebugNewProcess."""

  def _DebugRunningProcess(self, _inst, *_args, **_kwargs):
    """Mock out _DebugRunningProcess."""

  def Run(self, inst):
    init_unittest.MockCommand.Run(self, inst)


class DebugRunThroughTest(cros_test_lib.MockTempDirTestCase):
  """Test the flow of DebugCommand.run with the debug methods mocked out."""

  DEVICE = '1.1.1.1'
  EXE = '/path/to/exe'
  PID = '1'

  def SetupCommandMock(self, cmd_args):
    """Set up command mock."""
    self.cmd_mock = MockDebugCommand(
        cmd_args, base_args=['--cache-dir', self.tempdir])
    self.StartPatcher(self.cmd_mock)

  def setUp(self):
    """Patches objects."""
    self.cmd_mock = None
    self.PatchObject(remote_access, 'ChromiumOSDevice')

  def testMissingExecutable(self):
    """Test that command fails when executable is not provided."""
    self.SetupCommandMock([self.DEVICE])
    self.assertRaises(cros_build_lib.DieSystemExit, self.cmd_mock.inst.Run)

  def testDebugProcessWithPid(self):
    """Test that methods are called correctly when pid is provided."""
    self.SetupCommandMock([self.DEVICE, '--pid', self.PID])
    self.cmd_mock.inst.Run()
    self.assertFalse(self.cmd_mock.patched['_ListProcesses'].called)
    self.assertFalse(self.cmd_mock.patched['_DebugNewProcess'].called)
    self.assertTrue(self.cmd_mock.patched['_DebugRunningProcess'].called)

  def testListProcesses(self):
    """Test that methods are called correctly for listing processes."""
    self.SetupCommandMock([self.DEVICE, '--exe', self.EXE, '--list'])
    self.cmd_mock.inst.Run()
    self.assertTrue(self.cmd_mock.patched['_ListProcesses'].called)
    self.assertFalse(self.cmd_mock.patched['_DebugNewProcess'].called)
    self.assertFalse(self.cmd_mock.patched['_DebugRunningProcess'].called)

  def testDebugNewProcess(self):
    """Test that methods are called correctly for debugging a new process."""
    self.SetupCommandMock([self.DEVICE, '--exe', self.EXE])
    self.cmd_mock.inst.Run()
    self.assertFalse(self.cmd_mock.patched['_ListProcesses'].called)
    self.assertTrue(self.cmd_mock.patched['_DebugNewProcess'].called)
    self.assertFalse(self.cmd_mock.patched['_DebugRunningProcess'].called)

  def testDebugWithNoRunningProcess(self):
    """Test that command fails when trying to attach a not running program."""
    self.SetupCommandMock([self.DEVICE, '--exe', self.EXE, '--attach'])
    with mock.patch.object(cros_debug.DebugCommand, '_GetRunningPids',
                           return_value=[]):
      self.assertRaises(cros_build_lib.DieSystemExit, self.cmd_mock.inst.Run)

  def testDebugWithOneRunningProcess(self):
    """Test that methods are called correctly for attaching a single process."""
    self.SetupCommandMock([self.DEVICE, '--exe', self.EXE, '--attach'])
    with mock.patch.object(cros_debug.DebugCommand, '_GetRunningPids',
                           return_value=['1']):
      self.cmd_mock.inst.Run()
      self.assertFalse(self.cmd_mock.patched['_ListProcesses'].called)
      self.assertFalse(self.cmd_mock.patched['_DebugNewProcess'].called)
      self.assertTrue(self.cmd_mock.patched['_DebugRunningProcess'].called)

  def testDebugWithMultipleRunningProcesses(self):
    """Test that methods are called correctly with multiple processes found."""
    self.SetupCommandMock([self.DEVICE, '--exe', self.EXE, '--attach'])
    with mock.patch.object(cros_debug.DebugCommand, '_GetRunningPids',
                           return_value=['1', '2']):
      with mock.patch.object(cros_build_lib, 'GetChoice') as mock_prompt:
        self.cmd_mock.inst.Run()
        self.assertTrue(self.cmd_mock.patched['_ListProcesses'].called)
        self.assertFalse(self.cmd_mock.patched['_DebugNewProcess'].called)
        self.assertTrue(mock_prompt.called)
        self.assertTrue(self.cmd_mock.patched['_DebugRunningProcess'].called)
