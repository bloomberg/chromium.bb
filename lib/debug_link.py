# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module that initializes the Debug Link."""

from __future__ import print_function

import os
import time

from chromite.lib import cros_build_lib
from chromite.lib import osutils
from chromite.lib import timeout_util


_HOST_IP = '169.254.100.1'
# TODO(thieule): Change debug link VID/PID to actual values once we have
# working hardware (brbug.com/444).
_VENDOR_ID = '0101'
_PRODUCT_ID = '0202'
_PROPERTY_VENDOR_ID = 'ID_VENDOR_ID'
_PROPERTY_PRODUCT_ID = 'ID_MODEL_ID'

_IFNAME_PREFIX = 'usb'
_MAX_SUFFIX = 99


class DebugLinkException(Exception):
  """Base exception for this module."""


class DebugLinkMissingError(DebugLinkException):
  """Error indicating that there is no Debug Link."""


class DebugLinkInitializationFailedError(DebugLinkException):
  """Error indicating failure to initialize Debug Link."""


class InterfaceNameExistsError(DebugLinkException):
  """Error indicating that an interface name already exists."""


class NetworkInterface(object):
  """Object to manipulate network interface and query its properties."""
  # pylint: disable=interface-not-implemented

  _IF_UP = 'up'
  _SYSFS_NET = '/sys/class/net'
  _UP_STATE_STABLE_TIME = 0.5

  def __init__(self, ifname):
    """Initializes this object.

    Args:
      ifname: Name of interface (eg. 'eth0').
    """
    self._ifname = ifname
    self._device_properties = None
    self._mac_address = None

  @property
  def ifname(self):
    """Returns the interface name."""
    return self._ifname

  @property
  def _sysfs_path(self):
    """Returns the full sysfs path for this interface."""
    return os.path.join(self._SYSFS_NET, self._ifname)

  def GetDeviceProperties(self):
    """Returns the interface device properties as a dictionary."""
    if not self._device_properties:
      path = self._sysfs_path
      cmd = ['udevadm', 'info', '--query=property', '--path=%s' % path]
      props = cros_build_lib.RunCommand(
          cmd,
          error_message='Failed to query device %s properties' % path,
          capture_output=True).output.splitlines()

      self._device_properties = {}
      udev_props = dict(prop.split('=', 1) for prop in props)
      for prop_name in [_PROPERTY_VENDOR_ID, _PROPERTY_PRODUCT_ID]:
        self._device_properties[prop_name] = udev_props.get(prop_name)

    return self._device_properties

  def GetMacAddress(self):
    """Returns the MAC address of this interface."""
    if not self._mac_address:
      self._mac_address = osutils.ReadFile(
          os.path.join(self._sysfs_path, 'address')).rstrip()
    return self._mac_address

  def IsUp(self):
    """Returns True if this interface is up."""
    return osutils.ReadFile(
        os.path.join(self._sysfs_path, 'operstate')).rstrip() == self._IF_UP

  def HasIPAddress(self, ip):
    """Checks to see if the interface has the specified IP address.

    Args:
      ip: IP address to check.

    Returns:
      True if the interface has |ip|, else False.
    """
    return cros_build_lib.GetIPv4Address(self.ifname) == ip

  def BringDown(self):
    """Takes interface down."""
    cros_build_lib.SudoRunCommand(['ip', 'link', 'set', self.ifname, 'down'])

  def BringUp(self, ip):
    """Brings interface up.

    This method waits until the interface is up before returning.

    Args:
      ip: IP address to assign to interface.

    Raises:
      DebugLinkInitializationFailedError if the interface fails to come online.
    """
    if not self.HasIPAddress(ip):
      cros_build_lib.SudoRunCommand(
          ['ip', 'address', 'add', '%s/24' % ip, 'dev', self.ifname])

    if not self.IsUp():
      cros_build_lib.SudoRunCommand(
          ['ip', 'link', 'set', 'up', 'dev', self.ifname])

      # During development, we're using Ethernet dongles in place of the
      # Roll board.  Some Ethernet dongles momentarily bring down the interface
      # during the bring up process.  Make sure the interface stabilizes in the
      # 'up' state for |up_state_stable_time_seconds| before returning.
      # TODO(thieule): When Roll board is available, simplify this to just
      # waiting for the simple 'up' state transition (brbug.com/490).
      up_state_start_time = [None]

      def _CheckForInterfaceUp():
        if not self.IsUp():
          up_state_start_time[0] = None
        elif not up_state_start_time[0]:
          up_state_start_time[0] = time.time()
        if not up_state_start_time[0]:
          return False
        up_time = time.time() - up_state_start_time[0]
        return up_time >= self._UP_STATE_STABLE_TIME

      try:
        timeout_util.WaitForReturnTrue(_CheckForInterfaceUp, 10, period=0.1)
      except timeout_util.TimeoutError:
        raise DebugLinkInitializationFailedError(
            'Failed to bring up interface %s' % self.ifname)

  def Rename(self, new_ifname):
    """Renames interface to |new_ifname|.

    Args:
      new_ifname: New interface name.

    Raises:
      InterfaceNameExistsError if the new name is in use.
    """
    try:
      cros_build_lib.SudoRunCommand(
          ['nameif', new_ifname, self.GetMacAddress()])
      self._ifname = new_ifname
    except cros_build_lib.RunCommandError as e:
      if 'File exists' in str(e):
        raise InterfaceNameExistsError(
            'Rename failed, interface name %s exists' % new_ifname)
      raise

  @classmethod
  def GetInterfaces(cls):
    """Returns a list of all NetworkInterface in the system."""
    return [NetworkInterface(name) for name in os.listdir(cls._SYSFS_NET)]


def _ConfigureDebugLink(interface):
  """Renames interface to usb*, assigns IP address and brings it up.

  Args:
    interface: Network interface to configure.

  Returns:
    IP address of |interface|.

  Raises:
    DebugLinkInitializationFailedError if no usb* name is available.
  """

  if not interface.ifname.startswith(_IFNAME_PREFIX):
    interface.BringDown()
    for index in range(_MAX_SUFFIX + 1):
      try:
        interface.Rename('%s%d' % (_IFNAME_PREFIX, index))
        break
      except InterfaceNameExistsError:
        if index == _MAX_SUFFIX:
          raise DebugLinkInitializationFailedError(
              'No more %s* names available' % _IFNAME_PREFIX)

  interface.BringUp(_HOST_IP)
  return _HOST_IP


def InitializeDebugLink():
  """Initializes the Debug Link interface.

  Only one Debug Link is supported at this time.  This function finds the
  first Debug Link, assigns a fixed IP address and brings it up.

  Returns:
    IP address of the interface used as the Debug Link.

  Raises:
    DebugLinkMissingError if no Debug Link is found.
    DebugLinkInitializationFailedError if the interface fails to come online.
  """
  interfaces = NetworkInterface.GetInterfaces()
  for interface in interfaces:
    props = interface.GetDeviceProperties()
    vendor_id = props.get(_PROPERTY_VENDOR_ID, '')
    product_id = props.get(_PROPERTY_PRODUCT_ID, '')
    if vendor_id == _VENDOR_ID and product_id == _PRODUCT_ID:
      return _ConfigureDebugLink(interface)

  raise DebugLinkMissingError(
      'Debug Link missing, cannot find USB device with VID=%s, PID=%s' %
      (_VENDOR_ID, _PRODUCT_ID))
