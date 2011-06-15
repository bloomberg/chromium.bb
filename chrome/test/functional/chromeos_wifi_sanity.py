#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

import pyauto_functional
import chromeos_network  # pyauto_functional must come before chromeos_network


class ChromeosWifiSanity(chromeos_network.PyNetworkUITest):
  """Tests for ChromeOS wifi."""

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

  def testConnectToHiddenWiFiNonExistent(self):
    """Connecting to a non-existent network should fail.

    Assume network 'ThisIsANonExistentNetwork' is not a valid ssid within
    the vicinity of where this test is run.
    """
    ssid = 'ThisIsANonExistentNetwork'
    error = self.ConnectToHiddenWifiNetwork(ssid, 'SECURITY_NONE')
    self.assertTrue(error, msg='Device connected to a non-existent '
                                           'network "%s".' % ssid)

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
