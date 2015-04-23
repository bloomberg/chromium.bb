# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""cros devices: List and configure Brillo devices connected over USB."""

from __future__ import print_function

from chromite.cli import command
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import remote_access


@command.CommandDecorator('devices')
class DevicesCommand(command.CliCommand):
  """List and configure Brillo devices connected over USB.

  This command takes an optional subcommand: "alias" or "full-reset". If no
  subcommand is given, this command will print out information of USB-connected
  Brillo devices. No other subcommand is supported.

  When multiple Brillo devices are connected over USB, "--device" flag can be
  used to specify which device is targeted.
  """

  EPILOG = """
To list information of USB-connected devices:
  cros devices

To set the device's alias to "toaster1":
  cros devices alias toaster1

To reset the device to a fresh image for the current SDK version:
  cros devices full-reset
"""

  # Override base class property to enable stats upload.
  upload_stats = True

  ALIAS_CMD = 'alias'
  FULL_RESET_CMD = 'full-reset'

  def __init__(self, options):
    """Initialize DevicesCommand."""
    super(DevicesCommand, self).__init__(options)
    self.cmd = None
    self.alias_name = None
    self.device = None
    # SSH connection settings.
    self.ssh_hostname = None
    self.ssh_port = None
    self.ssh_username = None
    self.ssh_private_key = None

  @classmethod
  def AddParser(cls, parser):
    """Add parser arguments."""
    super(cls, DevicesCommand).AddParser(parser)
    # Device argument is always optional for the `devices` tool.
    cls.AddDeviceArgument(parser, optional=True)
    parser.add_argument(
        '--private-key', type='path', default=None,
        help='SSH identify file (private key).')
    parser.add_argument(
        'subcommand', nargs='?',
        help='Optional subcommand ("alias" or "full-reset").')
    parser.add_argument(
        'alias_name', nargs='?',
        help='The user-friendly alias name for the device. This is needed only '
        'when "alias" subcommand is used.')

  def _ReadOptions(self):
    """Process options and set variables."""
    self.cmd = self.options.subcommand
    if self.cmd and self.cmd not in (self.ALIAS_CMD, self.FULL_RESET_CMD):
      cros_build_lib.Die('Invalid subcommand: %s', self.cmd)

    self.alias_name = self.options.alias_name
    if self.cmd == self.FULL_RESET_CMD and self.alias_name:
      cros_build_lib.Die('Extra argument: %s', self.alias_name)
    if self.cmd == self.ALIAS_CMD and not self.alias_name:
      cros_build_lib.Die('Missing alias argument')

    if self.options.device:
      self.ssh_hostname = self.options.device.hostname
      self.ssh_username = self.options.device.username
      self.ssh_port = self.options.device.port
    self.ssh_private_key = self.options.private_key

  def _PrintDeviceInfo(self, devices):
    """Print out information about devices."""
    rows = [('Alias Name', 'IP Address')]
    rows.extend((device.alias, device.hostname) for device in devices)
    # Number of offset spaces for the second column. Get the max length of items
    # in the first column, and add 4 extra spaces to make a clear table view.
    offset = max(len(row[0]) for row in rows) + 4
    for row in rows:
      print('%-*s%s' % (offset, row[0], row[1]))

  def _ListDevices(self):
    """List all USB-connected Brillo devices."""
    devices = remote_access.GetUSBConnectedDevices()
    if devices:
      print('Info of all USB-connected Brillo devices:')
      self._PrintDeviceInfo(devices)
    else:
      print('No USB-connected Brillo devices are found.')

  def _FullReset(self):
    """Perform a full reset for the device."""
    logging.info('Ready to full reset the device.')
    # TODO(yimingc): Implement the full reset functionality.

  def Run(self):
    """Run cros devices."""
    self.options.Freeze()
    self._ReadOptions()

    try:
      if self.cmd == self.ALIAS_CMD:
        try:
          with remote_access.ChromiumOSDeviceHandler(
              self.ssh_hostname, port=self.ssh_port, username=self.ssh_username,
              private_key=self.ssh_private_key) as device:
            device.SetAlias(self.alias_name)
        except remote_access.InvalidDevicePropertyError as e:
          cros_build_lib.Die('The alias provided is invalid: %s', e)
      elif self.cmd == self.FULL_RESET_CMD:
        self._FullReset()
      else:
        if self.options.device:
          with remote_access.ChromiumOSDeviceHandler(
              self.ssh_hostname, port=self.ssh_port, username=self.ssh_username,
              private_key=self.ssh_private_key) as device:
            self._PrintDeviceInfo([device])
        else:
          self._ListDevices()
    except (Exception, KeyboardInterrupt) as e:
      cros_build_lib.Die('Command failed: %s', e)
