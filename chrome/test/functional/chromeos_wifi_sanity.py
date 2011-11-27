#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

import pyauto_functional
import chromeos_network  # pyauto_functional must come before chromeos_network


class ChromeosWifiSanity(chromeos_network.PyNetworkUITest):
  """Tests for ChromeOS network related functions."""

  def testNetworkInfoAndScan(self):
    """Get basic info on networks."""
    # NetworkScan will also call GetNetworkInfo and return the results.
    result = self.NetworkScan()
    self.assertTrue(result)
    logging.debug(result)

  def testGetProxySettings(self):
    """Print some information about proxy settings."""
    result = self.GetProxySettingsOnChromeOS()
    self.assertTrue(result)
    logging.debug(result)

  def testToggleNetworkDevice(self):
    """Sanity check to make sure wifi can be disabled and reenabled."""
    self.ToggleNetworkDevice('wifi', False)
    self.assertFalse(self.GetNetworkInfo()['wifi_enabled'],
                     'Disabled wifi but it is still enabled.')
    self.assertFalse('wifi_networks' in self.GetNetworkInfo(), 'GetNetworkInfo '
                     'returned a wifi_networks dict, but wifi is disabled.')
    self.ToggleNetworkDevice("wifi", True)
    self.assertTrue(self.GetNetworkInfo()['wifi_enabled'],
                    'Enabled wifi but it is still disabled.')
    self.assertTrue('wifi_networks' in self.GetNetworkInfo(), 'GetNetworkInfo '
                    'did not return a wifi_networks dict.')

  def testConnectToHiddenWiFiNonExistent(self):
    """Connecting to a non-existent network should fail.

    Assume network 'ThisIsANonExistentNetwork' is not a valid ssid within
    the vicinity of where this test is run.
    """
    ssid = 'ThisIsANonExistentNetwork'
    error = self.ConnectToHiddenWifiNetwork(ssid, 'SECURITY_NONE')
    self.assertTrue(error is not None, msg='Device connected to a non-existent '
                                           'network "%s".' % ssid)
    self.assertTrue(error != '', msg='Device had a connection error but no '
                                     'error message.')

  def testForgetWifiNetwork(self):
    """Basic test to verify there are no problems calling ForgetWifiNetwork."""
    self.ForgetAllRememberedNetworks()
    # This call should have no problems regardless of whether or not
    # the network exists.
    self.ForgetWifiNetwork('')
    self.assertFalse(self.GetNetworkInfo()['remembered_wifi'], 'All '
                     'remembered wifi networks should have been removed')


if __name__ == '__main__':
  pyauto_functional.Main()
