#!/usr/bin/env python
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

    # If the wifi network is expected to be invisible, the following
    # line should timeout which is expected.
    wifi_visible = self.WaitUntilWifiNetworkAvailable(router['ssid'],
                                                 is_hidden=router.get('hidden'))

    # Note, we expect wifi_visible and 'hidden' status to be opposites.
    # The test fails if the network visibility is not as expected.
    if wifi_visible == router.get('hidden', False):
      self.fail('We expected wifi network "%s" to be %s, but it was not.' %
                (router['ssid'],
                 {True: 'hidden', False: 'visible'}[router.get('hidden',
                 False)]))

    # Verify connect did not have any errors.
    error = self.ConnectToWifiRouter(router_name)
    self.assertFalse(error, 'Failed to connect to wifi network %s. '
                            'Reason: %s.' % (router['ssid'], error))

    # Verify the network we connected to.
    ssid = self.GetConnectedWifi()
    self.assertEqual(ssid, router['ssid'],
                     'Did not successfully connect to wifi network %s.' % ssid)

    self.DisconnectFromWifiNetwork()

  def testConnectBelkinG(self):
    """Test connecting to the Belkin G router."""
    self._BasicConnectRouterCompliance('Belkin_G')

  def testConnectBelkinNPlus(self):
    """Test connecting to the Belkin N+ router."""
    self._BasicConnectRouterCompliance('Belkin_N+')

  def testConnectDLinkN150(self):
    """Test connecting to the D-Link N150 router."""
    self._BasicConnectRouterCompliance('D-Link_N150')

  def testConnectLinksysE3000(self):
    """Test connecting to the Linksys E3000 router.

    The LinksysE3000 supports broadcasting of up to 2 SSID's.
    This test will try connecting to each of them one at a time.
    """
    self._BasicConnectRouterCompliance('LinksysE3000')
    self._BasicConnectRouterCompliance('LinksysE3000_2')

  def testConnectLinksysWRT54G2(self):
    """Test connecting to the Linksys WRT54G2 router."""
    self._BasicConnectRouterCompliance('Linksys_WRT54G2')

  def testConnectLinksysWRT54GL(self):
    """Test connecting to the LinksysWRT54GL router."""
    self._BasicConnectRouterCompliance('Linksys_WRT54GL')

  def testConnectNetgearN300(self):
    """Test connecting to the Netgear N300 router."""
    self._BasicConnectRouterCompliance('Netgear_N300')

  def testConnectNetgearWGR614(self):
    """Test connecting to the Netgear WGR 614 router."""
    self._BasicConnectRouterCompliance('Netgear_WGR614')

  def testConnectNfiniti(self):
    """Test connecting to the Nfiniti router."""
    self._BasicConnectRouterCompliance('Nfiniti')

  def testConnectSMCWBR145(self):
    """Test connecting to the SMC WBR 145 router."""
    self._BasicConnectRouterCompliance('SMC_WBR145')

  def testConnectTrendnet_639gr(self):
    """Test connecting to the Trendnet 639gr router.

    The LinksysE3000 supports broadcasting of up to 4 SSID's.
    This test will try connecting to each of them one at a time.
    """
    self._BasicConnectRouterCompliance('Trendnet_639gr')
    self._BasicConnectRouterCompliance('Trendnet_639gr_2')
    self._BasicConnectRouterCompliance('Trendnet_639gr_3')
    self._BasicConnectRouterCompliance('Trendnet_639gr_4')


if __name__ == '__main__':
  pyauto_functional.Main()
