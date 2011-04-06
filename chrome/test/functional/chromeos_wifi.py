#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

import pyauto_functional
import chromeos_network  # pyauto_functional must come before chromeos_network


class ChromeosWifi(chromeos_network.PyNetworkUITest):
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

if __name__ == '__main__':
  pyauto_functional.Main()
