#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re

import pyauto_functional
import chromeos_network  # pyauto_functional must come before chromeos_network


class WifiNotification(chromeos_network.PyNetworkUITest):
  """Test for ChromeOS wifi Network Disconnected Notification."""

  def testNetworkDisconnectNotification(self):
    """Check network disconnect notification."""
    self.NetworkScan()
    service_path = self.GetServicePath('dap-1522')
    self.ConnectToWifiNetwork(service_path, password='wrongpasswor')
    self.WaitForNotificationCount(1)
    notification_result = self.GetActiveNotifications()[0]['content_url']
    result_error = re.search('Error|Failed', notification_result)
    self.assertTrue(result_error, 'Expected to find Error/Failed in '
                                  'notification, not found as expected.')
    result_ssid = re.search('dap-1522', notification_result)
    self.assertTrue(result_ssid, 'SSID is not found.')

    # TODO(tturchetto): Uncomment this when bug is fixed. crosbug.com/15176
    #self.CloseNotification(0)


if __name__ == '__main__':
  pyauto_functional.Main()
