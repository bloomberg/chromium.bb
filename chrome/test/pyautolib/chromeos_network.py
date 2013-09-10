# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import dbus
import logging
import os
import time

from chromeos.power_strip import PowerStrip
import pyauto
import pyauto_errors

class PyNetworkUITest(pyauto.PyUITest):
  """A subclass of PyUITest for Chrome OS network tests.

  A subclass of PyUITest that automatically sets the flimflam
  priorities to put wifi connections first before starting tests.
  This is for convenience when writing wifi tests.
  """
  _ROUTER_CONFIG_FILE = os.path.join(pyauto.PyUITest.DataDir(),
                                     'pyauto_private', 'chromeos', 'network',
                                     'wifi_testbed_config')
  _FLIMFLAM_PATH = 'org.chromium.flimflam'

  def setUp(self):
    self.CleanupFlimflamDirsOnChromeOS()
    # Move ethernet to the end of the flimflam priority list,
    # effectively hiding any ssh connections that the
    # test harness might be using and putting wifi ahead.
    self._PushServiceOrder('wifi,ethernet')
    self._ParseDefaultRoutingTable()
    pyauto.PyUITest.setUp(self)

  def tearDown(self):
    pyauto.PyUITest.tearDown(self)
    self._PopServiceOrder()
    # Remove the route entry for the power strip.
    if hasattr(self, 'ps_route_entry'):
      os.system('route del -net %(ipaddress)s gateway %(gateway)s netmask '
                '%(netmask)s dev %(iface)s' % self.ps_route_entry)

  def _GetFlimflamManager(self):
    _proxy = dbus.SystemBus().get_object(self._FLIMFLAM_PATH, '/')
    return dbus.Interface(_proxy, self._FLIMFLAM_PATH + '.Manager')

  def _ParseDefaultRoutingTable(self):
    """Creates and stores a dictionary of the default routing paths."""
    route_table_headers = ['destination', 'gateway', 'genmask', 'flags',
                           'metric', 'ref', 'use', 'iface']
    routes = os.popen('route -n | egrep "^0.0.0.0"').read()
    routes = [interface.split() for interface in routes.split('\n')][:-1]
    self.default_routes = {}
    for iface in routes:
      self.default_routes[iface[-1]] = dict(zip(route_table_headers, iface))

  def _SetServiceOrder(self, service_order):
    self._GetFlimflamManager().SetServiceOrder(service_order)
    # Flimflam throws a dbus exception if device is already disabled.  This
    # is not an error.
    try:
      self._GetFlimflamManager().DisableTechnology('wifi')
    except dbus.DBusException as e:
      if 'org.chromium.flimflam.Error.AlreadyDisabled' not in str(e):
        raise e
    self._GetFlimflamManager().EnableTechnology('wifi')

  def _PushServiceOrder(self, service_order):
    self._old_service_order = self._GetFlimflamManager().GetServiceOrder()
    self._SetServiceOrder(service_order)
    service_order = service_order.split(',')

    # Verify services that are present in both the service_order
    # we set and the one retrieved from device are in the correct order.
    set_service_order = self._GetFlimflamManager().GetServiceOrder().split(',')
    common_service = set(service_order) & set(set_service_order)

    service_order = [s for s in service_order if s in common_service]
    set_service_order = [s for s in set_service_order if s in common_service]

    assert service_order == set_service_order, \
        'Flimflam service order not set properly. %s != %s' % \
        (service_order, set_service_order)

  def _PopServiceOrder(self):
    self._SetServiceOrder(self._old_service_order)

    # Verify services that are present in both the service_order
    # we set and the one retrieved from device are in the correct order.
    old_service_order = self._old_service_order.split(',')
    set_service_order = self._GetFlimflamManager().GetServiceOrder().split(',')
    common_service = set(old_service_order) & set(set_service_order)

    old_service_order = [s for s in old_service_order if s in common_service]
    set_service_order = [s for s in set_service_order if s in common_service]

    assert old_service_order == set_service_order, \
        'Flimflam service order not set properly. %s != %s' % \
        (old_service_order, set_service_order)
