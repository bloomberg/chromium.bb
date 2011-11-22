#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

import pyauto_functional
import chromeos_network  # pyauto_functional must come before chromeos_network
import pyauto_utils


class ChromeosWifiFunctional(chromeos_network.PyNetworkUITest):
  """Tests for ChromeOS wifi functionality.

  These tests should be run within vacinity of the power strip where the wifi
  routers are attached.
  """

  def _LoginDevice(self, test_account='test_google_account'):
    """Logs into the device and cleans up flimflam profile.

    Args: 
      test_account: The account used to login to the device.
    """
    if not self.GetLoginInfo()['is_logged_in']:
      credentials = self.GetPrivateInfo()[test_account]
      self.Login(credentials['username'], credentials['password'])
      login_info = self.GetLoginInfo()
      self.assertTrue(login_info['is_logged_in'], msg='Login failed.')

    ff_dir = '/home/chronos/user/flimflam'
    self.assertTrue(os.path.isdir(ff_dir), 'User is logged in but user '
                    'flimflam profile is not present.')

    # Clean up the items in the flimflam profile directory.
    for item in os.listdir(ff_dir):
      pyauto_utils.RemovePath(os.path.join(ff_dir, item))

  def tearDown(self):
    if self.GetLoginInfo().get('is_logged_in'):
      self.Logout()
    chromeos_network.PyNetworkUITest.tearDown(self)

  def _SetupRouter(self, router_name):
    """Turn on the router and wait for it to come on.

    Args:
      router_name: The name of the router as defined in wifi_testbed_config.

    Returns:
      A dictionary of the router and its attributes.  The format is same as
      the definition in wifi_testbed_config
    """
    self.InitWifiPowerStrip()
    router = self.GetRouterConfig(router_name)
    self.RouterPower(router_name, True)

    # When we connect to a wifi service, it should be added to the
    # remembered_wifi list.
    self.WaitUntilWifiNetworkAvailable(router['ssid'],
                                       is_hidden=router.get('hidden'))
    return router

  def _VerifyIfConnectedToNetwork(self, network_ssid, status='Online state'):
    """Verify if we are connected to the network.
    
    The test calling this function will fail for one of these three reasons:
    1. The server path for the SSID is not found.
    2. If we are not connected to the network.
    3. If we did not find the network in the wifi_networks list.

    Args:
      newtork_ssid: The network to which we are supposed to be connected to.
      status: The status that we expect the network to have, by default it
              would be 'Online state'.
    """
    service_path = self.GetServicePath(network_ssid)
    self.assertTrue(service_path is not None,
                    msg='Could not find a service path for the given ssid %s.' %
                    network_ssid)
    wifi_network = self.NetworkScan()['wifi_networks']
    for path in wifi_network:
      if path == service_path:
        self.assertTrue(
            wifi_network[path]['status'] == status,
            msg='Unexpected network status %s, Network %s should have '
                'status %s.' % (wifi_network[path]['status'],
                network_ssid, status))
        break;
    else:
      self.fail(msg='Did not find the network %s in the '
                    'wifi_networks list.' % network_ssid)

  def testConnectShareEncryptedNetwork(self):
    """A shared encrypted network can connect and is remembered.

    Note: This test does not verify that the network is added to the public
    profile
    """
    router_name = 'D-Link_N150'
    self._LoginDevice()
    router = self._SetupRouter(router_name)
    error = self.ConnectToWifiRouter(router_name, shared=True)
    self.assertFalse(error, 'Failed to connect to wifi network %s. '
                            'Reason: %s.' % (router['ssid'], error))
    service_path = self.GetServicePath(router['ssid'])
    self.assertTrue(service_path in self.GetNetworkInfo()['remembered_wifi'],
                    'Connected wifi was not added to the remembered list.')
    self.ForgetWifiNetwork(service_path)
    self.assertFalse(service_path in self.GetNetworkInfo()['remembered_wifi'],
                     'Connected wifi was not removed from the remembered list.')

  def testConnectNoShareEncryptedNetwork(self):
    """A non-shared encrypted network can connect and is remembered.

    Note: This test does not verify that the network is added to the private
    profile
    """
    router_name = 'D-Link_N150'
    self._LoginDevice()
    router = self._SetupRouter(router_name)
    error = self.ConnectToWifiRouter(router_name, shared=False)
    self.assertFalse(error, 'Failed to connect to wifi network %s. '
                            'Reason: %s.' % (router['ssid'], error))
    service_path = self.GetServicePath(router['ssid'])
    self.assertTrue(service_path in self.GetNetworkInfo()['remembered_wifi'],
                    'Connected wifi was not added to the remembered list.')
    self.ForgetWifiNetwork(service_path)
    self.assertFalse(service_path in self.GetNetworkInfo()['remembered_wifi'],
                     'Connected wifi was not removed from the remembered list.')

  def testConnectToSharedOpenNetwork(self):
    """Can connect to a shared open network.

    Verify that the connected network is in the remembered network list 
    for all the users.
    """
    router_name = 'Trendnet_639gr_4'
    self._LoginDevice()
    router = self._SetupRouter(router_name)
    error = self.ConnectToWifiRouter(router_name)
    self.assertFalse(error, msg='Failed to connect to wifi network %s. '
                            'Reason: %s.' % (router['ssid'], error))
    service_path = self.GetServicePath(router['ssid'])
    self.assertTrue(service_path in self.GetNetworkInfo()['remembered_wifi'],
                    msg='Open wifi is not remembered for the current user.')
    self.Logout()
    self._LoginDevice(test_account='test_google_account_2')
    self.assertTrue(service_path in self.NetworkScan()['remembered_wifi'],
                    msg='Open network is not shared with other users.')

  def testConnectToSharedHiddenNetwork(self):
    """Can connect to shared hidden network and verify that it's shared."""
    router_name = 'Netgear_WGR614'
    self._LoginDevice()
    router = self._SetupRouter(router_name)
    error = self.ConnectToWifiRouter(router_name)
    self.assertFalse(error, msg='Failed to connect to hidden network %s. '
                            'Reason: %s.' % (router['ssid'], error))
    service_path = self.GetServicePath(router['ssid'])
    self.assertTrue(service_path in self.NetworkScan()['remembered_wifi'],
                    msg='Hidden network is not added to the remembered list.')
    self.Logout()
    self._LoginDevice(test_account='test_google_account_2')
    self.assertTrue(service_path in self.NetworkScan()['remembered_wifi'],
                    msg='Shared hidden network is not in other user\'s '
                    'remembered list.')

  def testConnectToNonSharedHiddenNetwork(self):
    """Can connect to a non-shared hidden network.

    Verify that it is not shared with other users.
    """  
    router_name = 'Linksys_WRT54GL'
    self._LoginDevice()
    router = self._SetupRouter(router_name)
    error = self.ConnectToWifiRouter(router_name, shared=False)
    self.assertFalse(error, msg='Failed to connect to hidden network %s. '
                            'Reason: %s.' % (router['ssid'], error))
    service_path = self.GetServicePath(router['ssid'])
    self.assertTrue(service_path in self.NetworkScan()['remembered_wifi'],
                    msg='Hidden network is not added to the remembered list.')
    self.Logout()
    self._LoginDevice(test_account='test_google_account_2')
    self.assertFalse(service_path in self.NetworkScan()['remembered_wifi'],
                     msg='Non-shared hidden network %s is shared.'
                     % router['ssid'])

  def testConnectToEncryptedNetworkInLoginScreen(self):
    """Can connect to encrypted network in login screen.
 
    Verify that this network is in the remembered list after login.
    """
    router_name = 'Belkin_G'
    if self.GetLoginInfo()['is_logged_in']:
      self.Logout()
    router = self._SetupRouter(router_name)
    error = self.ConnectToWifiRouter(router_name)
    self.assertFalse(error, 'Failed to connect to wifi network %s. '
                            'Reason: %s.' % (router['ssid'], error))
    service_path = self.GetServicePath(router['ssid'])
    self._VerifyIfConnectedToNetwork(router['ssid'], 'Connected')   
    self._LoginDevice()
    self.assertTrue(service_path in self.NetworkScan()['remembered_wifi'],
                    msg='Network is not added to the remembered list.')


if __name__ == '__main__':
  pyauto_functional.Main()
