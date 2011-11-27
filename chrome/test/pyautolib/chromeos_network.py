# Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

class WifiPowerStrip(PowerStrip):
  """Manages the power state of wifi routers connected to a power strip.

  This class provides additional functionality over PowerStrip by providing
  a timeout feature for wifi routers connected to the strip.  This is to prevent
  repeated on/off calls to the same router which may put the router in an
  undesired state.
  """

  def __init__ (self, host, routers):
    """Initializes a WifiPowerStrip object.

    Args:
      host: IP of the switch that the routers are attached to.
      routers: Dictionary of wifi routers in the following format:
               {
                 '< router name >': {
                   'strip_id' : '.aX'  # where X is the port number
                   < additional fields may be added here for each router >
                 }
               }
    """
    self._router_dict = routers

    # Each router will have a timestamp associated to it regarding whether
    # or not an action can be performed on it yet.   This is to prevent
    # the spamming of power on/off calls on a particular router.
    # The WifiPowerStrip_UsableTime field specifies the earliest time
    # after which the router may be used.  We will initialize it to now
    # since they should all be usable at init.
    for router_info in self._router_dict.values():
      router_info['WifiPowerStrip_UsableTime'] = time.time()

    # _routers_used keeps track of which routers were used during the lifetime
    # of the WifiPowerStrip instance.  Adding used routers occurs when
    # a wifi router has been turned on.  Otherwise, it get clears upon
    # the TurnOffUsedRouters call.
    self._routers_used = set()
    PowerStrip.__init__(self, host)

  def GetRouterConfig(self, router_name):
    """Returns the configuration for the specified router.

    Args:
      router_name: A string specifying the router.

    Returns:
      The config dictionary for the given router if the router is defined.
      None otherwise.
    """
    return copy.deepcopy(self._router_dict.get(router_name))

  def RouterPower(self, router_name, power_state, pause_after=5):
    """Executes PowerStrip commands.

    Args:
      router_name: The name of the router to perform the action on.
      power_state: A boolean value where True represents turning the router on
                   and False represents turning the router off.
      pause_after: Specified in seconds, and specifies the time to sleep
                   after a command is run.  This is to prevent spamming of
                   power on/off of the same router which has put the router
                   in an undesirable state.

    Raises:
      Exception if router_name is not a valid router.
    """
    router = self.GetRouterConfig(router_name)
    if not router: raise Exception('Invalid router name \'%s\'.' % router_name)

    # Hidden routers will always be on.  Don't allow controlling of the power
    # for these networks.
    if router.get('hidden'):
      return

    sleep_time = router['WifiPowerStrip_UsableTime'] - time.time()
    if sleep_time > 0:
      time.sleep(sleep_time)

    if power_state:
      self._routers_used |= set([router_name])
      logging.debug('Turning on router %s:%s.' %
                    (router['strip_id'], router_name))
      self.PowerOn(router['strip_id'])
    else:
      logging.debug('Turning off router %s:%s.' %
                    (router['strip_id'], router_name))
      self.PowerOff(router['strip_id'])

    # Set the Usable time of the particular router to pause_after
    # seconds after the current time.
    router['WifiPowerStrip_UsableTime'] = time.time() + pause_after

  def TurnOffAllRouters(self):
    """Turns off all the routers."""
    for router in self._router_dict:
      self.RouterPower(router, False, pause_after=0)

  def TurnOffUsedRouters(self):
    """Turns off the routers that were once turned on."""
    for router in self._routers_used:
      self.RouterPower(router, False, pause_after=0)

    self._routers_used = set()


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
  _proxy = dbus.SystemBus().get_object(_FLIMFLAM_PATH, '/')
  _manager = dbus.Interface(_proxy, _FLIMFLAM_PATH + '.Manager')

  def setUp(self):
    # Move ethernet to the end of the flimflam priority list,
    # effectively hiding any ssh connections that the
    # test harness might be using and putting wifi ahead.
    self._PushServiceOrder('vpn,bluetooth,wifi,wimax,cellular,ethernet')
    self._ParseDefaultRoutingTable()
    pyauto.PyUITest.setUp(self)
    self.ForgetAllRememberedNetworks()
    self._wifi_power_strip = None

  def tearDown(self):
    self.ForgetAllRememberedNetworks()
    pyauto.PyUITest.tearDown(self)
    self._PopServiceOrder()
    if self._wifi_power_strip:
      self._wifi_power_strip.TurnOffUsedRouters()
    # Remove the route entry for the power strip.
    if hasattr(self, 'ps_route_entry'):
      os.system('route del -net %(ipaddress)s gateway %(gateway)s netmask '
                '%(netmask)s dev %(iface)s' % self.ps_route_entry)

  def _ParseDefaultRoutingTable(self):
    """Creates and stores a dictionary of the default routing paths."""
    route_table_headers = ['destination', 'gateway', 'genmask', 'flags',
                           'metric', 'ref', 'use', 'iface']
    routes = os.popen('route -n | egrep "^0.0.0.0"').read()
    routes = [interface.split() for interface in routes.split('\n')][:-1]
    self.default_routes = {}
    for iface in routes:
      self.default_routes[iface[-1]] = dict(zip(route_table_headers, iface))

  def ForgetAllRememberedNetworks(self):
    """Forgets all networks that the device has marked as remembered."""
    for service in self.GetNetworkInfo()['remembered_wifi']:
      self.ForgetWifiNetwork(service)

  def _SetServiceOrder(self, service_order):
    self._manager.SetServiceOrder(service_order)
    # Flimflam throws a dbus exception if device is already disabled.  This
    # is not an error.
    try:
      self._manager.DisableTechnology('wifi')
    except dbus.DBusException as e:
      if 'org.chromium.flimflam.Error.AlreadyDisabled' not in str(e):
        raise e
    self._manager.EnableTechnology('wifi')

  def _PushServiceOrder(self, service_order):
    self._old_service_order = self._manager.GetServiceOrder()
    self._SetServiceOrder(service_order)
    service_order = service_order.split(',')

    # Verify services that are present in both the service_order
    # we set and the one retrieved from device are in the correct order.
    set_service_order = self._manager.GetServiceOrder().split(',')
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
    set_service_order = self._manager.GetServiceOrder().split(',')
    common_service = set(old_service_order) & set(set_service_order)

    old_service_order = [s for s in old_service_order if s in common_service]
    set_service_order = [s for s in set_service_order if s in common_service]

    assert old_service_order == set_service_order, \
        'Flimflam service order not set properly. %s != %s' % \
        (old_service_order, set_service_order)

  def _SetupRouteForPowerStrip(self, ipaddress, iface='eth'):
    """Create a route table entry for the power strip."""

    # Assume device has only one interface that is prepended with
    # $iface and use that one.
    try:
      iface = [ key for key in self.default_routes.keys() if iface in key ][0]
    except:
      assert 'Unable to find interface of type %s.' % iface

    self.ps_route_entry = {
      'iface' : iface,
      'gateway' : self.default_routes[iface]['gateway'],
      'netmask' : '255.255.255.255',
      'ipaddress' : ipaddress
    }

    os.system('route add -net %(ipaddress)s gateway %(gateway)s netmask '
              '%(netmask)s dev %(iface)s' % self.ps_route_entry)

    # Verify the route was added.
    assert os.system('route -n | egrep "^%(ipaddress)s[[:space:]]+%(gateway)s'
                     '[[:space:]]+%(netmask)s"' % self.ps_route_entry) == 0, \
                     'Failed to create default route for powerstrip.'

  def InitWifiPowerStrip(self):
    """Initializes the router controller using the specified config file."""

    assert os.path.exists(PyNetworkUITest._ROUTER_CONFIG_FILE), \
           'Router configuration file does not exist.'

    config = pyauto.PyUITest.EvalDataFrom(self._ROUTER_CONFIG_FILE)
    strip_ip, routers = config['strip_ip'], config['routers']

    self._SetupRouteForPowerStrip(strip_ip)
    self._wifi_power_strip = WifiPowerStrip(strip_ip, routers)

    self.RouterPower = self._wifi_power_strip.RouterPower
    self.TurnOffAllRouters = self._wifi_power_strip.TurnOffAllRouters
    self.GetRouterConfig = self._wifi_power_strip.GetRouterConfig

  def WaitUntilWifiNetworkAvailable(self, ssid, timeout=60, is_hidden=False):
    """Waits until the given network is available.

    Routers that are just turned on may take up to 1 minute upon turning them
    on to broadcast their SSID.

    Args:
      ssid: SSID of the service we want to connect to.
      timeout: timeout (in seconds)

    Raises:
      Exception if timeout duration has been hit before wifi router is seen.

    Returns:
      True, when the wifi network is seen within the timout period.
      False, otherwise.
    """
    def _GotWifiNetwork():
      # Returns non-empty array if desired SSID is available.
      try:
        return [wifi for wifi in
                self.NetworkScan().get('wifi_networks', {}).values()
                if wifi.get('name') == ssid]
      except pyauto_errors.JSONInterfaceError:
        # Temporary fix until crosbug.com/14174 is fixed.
        # NetworkScan is only used in updating the list of networks so errors
        # thrown by it are not critical to the results of wifi tests that use
        # this method.
        return False

    # The hidden AP's will always be on, thus we will assume it is ready to
    # connect to.
    if is_hidden:
      return bool(_GotWifiNetwork())

    return self.WaitUntil(_GotWifiNetwork, timeout=timeout, retry_sleep=1)

  def GetConnectedWifi(self):
    """Returns the SSID of the currently connected wifi network.

    Returns:
      The SSID of the connected network or None if we're not connected.
    """
    service_list = self.GetNetworkInfo()
    connected_service_path = service_list.get('connected_wifi')
    if 'wifi_networks' in service_list and \
       connected_service_path in service_list['wifi_networks']:
       return service_list['wifi_networks'][connected_service_path]['name']

  def GetServicePath(self, ssid):
    """Returns the service path associated with an SSID.

    Args:
      ssid: String defining the SSID we are searching for.

    Returns:
      The service path or None if SSID does not exist.
    """
    service_list = self.GetNetworkInfo()
    service_list = service_list.get('wifi_networks', [])
    for service_path, service_obj in service_list.iteritems():
      if service_obj['name'] == ssid:
        return service_path
    return None

  def ConnectToWifiRouter(self, router_name, shared=True):
    """Connects to a router by name.

    Args:
      router_name: The name of the router that is specified in the
                   configuration file.
    """
    router = self._wifi_power_strip.GetRouterConfig(router_name)
    assert router, 'Router with name %s is not defined ' \
                   'in the router configuration.' % router_name
    security = router.get('security', 'SECURITY_NONE')
    passphrase = router.get('passphrase', '')

    # Branch off the connect calls depending on if the wifi network is hidden
    # or not.
    error_string = None
    if router.get('hidden'):
      error_string = self.ConnectToHiddenWifiNetwork(router['ssid'], security,
                                                     passphrase)
    else:
      service_path = self.GetServicePath(router['ssid'])
      assert service_path, 'Service with SSID %s is not present.' % \
             router['ssid']

      logging.debug('Connecting to router %s.' % router_name)
      error_string = self.ConnectToWifiNetwork(service_path,
                                               password=passphrase,
                                               shared=shared)
    return error_string
