# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os

import ap_configurator
import selenium.common.exceptions


class LinksysAPConfigurator(ap_configurator.APConfigurator):

  def __init__(self, pyauto_instance, admin_interface_url):
    super(LinksysAPConfigurator, self).__init__(pyauto_instance)
    # Override constants
    self.security_disabled = 'Disabled'
    self.security_wep = 'WEP'
    self.security_wpapsk = 'WPA Personal'
    self.security_wpa2psk = 'WPA2 Personal'
    self.security_wpa8021x = 'WPA Enterprise'
    self.security_wpa28021x = 'WPA2 Enterprise'

    self.admin_interface_url = admin_interface_url

  def GetRouterName(self):
    return 'Router Name: WRT54G2; Class: LinksysAPConfigurator'

  def GetRouterShortName(self):
    return 'WRT54G2'

  def GetNumberOfPages(self):
    return 2

  def GetSupportedBands(self):
    return [{'band': self.k2GHz,
             'channels': [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11]}]

  def GetSupportedModes(self):
    return [{'band': self.band_2ghz,
             'modes': [self.mode_b, self.mode_g, self.mode_b | self.mode_g]}]

  def NavigateToPage(self, page_number):
    if page_number == 1:
      self.pyauto_instance.NavigateToURL('http://%s/wireless.htm'
                                         % self.admin_interface_url)
    elif page_number == 2:
      self.pyauto_instance.NavigateToURL('http://%s/WSecurity.htm'
                                         % self.admin_interface_url)
    else:
      logging.exception('Invalid page number passed.  Number of pages %d, '
                        'page value sent was %d', self.GetNumberOfPages(),
                        page_number)
      return False
    return True

  def SavePage(self, page_number):
    try:
      self._wait.until(lambda _:
                       self._driver.find_element_by_xpath('id("divBT1")'))
    except selenium.common.exceptions.TimeoutException, e:
      logging.exception('Unable to locate the save button.\nWebDriver'
                        ' exception: %s', str(e))
      return False
    button = self._driver.find_element_by_xpath('id("divBT1")')
    button.click()
    # Wait for the continue button
    continue_xpath = '//input[@value="Continue" and @type="button"]'
    try:
      self._wait.until(lambda _:
                       self._driver.find_element_by_xpath(continue_xpath))
    except selenium.common.exceptions.TimeoutException, e:
      logging.exception('Unable to location the continue button, save probably'
                        ' failed.\nWebDriver exception: %s', str(e))
      return False
    button = self._driver.find_element_by_xpath(continue_xpath)
    button.click()
    return True

  def SetMode(self, mode, band=None):
    self.AddItemToCommandList(self._SetMode, (mode,), 1, 900)

  def _SetMode(self, mode):
    # Different bands are not supported so we ignore.
    # Create the mode to popup item mapping
    mode_mapping = {self.mode_b: 'B-Only', self.mode_g: 'G-Only',
                    self.mode_b | self.mode_g: 'Mixed'}
    mode_name = ''
    if mode in mode_mapping.keys():
      mode_name = mode_mapping[mode]
    else:
      logging.exception('The mode selected %d is not supported by router %s.',
                        hex(mode), self.getRouterName())
    xpath = ('//select[@onchange="SelWL()" and @name="Mode"]')
    self.SelectItemFromPopupByXPath(mode_name, xpath)

  def SetRadio(self, enabled=True):
    # If we are enabling we are activating all other UI components, do it
    # first.  Otherwise we are turning everything off so do it last.
    if enabled:
      weight = 1
    else:
      weight = 1000
    self.AddItemToCommandList(self._SetRadio, (enabled,), 1, weight)

  def _SetRadio(self, enabled=True):
    xpath = ('//select[@onchange="SelWL()" and @name="Mode"]')
    # To turn off we pick disabled, to turn on we set to G
    if not enabled:
      setting = 'Disabled'
    else:
      setting = 'G-Only'
    self.SelectItemFromPopupByXPath(setting, xpath)

  def SetSSID(self, ssid):
    self.AddItemToCommandList(self._SetSSID, (ssid,), 1, 900)

  def _SetSSID(self, ssid):
    self._SetRadio(enabled=True)
    xpath = ('//input[@maxlength="32" and @name="SSID"]')
    self.SetConentsOfTextFieldByXPath(ssid, xpath)

  def SetChannel(self, channel):
    self.AddItemToCommandList(self._SetChannel, (channel,), 1, 900)

  def _SetChannel(self, channel):
    self._SetRadio(enabled=True)
    channel_choices = ['1 - 2.412GHz', '2 - 2.417GHz', '3 - 2.422GHz',
                       '4 - 2.427GHz', '5 - 2.432GHz', '6 - 2.437GHz',
                       '7 - 2.442GHz', '8 - 2.447GHz', '9 - 2.452GHz',
                       '10 - 2.457GHz', '11 - 2.462GHz']
    xpath = ('//select[@onfocus="check_action(this,0)" and @name="Freq"]')
    self.SelectItemFromPopupByXPath(channel_choices[channel - 1], xpath)

  def SetBand(self, band):
    return None

  def SetSecurityDisabled(self):
    self.AddItemToCommandList(self._SetSecurityDisabled, (), 2, 1000)

  def _SetSecurityDisabled(self):
    xpath = ('//select[@name="SecurityMode"]')
    self.SelectItemFromPopupByXPath(self.security_disabled, xpath)

  def SetSecurityWEP(self, key_value, authentication):
    self.AddItemToCommandList(self._SetSecurityWEP, (key_value, authentication),
                              2, 1000)

  def _SetSecurityWEP(self, key_value, authentication):
    logging.info('This router %s does not support WEP authentication type: %s',
                 self.GetRouterName(), authentication)
    popup = '//select[@name="SecurityMode"]'
    try:
      self._wait.until(lambda _: self._driver.find_element_by_xpath(popup))
    except selenium.common.exceptions.TimeoutException, e:
      logging.exception('Unable to find the security mode pop up.\nWebDriver '
                        ' exception: %s', str(e))
    text_field = ('//input[@name="wl_passphrase"]')
    self.SelectItemFromPopupByXPath(self.security_wep, popup,
                                    wait_for_xpath=text_field)
    self.SetConentsOfTextFieldByXPath(key_value, text_field)
    button = self._driver.find_element_by_xpath('//input[@value="Generate"]')
    button.click()

  def SetSecurityWPAPSK(self, shared_key, update_interval=1800):
    self.AddItemToCommandList(self._SetSecurityWPAPSK,
                              (shared_key, update_interval), 1, 900)

  def _SetSecurityWPAPSK(self, shared_key, update_interval=1800):
    popup = '//select[@name="SecurityMode"]'
    try:
      self._wait.until(lambda _: self._driver.find_element_by_xpath(popup))
    except selenium.common.exceptions.TimeoutException, e:
      logging.exception('Unable to find the security mode pop up. WebDriver '
                        ' exception: %s', str(e))
    key_field = '//input[@name="PassPhrase"]'
    self.SelectItemFromPopupByXPath(self.security_wpapsk, popup,
                                    wait_for_xpath=key_field)
    self.SetConentsOfTextFieldByXPath(shared_key, key_field)
    interval_field = ('//input[@name="GkuInterval"]')
    self.SetConentsOfTextFieldByXPath(str(update_interval), interval_field)

  def SetVisibility(self, visible=True):
    self.AddItemToCommandList(self._SetVisibility, (visible,), 1, 900)

  def _SetVisibility(self, visible=True):
    self._SetRadio(enabled=True)
    # value=1 is visible; value=0 is invisible
    int_value = 1
    if not visible:
      int_value = 0
    xpath = ('//input[@value="%d" and @name="wl_closed"]' % int_value)
    element = self._driver.find_element_by_xpath(xpath)
    element.click()

