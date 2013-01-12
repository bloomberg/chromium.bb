#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

import pyauto_functional  # must come before pyauto.
import policy_base
import pyauto


class ChromeosONC(policy_base.PolicyTestBase):
  """
  Tests for Open Network Configuration (ONC).

  Open Network Configuration (ONC) files is a json dictionary
  that contains network configurations and is pulled via policies.
  These tests verify that ONC files that are formatted correctly
  add the network/certificate to the device.
  """

  ONC_PATH = os.path.join(pyauto.PyUITest.ChromeOSDataDir(), 'network')

  def setUp(self):
    self.CleanupFlimflamDirsOnChromeOS()
    policy_base.PolicyTestBase.setUp(self)
    self.LoginWithTestAccount()

  def _ReadONCFileAndSet(self, filename):
    """Reads the specified ONC file and sends it as a policy.

    Inputs:
      filename: The filename of the ONC file.  ONC files should
                all be stored in the path defined by ONC_PATH.
    """
    with open(os.path.join(self.ONC_PATH, filename)) as fp:
      self.SetUserPolicy({'OpenNetworkConfiguration': fp.read()})

  def _VerifyRememberedWifiNetworks(self, wifi_expect):
    """Verify the list of remembered networks contains those in wifi_expect.

    Inputs:
      wifi_expect: A dictionary of wifi networks where the key is the ssid
                      and the value is the encryption type of the network.
    """
    # Sometimes there is a race condition where upon restarting chrome
    # NetworkScan has not populated the network lists yet.  We should
    # scan until the device is online.
    self.WaitUntil(lambda: not self.NetworkScan().get('offline_mode', True))
    networks = self.NetworkScan()

    # Temprorary dictionary to keep track of which wifi networks
    # have been visited by removing them as we see them.
    wifi_expect_temp = dict(wifi_expect)

    for service, wifi_dict in networks['remembered_wifi'].iteritems():
      if isinstance(wifi_dict, dict) and \
         'encryption' in wifi_dict and \
         'name' in wifi_dict:

        msg = ('Wifi network %s was in the remembered_network list but '
               'shouldn\'t be.' % wifi_dict['name'])

        # wifi_dict['encryption'] will always be a string and not None.
        self.assertTrue(wifi_expect.get(wifi_dict['name'], None) ==
                        wifi_dict['encryption'], msg)

        del wifi_expect_temp[wifi_dict['name']]

    # Error if wifi_expect_temp is not empty.
    self.assertFalse(wifi_expect_temp, 'The following networks '
                     'were not remembered: %s' % self.pformat(wifi_expect_temp))

  def testONCAddOpenWifi(self):
    """Test adding open network."""
    wifi_networks = {
        'ssid-none': '',
    }

    self._ReadONCFileAndSet('toplevel_wifi_open.onc')
    self._VerifyRememberedWifiNetworks(wifi_networks)

  def testONCAddWEPWifi(self):
    """Test adding WEP network."""
    wifi_networks = {
        'ssid-wep': 'WEP',
    }

    self._ReadONCFileAndSet('toplevel_wifi_wep_proxy.onc')
    self._VerifyRememberedWifiNetworks(wifi_networks)

  def testONCAddPSKWifi(self):
    """Test adding WPA network."""
    wifi_networks = {
        'ssid-wpa': 'WPA',
    }
    self._ReadONCFileAndSet('toplevel_wifi_wpa_psk.onc')
    self._VerifyRememberedWifiNetworks(wifi_networks)

  def testAddBacktoBackONC(self):
    """Test adding three different ONC files one after the other."""
    test_dict = {
      'toplevel_wifi_open.onc': { 'ssid-none': '' },
      'toplevel_wifi_wep_proxy.onc': { 'ssid-wep': 'WEP' },
      'toplevel_wifi_wpa_psk.onc': { 'ssid-wpa': 'WPA' },
    }

    for onc, wifi_networks in test_dict.iteritems():
      self._ReadONCFileAndSet(onc)
      self._VerifyRememberedWifiNetworks(wifi_networks)

  def testAddBacktoBackONC2(self):
    """Test adding three different ONC files one after the other.

    Due to inconsistent behaviors as addressed in crosbug.com/27862
    this test does not perform a network scan/verification between
    the setting of policies.
    """

    wifi_networks = {
      'ssid-wpa': 'WPA',
    }

    self._ReadONCFileAndSet('toplevel_wifi_open.onc')
    self._ReadONCFileAndSet('toplevel_wifi_wep_proxy.onc')
    self._ReadONCFileAndSet('toplevel_wifi_wpa_psk.onc')

    # Verify that only the most recent onc is updated.
    self._VerifyRememberedWifiNetworks(wifi_networks)

  def testAddONCWithUnknownFields(self):
    """Test adding an ONC file with unknown fields."""
    wifi_networks = {
        'ssid-none': '',
        'ssid-wpa': 'WPA'
    }

    self._ReadONCFileAndSet('toplevel_with_unknown_fields.onc')
    self._VerifyRememberedWifiNetworks(wifi_networks)


if __name__ == '__main__':
  pyauto_functional.Main()
