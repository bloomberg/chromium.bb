# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import ap_configurator_factory
import dlink_ap_configurator
import linksys_ap_configurator

import pyauto_ap_configurator  # must preceed pyauto
import pyauto


class ConfiguratorTest(pyauto.PyUITest):
  """This test needs to be run against the UI interface.

  The purpose of this test is to act as a basic acceptance test when developing
  a new AP configurator class.  Use this to make sure all core functionality is
  implemented.

  This test does not verify that everything works.

  """

  def setUp(self):
    pyauto.PyUITest.setUp(self)
    factory = ap_configurator_factory.APConfiguratorFactory(self)
    # Set self.ap to the one you want to test against.
    self.ap = factory.GetAPConfiguratorByShortName('DAP-1522')

  def testMakeNoChanges(self):
    """Test saving with no changes doesn't throw an error."""
    # Set to a known state.
    self.ap.SetRadio(True)
    self.ap.ApplySettings()
    # Set the same setting again.
    self.ap.SetRadio(True)
    self.ap.ApplySettings()

  def testRadio(self):
    """Test we can adjust the radio setting."""
    self.ap.SetRadio(True)
    self.ap.ApplySettings()
    self.ap.SetRadio(False)
    self.ap.ApplySettings()

  def testChannel(self):
    """Test adjusting the channel."""
    self.ap.SetRadio(4)
    self.ap.ApplySettings()

  def testVisibility(self):
    """Test adjusting the visibility."""
    self.ap.SetVisibility(False)
    self.ap.ApplySettings()
    self.ap.SetVisibility(True)
    self.ap.ApplySettings()

  def testSSID(self):
    """Test setting the SSID."""
    self.ap.SetSSID('AP-automated-ssid')
    self.ap.ApplySettings()

  def testSecurityWEP(self):
    """Test configuring WEP security."""
    self.ap.SetSecurityWEP('45678', self.ap.wep_authentication_open)
    self.ap.ApplySettings()
    self.ap.SetSecurityWEP('90123', self.ap.wep_authentication_shared)
    self.ap.ApplySettings()

  def testPrioritySets(self):
    """Test that commands are run in the right priority."""
    self.ap.SetRadio(False)
    self.ap.SetVisibility(True)
    self.ap.SetSSID('priority_test')
    self.ap.ApplySettings()

  def testSecurityAndGeneralSettings(self):
    """Test updating settings that are general and security related."""
    self.ap.SetRadio(False)
    self.ap.SetVisibility(True)
    self.ap.SetSecurityWEP('88888', self.ap.wep_authentication_open)
    self.ap.SetSSID('sec&gen_test')
    self.ap.ApplySettings()

  def testModes(self):
    """Tests switching modes."""
    modes_info = self.ap.GetSupportedModes()
    self.assertFalse(not modes_info,
                     msg='Returned an invalid mode list.  Is this method'
                     ' implemented?')
    for band_modes in modes_info:
      for mode in band_modes['modes']:
        self.ap.SetMode(mode)
        self.ap.ApplySettings()

  def testModesWithBand(self):
    """Tests switching modes that support adjusting the band."""
    # Check if we support self.kModeN across multiple bands
    modes_info = self.ap.GetSupportedModes()
    n_bands = []
    for band_modes in modes_info:
      if self.ap.mode_n in band_modes['modes']:
        n_bands.append(band_modes['band'])
    if len(n_bands) > 1:
      for n_band in n_bands:
        self.ap.SetMode(self.ap.mode_n, band=n_band)
        self.ap.ApplySettings()

  def testFastCycleSecurity(self):
    """Mini stress for changing security settings rapidly."""
    self.ap.SetRadio(True)
    self.ap.SetSecurityWEP('77777', self.ap.wep_authentication_open)
    self.ap.SetSecurityDisabled()
    self.ap.SetSecurityWPAPSK('qwertyuiolkjhgfsdfg')
    self.ap.ApplySettings()

  def testCycleSecurity(self):
    """Test switching between different security settings."""
    self.ap.SetRadio(True)
    self.ap.SetSecurityWEP('77777', self.ap.wep_authentication_open)
    self.ap.ApplySettings()
    self.ap.SetSecurityDisabled()
    self.ap.ApplySettings()
    self.ap.SetSecurityWPAPSK('qwertyuiolkjhgfsdfg')
    self.ap.ApplySettings()

  def testActionsWhenRadioDisabled(self):
    """Test making changes when the radio is diabled."""
    self.ap.SetRadio(False)
    self.ap.ApplySettings()
    self.ap.SetSecurityWEP('77777', self.ap.wep_authentication_open)
    self.ap.SetRadio(False)
    self.ap.ApplySettings()


if __name__ == '__main__':
  pyauto_ap_configurator.Main()
