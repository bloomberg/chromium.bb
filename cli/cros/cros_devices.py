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

  Examples:

    $ cros devices
    List information of USB-connected Brillo devices.

    $ cros devices alias toaster1
    Set the device's alias to "toaster1".

    $ cros devices full-reset
    Reset the device to a fresh image for the current SDK version.
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

  @classmethod
  def AddParser(cls, parser):
    """Add parser arguments."""
    super(cls, DevicesCommand).AddParser(parser)
    # Device argument is always optional for the `devices` tool.
    cls.AddDeviceArgument(parser, optional=True)
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

    self.device = self._FindDevice(self.options.device)

  def _FindDevice(self, device):
    """Find the Brillo device based on the IP address or the alias name.

    Args:
      device: A string which can be the device's IP address or alias name.

    Returns:
      A BrilloDevice object of the device associated with the IP address or
      alias name. None if |device| is None or cannot find the device.
    """
    if not device:
      return None
    # TODO(yimingc): Find the USB-connected device.
    return None

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

  def _SetAlias(self):
    """Set a user-friendly alias name to the Brillo device."""
    # TODO(yimingc): Set the device alias to self.arg (if not None).
    logging.info('Device 1.1.1.1 now has alias "%s"', self.alias_name)

  def _FullReset(self):
    """Perform a full reset for the device."""
    logging.info('Ready to full reset the device.')
    # TODO(yimingc): Implement the full reset functionality.

  def Run(self):
    """Run cros devices."""
    self.options.Freeze()
    self._ReadOptions()

    if self.cmd == self.ALIAS_CMD:
      self._SetAlias()
    elif self.cmd == self.FULL_RESET_CMD:
      self._FullReset()
    else:
      self._ListDevices()
