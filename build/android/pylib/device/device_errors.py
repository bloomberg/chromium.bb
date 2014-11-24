# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Exception classes raised by AdbWrapper and DeviceUtils.
"""

from pylib import cmd_helper


class BaseError(Exception):
  """Base exception for all device and command errors."""
  pass


class CommandFailedError(BaseError):
  """Exception for command failures."""

  def __init__(self, message, device_serial=None):
    if device_serial is not None:
      message = '(device: %s) %s' % (device_serial, message)
    self.device_serial = device_serial
    super(CommandFailedError, self).__init__(message)


class AdbCommandFailedError(CommandFailedError):
  """Exception for adb command failures."""

  def __init__(self, cmd, output, status=None, device_serial=None):
    self.cmd = cmd
    self.output = output
    self.status = status
    message = []
    if self.cmd[0] == 'shell':
      assert len(self.cmd) == 2
      message.append('adb shell command %r failed with' % self.cmd[1])
    else:
      command = ' '.join(cmd_helper.SingleQuote(arg) for arg in self.cmd)
      message.append('adb command %r failed with' % command)
    if status:
      message.append(' exit status %d and' % self.status)
    if output:
      message.append(' output:\n')
      message.extend('> %s\n' % line for line in output.splitlines())
    else:
      message.append(' no output')
    super(AdbCommandFailedError, self).__init__(''.join(message), device_serial)


class CommandTimeoutError(BaseError):
  """Exception for command timeouts."""
  pass


class DeviceUnreachableError(BaseError):
  """Exception for device unreachable failures."""
  pass


class NoDevicesError(BaseError):
  """Exception for having no devices attached."""

  def __init__(self):
    super(NoDevicesError, self).__init__('No devices attached.')

