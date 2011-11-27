#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess

import pyauto_functional
import pyauto
import chromeos_network


class PrivateNetworkTest(chromeos_network.PyNetworkUITest):
  """Tests for VPN.

  Expected to be run with access to the lab setup as defined in
  vpn_testbed_config.
  """

  def _PingTest(self, hostname, timeout=10):
    """Attempt to ping a remote host.

    Returns:
      True if the ping succeeds.
      False otherwise.
    """
    return subprocess.call(['ping', '-c', '1', '-W',
                           str(timeout), hostname]) == 0

  def testCanAddNetwork(self):
    """Test to add a VPN network, connect and disconnect."""
    # Load VPN config data from file.
    vpn_info_file = os.path.join(pyauto.PyUITest.DataDir(),
                                 'pyauto_private/chromeos/network',
                                 'vpn_testbed_config')
    self.assertTrue(os.path.exists(vpn_info_file))
    vpn = self.EvalDataFrom(vpn_info_file)

    # Connect to wifi.
    self.NetworkScan()
    self.WaitUntilWifiNetworkAvailable(vpn['wifi'])
    wifi_vpn = self.GetServicePath(vpn['wifi'])
    self.assertTrue(wifi_vpn)
    self.assertTrue(self.ConnectToWifiNetwork(wifi_vpn) is None)
    self.assertFalse(self._PingTest(vpn['ping']),
                     msg='VPN ping succeeded when not connected.')

    # Connect to the VPN.
    self.AddPrivateNetwork(hostname=vpn['hostname'],
                           service_name=vpn['service_name'],
                           provider_type=vpn['provider_type'],
                           username=vpn['username'],
                           password=vpn['password'],
                           key=vpn['key'])

    # Get private network info.
    result = self.GetPrivateNetworkInfo()
    self.assertTrue('connected' in result, msg='Could not connect to VPN')
    connected = result['connected']
    self.assertTrue(self._PingTest(vpn['ping']), msg='VPN ping failed.')
    self.DisconnectFromPrivateNetwork()
    self.assertFalse(self._PingTest(vpn['ping']),
                     msg='VPN ping succeeded when not connected.')
    # Connect to the remembered private network.
    self.ConnectToPrivateNetwork(connected)
    self.assertTrue(self._PingTest(vpn['ping']), msg='VPN ping failed.')
    self.DisconnectFromPrivateNetwork()
    self.assertFalse(self._PingTest(vpn['ping']),
                     msg='VPN ping succeeded when not connected.')


if __name__ == '__main__':
  pyauto_functional.Main()
