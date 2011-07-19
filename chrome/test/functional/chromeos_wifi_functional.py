#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pyauto_functional
import chromeos_network  # pyauto_functional must come before chromeos_network


class ChromeosWifiFunctional(chromeos_network.PyNetworkUITest):
  """Tests for ChromeOS wifi functionality.

  These tests should be run within vacinity of the power strip where the wifi
  routers are attached.
  """
  def testAddForgetRememberedNetworks(self):
    """Verifies that networks are remembered on connect and can be forgotten."""
    router_name = 'Belkin_G'
    self.InitWifiPowerStrip()
    router = self.GetRouterConfig(router_name)
    self.RouterPower(router_name, True)

    # When we connect to a wifi service, it should be added to the
    # remembered_wifi list.
    self.WaitUntilWifiNetworkAvailable(router['ssid'],
                                       is_hidden=router.get('hidden'))
    error = self.ConnectToWifiRouter(router_name)
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
