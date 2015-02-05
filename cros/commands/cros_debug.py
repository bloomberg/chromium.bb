# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""cros debug: Debug the applications on the target device."""

from __future__ import print_function

import os
import logging
import urlparse

from chromite import cros
from chromite.lib import cros_build_lib
from chromite.lib import remote_access


@cros.CommandDecorator('debug')
class DebugCommand(cros.CrosCommand):
  """Use GDB to debug a process running on the target device.

  This command starts a GDB session to debug a remote process running on the
  target device. The remote process can either be an existing process or newly
  started by calling this command.

  This command can also be used to find out information about all running
  processes of an executable on the target device.
  """

  # Override base class property to enable stats upload.
  upload_stats = True

  def __init__(self, options):
    """Initialize DebugCommand."""
    cros.CrosCommand.__init__(self, options)
    # SSH connection settings.
    self.ssh_hostname = None
    self.ssh_port = None
    self.ssh_username = None
    self.ssh_private_key = None
    # The board name of the target device.
    self.board = None
    # Settings of the process to debug.
    self.attach = False
    self.list = False
    self.exe = None
    self.pid = None

  @classmethod
  def AddParser(cls, parser):
    """Add parser arguments."""
    super(cls, DebugCommand).AddParser(parser)
    parser.add_argument(
        'device', help='IP[:port] address of the target device.')
    parser.add_argument(
        '--board', default=None, help='The board to use. By default it is '
        'automatically detected. You can override the detected board with '
        'this option.')
    parser.add_argument(
        '--private-key', type='path', default=None,
        help='SSH identity file (private key).')
    parser.add_argument(
        '--attach', action='store_true', default=False,
        help='Attach GDB to an already running process on the target device.')
    parser.add_argument(
        '-l', '--list', action='store_true', default=False,
        help='List running processes of the executable on the target device.')
    parser.add_argument(
        '--exe', help='Full path of the executable on the target device.')
    parser.add_argument(
        '-p', '--pid', type=int,
        help='The pid of the process on the target device.')

  def _GetRunningPids(self, device):
    """Get all the running pids on the device with the executable path."""
    try:
      result = device.BaseRunCommand(['pgrep', '-fx', self.exe])
      try:
        return [int(pid) for pid in result.output.splitlines()]
      except ValueError:
        cros_build_lib.Die('Parsing output failed:\n%s', result.output)
    except cros_build_lib.RunCommandError:
      cros_build_lib.Die(
          'Failed to find any running process of %s on device %s', self.exe,
          self.ssh_hostname)

  def _ListProcesses(self, device, pids):
    """Provided with a list of pids, print out information of the processes."""
    try:
      result = device.BaseRunCommand(['ps', 'aux'])
      lines = result.output.splitlines()
      try:
        header, procs = lines[0], lines[1:]
        info = os.linesep.join([p for p in procs if int(p.split()[1]) in pids])
      except ValueError:
        cros_build_lib.Die('Parsing output failed:\n%s', result.output)

      print('\nList running processes of %s on device %s:\n%s\n%s' %
            (self.exe, self.ssh_hostname, header, info))
    except cros_build_lib.RunCommandError:
      cros_build_lib.Die(
          'Failed to find any running process on device %s', self.ssh_hostname)

  def _DebugNewProcess(self):
    """Start a new process on the target device and attach gdb to it."""
    logging.info(
        'Ready to start and debug %s on device %s', self.exe, self.ssh_hostname)
    cmd = [
        'gdb_remote', '--ssh',
        '--board', self.board,
        '--remote', self.ssh_hostname,
        '--remote_file', self.exe,
    ]
    cros_build_lib.RunCommand(cmd)

  def _DebugRunningProcess(self, pid):
    """Start gdb and attach it to the remote running process with |pid|."""
    logging.info(
        'Ready to debug process %d on device %s', pid, self.ssh_hostname)
    cmd = [
        'gdb_remote', '--ssh',
        '--board', self.board,
        '--remote', self.ssh_hostname,
        '--remote_pid', str(pid),
    ]
    cros_build_lib.RunCommand(cmd)

  def _ReadOptions(self):
    """Process options and set variables."""
    device = self.options.device
    if urlparse.urlparse(device).scheme == '':
      # For backward compatibility, prepend ssh:// ourselves.
      device = 'ssh://%s' % device

    parsed = urlparse.urlparse(device)
    if parsed.scheme == 'ssh':
      self.ssh_hostname = parsed.hostname
      self.ssh_username = parsed.username
      self.ssh_port = parsed.port
      self.ssh_private_key = self.options.private_key
      self.attach = self.options.attach
      self.list = self.options.list
      self.exe = self.options.exe
      self.pid = self.options.pid
    else:
      cros_build_lib.Die('Does not support device %s', self.options.device)

  def Run(self):
    """Run cros debug."""
    self.options.Freeze()
    self._ReadOptions()
    self.RunInsideChroot(auto_detect_project=True)
    try:
      with remote_access.ChromiumOSDeviceHandler(
          self.ssh_hostname, port=self.ssh_port, username=self.ssh_username,
          private_key=self.ssh_private_key) as device:
        self.board = cros_build_lib.GetBoard(device_board=device.board,
                                             override_board=self.options.board)
        logging.info('Board is %s', self.board)

        if self.pid:
          # Ignore other flags and start GDB on the pid if it is provided.
          self._DebugRunningProcess(self.pid)
          return

        if not (self.exe and self.exe.startswith('/')):
          cros_build_lib.Die('--exe is required and must be a full pathname.')
        logging.info('Executable path is %s', self.exe)
        if not device.IsFileExecutable(self.exe):
          cros_build_lib.Die(
              'File path "%s" does not exist or is not executable on device %s',
              self.exe, self.ssh_hostname)

        if self.list:
          # If '--list' flag is on, list the processes without launching GDB.
          pids = self._GetRunningPids(device)
          self._ListProcesses(device, pids)
          return

        if self.attach:
          pids = self._GetRunningPids(device)
          if not pids:
            cros_build_lib.Die('No running process of %s is found on device %s',
                               self.exe, self.ssh_hostname)
          elif len(pids) == 1:
            idx = 0
          else:
            self._ListProcesses(device, pids)
            idx = cros_build_lib.GetChoice(
                'Please select the process pid to debug:', pids)
          self._DebugRunningProcess(pids[idx])
          return

        self._DebugNewProcess()

    except (Exception, KeyboardInterrupt) as e:
      logging.error(e)
      if self.options.debug:
        raise
