#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re

import pyauto_functional
import chromeos_network  # pyauto_functional must come before chromeos_network


class WifiNotification(chromeos_network.PyNetworkUITest):
  """Test for ChromeOS wifi Network Disconnected Notification.

  These tests will be testing Network Disconnected Notification on
  various network encryptions (WEP,RSN, WPA) and various password lengths.
  """

  password1 = 'wrongpasswor'
  password5 = 'tente'
  password10 = 'tententent'
  password13 = 'thirteenthirt'
  password26 = 'twentysixtwentysixtwentysi'
  password64 = \
    'abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijkl'

  def _WifiNotification(self, router_name, password):
    """Basic test for wifi notification.

    Args:
      router_name: The name of the router.
      password: invalid password.
    """
    self.InitWifiPowerStrip()
    router_config = self.GetRouterConfig(router_name)
    self.RouterPower(router_name, True)

    self.assertTrue(self.WaitUntilWifiNetworkAvailable(router_config['ssid']),
                    'Wifi network %s never showed up.' % router_config['ssid'])

    service_path = self.GetServicePath(router_config['ssid'])
    self.ConnectToWifiNetwork(service_path, password=password)
    self.WaitForNotificationCount(1)

    notification_result = self.GetActiveNotifications()[0]['content_url']
    result_error = re.search('Error|Failed', notification_result)
    self.assertTrue(result_error, 'Expected to find Error/Failed in '
                                  'notification, not found as expected.')
    result_ssid = re.search(router_config['ssid'], notification_result)
    self.assertTrue(result_ssid,
                    'SSID is not found. Notification text is: "%s"'
                    % notification_result)

  def testWifiNotificationWEP_Linksys_WRT54G2_wrongpassword(self):
    """wifi disconnect notification-Linksys_WRT54G2.(WEP)-invalid password"""
    self._WifiNotification('Linksys_WRT54G2', WifiNotification.password1)

  def testWifiNotificationWEP_Linksys_WRT54G2_five_char(self):
    """wifi disconnect notification for Linksys_WRT54G2.(WEP)-5 password """
    self._WifiNotification('Linksys_WRT54G2', WifiNotification.password5)

  def testWifiNotificationWEP_Linksys_WRT54G2_ten_char(self):
    """wifi disconnect notification for Linksys_WRT54G2.(WEP)-10 password"""
    self._WifiNotification('Linksys_WRT54G2', WifiNotification.password10)

  def testWifiNotificationWEP_Linksys_WRT54G2_thirteen_char(self):
    """wifi disconnect notification for Linksys_WRT54G2.(WEP)-13 password"""
    self._WifiNotification('Linksys_WRT54G2', WifiNotification.password13)

  def testWifiNotificationWEP_Linksys_WRT54G2_twentysix_char(self):
    """wifi disconnect notification for Linksys_WRT54G2.(WEP)-26 password"""
    self._WifiNotification('Linksys_WRT54G2', WifiNotification.password26)

  def testWifiNotificationRSN_Belkin_G_wrongpassword(self):
    """wifi disconnect notification for Belkin_G (rsn)-wrong password"""
    self._WifiNotification('Belkin_G', WifiNotification.password1)

  def testWifiNotificationWPA_Trendnet_639gr_wrongpassword(self):
    """wifi disconnect notification for Trendnet_639gr (WPA)-wrong password"""
    self._WifiNotification('Trendnet_639gr', WifiNotification.password1)

  def testWifiNotificationWPA_Trendnet_639gr_five_char(self):
    """wifi disconnect notification for Trendnet_639gr (WPA)-5 password"""
    self._WifiNotification('Trendnet_639gr', WifiNotification.password5)

  def testWifiNotificationWPA_Trendnet_639gr_sixtyfour_char(self):
    """wifi disconnect notification for Trendnet_639gr (WPA)-64 password"""
    self._WifiNotification('Trendnet_639gr', WifiNotification.password64)


if __name__ == '__main__':
  pyauto_functional.Main()
