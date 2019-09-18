# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Device-related helper functions/classes."""

from __future__ import print_function

import argparse
import os

from chromite.cli.cros import cros_chrome_sdk
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import remote_access
from chromite.lib import retry_util


class DeviceError(Exception):
  """Exception for Device failures."""

  def __init__(self, message):
    super(DeviceError, self).__init__()
    logging.error(message)


class Device(object):
  """Class for managing a test device."""

  def __init__(self, opts):
    """Initialize Device.

    Args:
      opts: command line options.
    """
    self.device = opts.device
    self.ssh_port = None
    self.board = opts.board

    self.use_sudo = False
    self.cmd = opts.args[1:] if opts.cmd else None
    self.private_key = opts.private_key
    self.dry_run = opts.dry_run
    # log_level is only set if --log-level or --debug is specified.
    self.log_level = getattr(opts, 'log_level', None)
    self.InitRemote()

  def InitRemote(self):
    """Initialize remote access."""
    self.remote = remote_access.RemoteDevice(
        self.device,
        port=self.ssh_port,
        connect_settings=self._ConnectSettings(),
        private_key=self.private_key)

    self.device_addr = 'ssh://%s' % self.device
    if self.ssh_port:
      self.device_addr += ':%d' % self.ssh_port

  def WaitForBoot(self, sleep=5):
    """Wait for the device to boot up.

    Wait for the ssh connection to become active.
    """
    try:
      result = retry_util.RetryException(
          exception=remote_access.SSHConnectionError,
          max_retry=10,
          functor=lambda: self.RemoteCommand(cmd=['true']),
          sleep=sleep)
    except remote_access.SSHConnectionError:
      raise DeviceError(
          'WaitForBoot timed out trying to connect to the device.')

    if result.returncode != 0:
      raise DeviceError('WaitForBoot failed: %s.' % result.error)

  def RunCommand(self, cmd, **kwargs):
    """Use SudoRunCommand or RunCommand as necessary.

    Args:
      cmd: command to run.
      kwargs: optional args to RunCommand.

    Returns:
      cros_build_lib.CommandResult object.
    """
    if self.dry_run:
      return self._DryRunCommand(cmd)
    elif self.use_sudo:
      return cros_build_lib.SudoRunCommand(cmd, **kwargs)
    else:
      return cros_build_lib.RunCommand(cmd, **kwargs)

  def RemoteCommand(self, cmd, stream_output=False, **kwargs):
    """Run a remote command.

    Args:
      cmd: command to run.
      stream_output: Stream output of long-running commands.
      kwargs: additional args (see documentation for RemoteDevice.RunCommand).

    Returns:
      cros_build_lib.CommandResult object.
    """
    if self.dry_run:
      return self._DryRunCommand(cmd)
    else:
      kwargs.setdefault('error_code_ok', True)
      if stream_output:
        kwargs.setdefault('capture_output', False)
      else:
        kwargs.setdefault('combine_stdout_stderr', True)
        kwargs.setdefault('log_output', True)
      return self.remote.RunCommand(cmd, debug_level=logging.INFO, **kwargs)

  def _DryRunCommand(self, cmd):
    """Print a command for dry_run.

    Args:
      cmd: command to print.

    Returns:
      cros_build_lib.CommandResult object.
    """
    assert self.dry_run, 'Use with --dry-run only'
    logging.info('[DRY RUN] %s', cros_build_lib.CmdToStr(cmd))
    return cros_build_lib.CommandResult(cmd, output='', returncode=0)

  def _ConnectSettings(self):
    """Increase ServerAliveCountMax and ServerAliveInterval.

    Wait 2 min before dropping the SSH connection.
    """
    return remote_access.CompileSSHConnectSettings(
        ServerAliveInterval=15, ServerAliveCountMax=8)

  @property
  def is_vm(self):
    """Returns true if we're a VM."""
    return self._IsVM(self.device)

  @staticmethod
  def _IsVM(device):
    """VM if |device| is specified and it's not localhost."""
    return not device or device == remote_access.LOCALHOST

  @staticmethod
  def Create(opts):
    """Create either a Device or VM based on |opts.device|."""
    if Device._IsVM(opts.device):
      from chromite.lib import vm

      return vm.VM(opts)
    return Device(opts)

  @staticmethod
  def GetParser():
    """Parse a list of args.

    Args:
      argv: list of command line arguments.

    Returns:
      List of parsed opts.
    """
    parser = commandline.ArgumentParser(description=__doc__)
    parser.add_argument('--device', help='Hostname or Device IP.')
    sdk_board_env = os.environ.get(cros_chrome_sdk.SDKFetcher.SDK_BOARD_ENV)
    parser.add_argument('--board', default=sdk_board_env, help='Board to use.')
    parser.add_argument('--private-key', help='Path to ssh private key.')
    parser.add_argument('--dry-run', action='store_true', default=False,
                        help='dry run for debugging.')
    parser.add_argument('--cmd', action='store_true', default=False,
                        help='Run a command.')
    parser.add_argument('args', nargs=argparse.REMAINDER,
                        help='Command to run.')
    return parser
