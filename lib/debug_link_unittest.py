# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for the debug_link.py module."""

from __future__ import print_function

import mock

from chromite.lib import cros_build_lib_unittest
from chromite.lib import cros_test_lib
from chromite.lib import debug_link
from chromite.lib import partial_mock


# pylint: disable=protected-access


_IFNAME0 = '%s0' % debug_link._IFNAME_PREFIX
_IFNAME1 = '%s1' % debug_link._IFNAME_PREFIX


class NetworkInterfaceTest(cros_build_lib_unittest.RunCommandTestCase):
  """Tests for NetworkInterface."""

  def testDevicePropertiesParse(self):
    """Test for correct parsing of udevadm results in GetDeviceProperties()."""
    self.rc.AddCmdResult(
        partial_mock.In('udevadm'),
        output='%s=1111\n%s=2222\nignore=value' %
        (debug_link._PROPERTY_VENDOR_ID, debug_link._PROPERTY_PRODUCT_ID))
    results = debug_link.NetworkInterface('').GetDeviceProperties()
    self.assertEqual(results[debug_link._PROPERTY_VENDOR_ID], '1111')
    self.assertEqual(results[debug_link._PROPERTY_PRODUCT_ID], '2222')
    self.assertEqual(results.get('ignore'), None)


class NetworkInterfaceMock(partial_mock.PartialMock):
  """Mocks debug_link.NetworkInterface."""

  TARGET = 'chromite.lib.debug_link.NetworkInterface'
  ATTRS = ('BringDown', 'BringUp', 'Rename')

  def BringDown(self, _inst):
    pass

  def BringUp(self, _inst, *_args):
    pass

  def Rename(self, _inst, *_args):
    pass


class ConfigureDebugLinkRenameTest(cros_test_lib.MockTestCase):
  """Tests for _ConfigureDebugLink() interface renaming."""

  def setUp(self):
    self.StartPatcher(NetworkInterfaceMock())

  def testCollision(self):
    """Test that rename collisions are handled."""
    def _RenameSideEffect(new_ifname):
      if new_ifname == _IFNAME0:
        raise debug_link.InterfaceNameExistsError()

    interface = debug_link.NetworkInterface('eth0')
    self.PatchObject(interface, 'Rename', side_effect=_RenameSideEffect)
    debug_link._ConfigureDebugLink(interface)
    interface.Rename.assert_has_calls([mock.call(_IFNAME0),
                                       mock.call(_IFNAME1)])

  def testExhaustNames(self):
    """Test to make sure exception is thrown if all usb* names are in use."""
    interface = debug_link.NetworkInterface('eth0')
    self.PatchObject(
        interface, 'Rename', side_effect=debug_link.InterfaceNameExistsError())
    self.assertRaises(debug_link.DebugLinkInitializationFailedError,
                      debug_link._ConfigureDebugLink, (interface))


class InitializeDebugLinkTest(cros_test_lib.MockTestCase):
  """Tests for InitializeDebugLink()."""

  def setUp(self):
    self.StartPatcher(NetworkInterfaceMock())

  def _MockNetworkDevices(self, devices_info):
    """Mocks the environment to return the specified device information.

    Args:
      devices_info: List of device information to return.  Each entry is a
        tuple: (ifname, properties, is_up, has_ip).

    Returns:
      Dictionary of mock NetworkInterface objects with |ifname| as keys.
    """
    interfaces = {}
    for ifname, props, is_up, has_ip in devices_info:
      interface = debug_link.NetworkInterface(ifname)
      self.PatchObject(interface, 'GetDeviceProperties', return_value=props)
      self.PatchObject(interface, 'IsUp', return_value=is_up)
      self.PatchObject(interface, 'HasIPAddress', return_value=has_ip)
      interfaces[ifname] = interface
    self.PatchObject(
        debug_link.NetworkInterface, 'GetInterfaces',
        return_value=[interface for _, interface in interfaces.iteritems()])
    return interfaces

  def testOneDebugLinkUnconfigured(self):
    """Test initializing one unconfigured Debug Link."""
    # Mock out one uninteresting device and one interesting device that is
    # not configured.
    interfaces = self._MockNetworkDevices([
        ('eth0', {'prop': 'not-important'}, True, True),
        ('eth1', {debug_link._PROPERTY_PRODUCT_ID: debug_link._PRODUCT_ID,
                  debug_link._PROPERTY_VENDOR_ID: debug_link._VENDOR_ID},
         False, False)])

    ip = debug_link.InitializeDebugLink()
    self.assertEqual(ip, debug_link._HOST_IP)
    intf = interfaces['eth1']
    intf.BringDown.assert_called_once_with(intf)
    intf.Rename.assert_called_once_with(intf, _IFNAME0)
    intf.BringUp.assert_called_once_with(intf, debug_link._HOST_IP)

  def testOneDebugLinkAlreadyInitialized(self):
    """Test one Debug Link that is already initialized."""
    # Mock out one interesting device that is already configured.
    interfaces = self._MockNetworkDevices([
        (_IFNAME0, {debug_link._PROPERTY_PRODUCT_ID: debug_link._PRODUCT_ID,
                    debug_link._PROPERTY_VENDOR_ID: debug_link._VENDOR_ID},
         True, True)])

    ip = debug_link.InitializeDebugLink()
    self.assertEqual(ip, debug_link._HOST_IP)
    intf = interfaces[_IFNAME0]
    self.assertFalse(intf.BringDown.called)
    self.assertFalse(intf.Rename.called)

  def testNoDebugLink(self):
    """Test for exception raised when no Debug Link is found."""
    # Mock out two uninteresting devices.
    self._MockNetworkDevices([
        ('eth0', {'prop': 'not-important'}, True, True),
        ('eth1', {'prop': 'also-not-important'}, True, True)])

    self.assertRaises(debug_link.DebugLinkMissingError,
                      debug_link.InitializeDebugLink)
