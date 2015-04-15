# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from pylib.device import device_blacklist
from pylib.device import device_errors


def DefaultFilters():
  """Returns a list of the most commonly-used device filters.

  These filters match devices that:
    - are in a "device" state (as opposed to, e.g., "unauthorized" or
      "emulator")
    - are not blacklisted.

  Returns:
    A list of the most commonly-used device filters.
  """
  return [DeviceFilter, BlacklistFilter()]


def BlacklistFilter():
  """Returns a filter that matches devices that are not blacklisted.

  Note that this function is not the filter. It creates one when called using
  the blacklist at that time and returns that.

  Returns:
    A filter function that matches devices that are not blacklisted.
  """
  blacklist = set(device_blacklist.ReadBlacklist())
  def f(adb):
    return adb.GetDeviceSerial() not in blacklist

  return f


def DeviceFilter(adb):
  """A filter that matches devices in a "device" state.

  (Basically, this is adb get-state == "device")

  Args:
    adb: An instance of AdbWrapper.
  Returns:
    True if the device is in a "device" state.
  """
  try:
    return adb.GetState() == 'device'
  except device_errors.CommandFailedError:
    return False

