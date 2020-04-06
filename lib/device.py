# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Device-related helper functions/classes."""

from __future__ import print_function

import argparse
import os
import subprocess
import sys

from chromite.cli.cros import cros_chrome_sdk
from chromite.lib import commandline
from chromite.lib import cros_logging as logging
from chromite.lib import remote_access
from chromite.lib import retry_util


assert sys.version_info >= (3, 6), 'This module requires Python 3.6+'


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
    self.device = opts.device.hostname if opts.device else None
    self.ssh_port = opts.device.port if opts.device else None
    self.should_start_vm = not self.device
    self.board = opts.board

    self.use_sudo = False
    self.cmd = opts.args[1:] if opts.cmd else None
    self.private_key = opts.private_key
    self.dryrun = opts.dryrun
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

  def WaitForBoot(self, max_retry=10, sleep=5):
    """Wait for the device to boot up.

    Wait for the ssh connection to become active.
    """
    try:
      result = retry_util.RetryException(
          exception=remote_access.SSHConnectionError,
          max_retry=max_retry,
          functor=lambda: self.remote_run(cmd=['true']),
          sleep=sleep)
    except remote_access.SSHConnectionError:
      raise DeviceError(
          'WaitForBoot timed out trying to connect to the device.')

    if result.returncode != 0:
      raise DeviceError('WaitForBoot failed: %s.' % result.error)

  def remote_run(self, cmd, stream_output=False, **kwargs):
    """Run a remote command.

    Args:
      cmd: command to run.
      stream_output: Stream output of long-running commands.
      kwargs: additional args (see documentation for RemoteDevice.run).

    Returns:
      cros_build_lib.CommandResult object.
    """
    kwargs.setdefault('check', False)
    if stream_output:
      kwargs.setdefault('capture_output', False)
    else:
      kwargs.setdefault('stderr', subprocess.STDOUT)
      kwargs.setdefault('log_output', True)
    return self.remote.run(cmd, dryrun=self.dryrun,
                           debug_level=logging.INFO, **kwargs)

  def _ConnectSettings(self):
    """Increase ServerAliveCountMax and ServerAliveInterval.

    Wait 2 min before dropping the SSH connection.
    """
    return remote_access.CompileSSHConnectSettings(
        ServerAliveInterval=15, ServerAliveCountMax=8)

  @staticmethod
  def Create(opts):
    """Create either a Device or VM based on |opts.device|."""
    if not opts.device:
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
    parser.add_argument(
        '-d', '--device',
        type=commandline.DeviceParser(commandline.DEVICE_SCHEME_SSH),
        help='Hostname or device IP in format hostname[:port]. If not '
             'specified, a VM will be launched for the duration of the test.')
    sdk_board_env = os.environ.get(cros_chrome_sdk.SDKFetcher.SDK_BOARD_ENV)
    parser.add_argument('--board', default=sdk_board_env, help='Board to use.')
    parser.add_argument('--private-key', help='Path to ssh private key.')
    parser.add_argument('--dry-run', dest='dryrun', action='store_true',
                        default=False, help='dry run for debugging.')
    parser.add_argument('--cmd', action='store_true', default=False,
                        help='Run a command.')
    parser.add_argument('args', nargs=argparse.REMAINDER,
                        help='Command to run.')
    return parser
