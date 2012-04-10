# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os

import ap_configurator
import selenium.common.exceptions


class DLinkAPConfigurator(ap_configurator.APConfigurator):
  """Derived class to control the DLink DAP-1522."""

  def __init__(self, pyauto_instance, admin_interface_url):
    super(DLinkAPConfigurator, self).__init__(pyauto_instance)
    # Override constants
    self.security_disabled = 'Disable Wireless Security (not recommended)'
    self.security_wep = 'WEP'
    self.security_wpapsk = 'WPA-Personal'
    self.security_wpa2psk = 'WPA-Personal'
    self.security_wpa8021x = 'WPA-Enterprise'
    self.security_wpa28021x = 'WPA2-Enterprise'

    self.admin_interface_url = admin_interface_url

  def _OpenLandingPage(self):
    self.pyauto_instance.NavigateToURL('http://%s/index.php' %
                                       self.admin_interface_url)
    page_name = os.path.basename(self.pyauto_instance.GetActiveTabURL().spec())
    if page_name == 'login.php' or page_name == 'index.php':
      try:
        self._wait.until(lambda _: self._driver.find_element_by_xpath(
            '//*[@name="login"]'))
      except selenium.common.exceptions.TimeoutException, e:
        # Maybe we were re-routes to the configuration page
        if (os.path.basename(self.pyauto_instance.GetActiveTabURL().spec()) ==
            'bsc_wizard.php'):
          return
        logging.exception('WebDriver exception: %s', str(e))
      login_button = self._driver.find_element_by_xpath('//*[@name="login"]')
      login_button.click()

  def _OpenConfigurationPage(self):
    self._OpenLandingPage()
    if (os.path.basename(self.pyauto_instance.GetActiveTabURL().spec()) !=
        'bsc_wizard.php'):
      self.fail(msg='Taken to an unknown page %s' %
                self.pyauto_instance.GetActiveTabURL().spec())

    # Else we are being logged in automatically to the landing page
    wlan = '//*[@name="wlan_wireless"]'
    try:
      self._wait.until(lambda _: self._driver.find_element_by_xpath(wlan))
    except selenium.common.exceptions.TimeoutException, e:
      logging.exception('WebDriver exception: %s', str(e))

    wlan_button = self._driver.find_element_by_xpath(wlan)
    wlan_button.click()
    # Wait for the main configuration page, look for the radio button
    try:
      self._wait.until(lambda _: self._driver.find_element_by_xpath(
          'id("enable")'))
    except selenium.common.exceptions.TimeoutException, e:
      logging.exception('Unable to find the radio button on the main landing '
                        'page.\nWebDriver exception: %s', str(e))

  def GetRouterName(self):
    return 'Router Name: DAP-1522; Class: DLinkAPConfigurator'

  def GetRouterShortName(self):
    return 'DAP-1522'

  def GetNumberOfPages(self):
    return 1

  def GetSupportedBands(self):
    return [{'band': self.band_2ghz,
             'channels': [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11]},
            {'band': self.band_5ghz,
             'channels': [26, 40, 44, 48, 149, 153, 157, 161, 165]}]

  def GetSupportedModes(self):
    return [{'band': self.band_2ghz,
             'modes': [self.mode_b, self.mode_g, self.mode_n,
                       self.mode_b | self.mode_g, self.mode_g | self.mode_n]},
            {'band': self.band_5ghz,
             'modes': [self.mode_a, self.mode_n, self.mode_a | self.mode_n]}]

  def NavigateToPage(self, page_number):
    # All settings are on the same page, so we always open the config page
    self._OpenConfigurationPage()
    return True

  def SavePage(self, page_number):
    # All settings are on the same page, we can ignore page_number
    button = self._driver.find_element_by_xpath('//input[@name="apply"]')
    button.click()
    # If we did not make changes so we are sent to the continue screen.
    continue_screen = True
    button_xpath = '//input[@name="bt"]'
    try:
      self._wait.until(lambda _:
                       self._driver.find_element_by_xpath(button_xpath))
    except selenium.common.exceptions.TimeoutException, e:
      continue_screen = False
    if continue_screen:
      button = self._driver.find_element_by_xpath(button_xpath)
      button.click()
    # We will be returned to the landing page when complete
    try:
      self._wait.until(lambda _:
                       self._driver.find_element_by_xpath('id("enable")'))
    except selenium.common.exceptions.TimeoutException, e:
      logging.exception('Unable to find the radio button on the main landing '
                        'page.\nWebDriver exception: %s', str(e))
      return False
    return True

  def SetMode(self, mode, band=None):
    # Mode overrides the band.  So if a band change is made after a mode change
    # it may make an incompatible pairing.
    self.AddItemToCommandList(self._SetMode, (mode, band), 1, 800)

  def _SetMode(self, mode, band=None):
    # Create the mode to popup item mapping
    mode_mapping = {self.mode_b: '802.11b Only', self.mode_g: '802.11g Only',
                    self.mode_n: '802.11n Only',
                    self.mode_b | self.mode_g: 'Mixed 802.11g and 802.11b',
                    self.mode_n | self.mode_g: 'Mixed 802.11n and 802.11g',
                    self.mode_n | self.mode_g | self.mode_b:
                    'Mixed 802.11n, 802.11g, and 802.11b',
                    self.mode_n | self.mode_g | self.mode_b:
                    'Mixed 802.11n, 802.11g, and 802.11b',
                    self.mode_a: '802.11a Only',
                    self.mode_n | self.mode_a: 'Mixed 802.11n and 802.11a'}
    band_value = self.band_2ghz
    if mode in mode_mapping.keys():
      popup_value = mode_mapping[mode]
      # If the mode contains 802.11a we use 5Ghz
      if mode & self.mode_a == self.mode_a:
        band_value = self.band_5ghz
      # If the mode is 802.11n mixed with 802.11a it must be 5Ghz
      elif mode & (self.mode_n | self.mode_a) == (self.mode_n | self.mode_a):
        band_value = self.band_5ghz
      # If the mode is 802.11n mixed with something other than 802.11a its 2Ghz
      elif mode & self.mode_n == self.mode_n and mode ^ self.mode_n > 0:
        band_value = self.band_2ghz
      # If the mode is 802.11n then we default to 5Ghz unless there is a band
      elif mode == self.mode_n:
        band_value = self.band_5ghz
        if band:
          band_value = band
    else:
      logging.exception('The mode selected %d is not supported by router %s.',
                        hex(mode), self.getRouterName())
    # Set the band first
    self._SetBand(band_value)
    popup_id = 'mode_80211_11g'
    if band_value == self.band_5ghz:
      popup_id = 'mode_80211_11a'
    self.SelectItemFromPopupByID(popup_value, popup_id)

  def SetRadio(self, enabled=True):
    # If we are enabling we are activating all other UI components, do it first.
    # Otherwise we are turning everything off so do it last.
    if enabled:
      weight = 1
    else:
      weight = 1000
    # This disables all UI so it should  be the last item to be changed
    self.AddItemToCommandList(self._SetRadio, (enabled,), 1, weight)

  def _SetRadio(self, enabled=True):
    # The radio checkbox for this router always has a value of 1.  So we need to
    # use other methods to determine if the radio is on or not.  Check if the
    # ssid textfield is disabled.
    ssid = self._driver.find_element_by_xpath('//input[@name="ssid"]')
    if ssid.get_attribute('disabled') == 'true':
      radio_enabled = False
    else:
      radio_enabled = True
    if radio_enabled == enabled:
      # Nothing to do
      return
    self.SetCheckBoxSelectedByID('enable', selected=False,
                                 wait_for_xpath='id("security_type_ap")')

  def SetSSID(self, ssid):
    # Can be done as long as it is enabled
    self.AddItemToCommandList(self._SetSSID, (ssid,), 1, 900)

  def _SetSSID(self, ssid):
    self._SetRadio(enabled=True)
    self.SetContentOfTextFieldByID(ssid, 'ssid')

  def SetChannel(self, channel):
    self.AddItemToCommandList(self._SetChannel, (channel,), 1, 900)

  def _SetChannel(self, channel):
    self._SetRadio(enabled=True)
    self.SetCheckBoxSelectedByID('autochann', selected=False)
    self.SelectItemFromPopupByID(str(channel), 'channel_g')

  # Experimental
  def GetBand(self):
    # The radio buttons do more than run a script that adjusts the possible
    # channels.  We will just check the channel to popup.
    self.setRadioSetting(enabled=True)
    xpath = ('id("channel_g")')
    self._OpenConfigurationPage()
    try:
      self._wait.until(lambda _: self._driver.find_element_by_xpath(xpath))
    except selenium.common.exceptions.TimeoutException, e:
      logging.exception('WebDriver exception: %s', str(e))
    element = self._driver.find_element_by_xpath(xpath)
    if element.find_elements_by_tag_name('option')[0].text == '1':
      return self.band_2ghz
    return self.band_5ghz

  def SetBand(self, band):
    if band != self.band_2GHz or band != self.band_5ghz:
      self.fail(msg='Invalid band sent %s' % band)
    self.AddItemToCommandList(self._SetBand, (band,), 1, 900)

  def _SetBand(self, band):
    self._SetRadio(enabled=True)
    if band == self.band_2ghz:
      int_value = 0
      wait_for_xpath = 'id("mode_80211_11g")'
    elif band == self.band_5ghz:
      int_value = 1
      wait_for_xpath = 'id("mode_80211_11a")'
    xpath = ('//*[contains(@class, "l_tb")]/input[@value="%d" and @name="band"]'
             % int_value)
    element = self._driver.find_element_by_xpath(xpath)
    element.click()
    try:
      self._wait.until(lambda _:
                       self._driver.find_element_by_xpath(wait_for_xpath))
    except selenium.common.exceptions.TimeoutException, e:
      logging.exception('The appropriate mode popup could not be found after '
                        'adjusting the band.  WebDriver exception: %s', str(e))

  def SetSecurityDisabled(self):
    self.AddItemToCommandList(self._SetSecurityDisabled, (), 1, 900)

  def _SetSecurityDisabled(self):
    self._SetRadio(enabled=True)
    self.SelectItemFromPopupByID(self.security_disabled, 'security_type_ap')

  def SetSecurityWEP(self, key_value, authentication):
    self.AddItemToCommandList(self._SetSecurityWEP, (key_value, authentication),
                              1, 900)

  def _SetSecurityWEP(self, key_value, authentication):
    self._SetRadio(enabled=True)
    self.SelectItemFromPopupByID(self.security_wep, 'security_type_ap',
                                 wait_for_xpath='id("auth_type")')
    self.SelectItemFromPopupByID(authentication, 'auth_type',
                                 wait_for_xpath='id("wep_key_value")')
    self.SetContentOfTextFieldByID(key_value, 'wep_key_value')
    self.SetContentOfTextFieldByID(key_value, 'verify_wep_key_value')

  def SetSecurityWPAPSK(self, shared_key, update_interval=1800):
    self.AddItemToCommandList(self._SetSecurityWPAPSK,
                              (shared_key, update_interval), 1, 900)

  def _SetSecurityWPAPSK(self, shared_key, update_interval=1800):
    self._SetRadio(enabled=True)
    self.SelectItemFromPopupByID(self.security_wpapsk, 'security_type_ap',
                                 wait_for_xpath='id("wpa_mode")')
    self.SelectItemFromPopupByID('WPA Only', 'wpa_mode',
                                 wait_for_xpath='id("grp_key_interval")')
    self.SetContentOfTextFieldByID(str(update_interval), 'grp_key_interval')
    self.SetContentOfTextFieldByID(shared_key, 'wpapsk1')

  def SetVisibility(self, visible=True):
    self.AddItemToCommandList(self._SetVisibility, (visible,), 1, 900)

  def _SetVisibility(self, visible=True):
    self._SetRadio(enabled=True)
    # value=0 is visible; value=1 is invisible
    int_value = 0
    if not visible:
      int_value = 1
    xpath = ('//*[contains(@class, "l_tb")]/input[@value="%d" '
             'and @name="visibility_status"]' % int_value)
    element = self._driver.find_element_by_xpath(xpath)
    element.click()

