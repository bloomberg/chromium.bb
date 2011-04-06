#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pyauto_functional
import chromeos_network  # pyauto_functional must come before chromeos_network


class ChromeosWifiCompliance(chromeos_network.PyNetworkUITest):
  """Tests for ChromeOS wifi complaince.

  These tests should be run within vacinity of the power strip where the wifi
  routers are attached.
  """

  def _BasicConnectRouterCompliance(self, router_name):
    """Generic basic test routine for connecting to a router.

    Args:
      router_name: The name of the router.
    """
    self.InitWifiPowerStrip()
    router = self.GetRouterConfig(router_name)
    self.RouterPower(router_name, True)

    self.assertTrue(self.WaitUntilWifiNetworkAvailable(router['ssid']),
                    'Wifi network %s never showed up.' % router['ssid'])

    # Verify connect did not have any errors.
    error = self.ConnectToWifiRouter(router_name)
    self.assertFalse(error, 'Failed to connect to wifi network %s. '
                            'Reason: %s.' % (router['ssid'], error))

    # Verify the network we connected to.
    ssid = self.GetConnectedWifi()
    self.assertEqual(ssid, router['ssid'],
                     'Did not successfully connect to wifi network %s.' % ssid)

    self.DisconnectFromWifiNetwork()
    self.RouterPower(router_name, False)

  def testConnectBelkinG(self):
    """Test connecting to the Belkin G router."""
    self._BasicConnectRouterCompliance('Belkin_G')

  def testConnectLinksysWRT54G2(self):
    """Test connecting to the Linksys WRT54G2 router."""
    self._BasicConnectRouterCompliance('Linksys_WRT54G2')


if __name__ == '__main__':
  pyauto_functional.Main()
