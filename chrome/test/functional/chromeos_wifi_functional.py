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

  def _LoginDevice(self):
    """Logs into the device and cleans up flimflam profile."""
    if not self.GetLoginInfo()['is_logged_in']:
      credentials = self.GetPrivateInfo()['test_google_account']
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


if __name__ == '__main__':
  pyauto_functional.Main()
