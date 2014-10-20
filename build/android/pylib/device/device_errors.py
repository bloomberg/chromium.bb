# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Exception classes raised by AdbWrapper and DeviceUtils.
"""

class BaseError(Exception):
  """Base exception for all device and command errors."""
  pass


class CommandFailedError(BaseError):
  """Exception for command failures."""

  def __init__(self, msg, device=None):
    super(CommandFailedError, self).__init__(
        '%s%s' % ('(device: %s) ' % device if device else '', msg))


class AdbCommandFailedError(CommandFailedError):
  """Exception for adb command failures."""

  def __init__(self, cmd, msg, device=None):
    super(AdbCommandFailedError, self).__init__(
        'adb command \'%s\' failed with message: \'%s\'' % (' '.join(cmd), msg),
        device=device)


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

