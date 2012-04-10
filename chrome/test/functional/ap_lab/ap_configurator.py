# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import logging
import os

import pyauto_ap_configurator
import pyauto

import selenium.common.exceptions
from selenium.webdriver.support.ui import WebDriverWait


class APConfigurator(object):
  """Base class for objects to configure access points using webdriver."""

  def __init__(self, pyauto_instance):
    self.pyauto_instance = pyauto_instance
    self._driver = pyauto_instance.NewWebDriver()
    # Any call to wait.until() will raise an exception if the timeout is hit.
    self._wait = WebDriverWait(self._driver, timeout=5)

    # Possible bands
    self.band_2ghz = '2.4GHz'
    self.band_5ghz = '5GHz'

    # Possible modes
    self.mode_a = 0x0001
    self.mode_b = 0x0010
    self.mode_g = 0x0100
    self.mode_n = 0x1000

    # Possible security settings
    self.security_disabled = 'Disabled'
    self.security_wep = 'WEP'
    self.security_wpawpsk = 'WPA-Personal'
    self.security_wpa2wpsk = 'WPA2-Personal'
    self.security_wpa8021x = 'WPA-Enterprise'
    self.security_wpa28021x = 'WPA2-Enterprise'

    self.wep_authentication_open = 'Open'
    self.wep_authentication_shared = 'Shared Key'

    self._command_list = []

  def _WaitForObjectByXPath(self, xpath):
    """Waits for an object to appear."""
    try:
      self._wait.until(lambda _: self._driver.find_element_by_xpath(xpath))
    except selenium.common.exceptions.TimeoutException, e:
      logging.exception('Unable to find the wait for object by xpath: %s\n'
                        'WebDriver exception: %s', xpath, str(e))

  def SelectItemFromPopupByID(self, item, element_id, wait_for_xpath=None):
    """Selects an item from a popup, by passing the element ID.

    Args:
      item: the item to select from the popup
      element_id: the html ID of the item
      wait_for_xpath: an item to wait for before returning
    """
    xpath = 'id("%s")' % element_id
    self.SelectItemFromPopupByXPath(item, xpath, wait_for_xpath)

  def SelectItemFromPopupByXPath(self, item, xpath, wait_for_xpath=None):
    """Selects an item from a popup, by passing the xpath of the popup.

    Args:
      item: the item to select from the popup
      xpath: the xpath of the popup
      wait_for_xpath: an item to wait for before returning
    """
    popup = self._driver.find_element_by_xpath(xpath)
    for option in popup.find_elements_by_tag_name('option'):
      if option.text == item:
        option.click()
        break
    if wait_for_xpath: self._WaitForObjectByXPath(wait_for_xpath)

  def SetContentOfTextFieldByID(self, content, text_field_id,
                                wait_for_xpath=None):
    """Sets the content of a textfield, by passing the element ID.

    Args:
      content: the content to apply to the textfield
      text_field_id: the html ID of the textfield
      wait_for_xpath: an item to wait for before returning
    """
    xpath = 'id("%s")' % text_field_id
    self.SetConentsOfTextFieldByXPath(content, xpath, wait_for_xpath)

  def SetConentsOfTextFieldByXPath(self, content, xpath, wait_for_xpath=None):
    """Sets the content of a textfield, by passing the xpath.

    Args:
      content: the content to apply to the textfield
      xpath: the xpath of the textfield
      wait_for_xpath: an item to wait for before returning
    """
     # When we can get the value we know the text field is ready.
    text_field = self._driver.find_element_by_xpath(xpath)
    try:
      self._wait.until(lambda _: text_field.get_attribute('value'))
    except selenium.common.exceptions.TimeoutException, e:
      logging.exception('Unable to obtain the value of the text field %s.\n'
                        'WebDriver exception: %s', wait_for_xpath, str(e))
    text_field = self._driver.find_element_by_xpath(xpath)
    text_field.clear()
    text_field.send_keys(content)
    if wait_for_xpath: self._WaitForObjectByXPath(wait_for_xpath)

  def SetCheckBoxSelectedByID(self, check_box_id, selected=True,
                              wait_for_xpath=None):
    """Sets the state of a checkbox, by passing the ID.

    Args:
      check_box_id: the html id of the checkbox
      selected: True to enable the checkbox; False otherwise
      wait_for_xpath: an item to wait for before returning
    """
    xpath = 'id("%s")' % check_box_id
    self.SetCheckBoxSelectedByXPath(xpath, selected, wait_for_xpath)

  def SetCheckBoxSelectedByXPath(self, xpath, selected=True,
                                 wait_for_xpath=None):
    """Sets the state of a checkbox, by passing the xpath.

    Args:
      xpath: the xpath of the checkbox
      selected: True to enable the checkbox; False otherwise
      wait_for_xpath: an item to wait for before returning
    """
    check_box = self._driver.find_element_by_xpath(xpath)
    value = check_box.get_attribute('value')
    if (value == '1' and not selected) or (value == '0' and selected):
      check_box.click()
    if wait_for_xpath: self._WaitForObjectByXPath(wait_for_xpath)

  def AddItemToCommandList(self, method, args, page, priority):
    """Adds commands to be executed against the AP web UI.

    Args:
      method: the method to run
      args: the arguments for the method you want executed
      page: the page on the web ui where the method should be run against
      priority: the priority of the method
    """
    self._command_list.append({'method': method,
                               'args': copy.copy(args),
                               'page': page,
                               'priority': priority})

  def GetRouterName(self):
    """Returns a string to describe the router.

    Note: The derived class must implement this method.
    """
    raise NotImplementedError

  def GetRouterShortName(self):
    """Returns a short string to describe the router.

    Note: The derived class must implement this method.
    """
    raise NotImplementedError

  def GetNumberOfPages(self):
    """Returns the number of web pages used to configure the router.

    Note: This is used internally by applySettings, and this method must be
          implemented by the derived class.
    """
    raise NotImplementedError

  def GetSupportedBands(self):
    """Returns a list of dictionaries describing the supported bands.

    Example: returned is a dictionary of band and a list of channels.  The band
             object returned must be one of those defined in the __init___ of
             this class.

    supported_bands = [{'band' : self.band_2GHz,
                        'channels' : [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11]},
                       {'band' : self.band_5ghz,
                        'channels' : [26, 40, 44, 48, 149, 153, 157, 161, 165]}]

    Returns:
      A list of dictionaries as described above

    Note: The derived class must implement this method.
    """
    raise NotImplementedError

  def GetSupportedModes(self):
    """Returns a list of dictionaries describing the supported modes.

    Example: returned is a dictionary of band and a list of modess.  The band
             and modes objects returned must be one of those defined in the
             __init___ of this class.

    supported_modes = [{'band' : self.band_2GHz,
                        'modes' : [mode_b, mode_b | mode_g]},
                       {'band' : self.band_5ghz,
                        'modes' : [mode_a, mode_n, mode_a | mode_n]}]

    Returns:
      A list of dictionaries as described above

    Note: The derived class must implement this method.
    """
    raise NotImplementedError

  def NavigateToPage(self, page_number):
    """Navigates to the page corresponding to the given page number.

    This method performs the translation between a page number and a url to
    load.  This is used internally by applySettings.

    Args:
      page_number: Page number of the page to load

    Returns:
      True if navigation is successful; False otherwise.

    Note: The derived class must implement this method.
    """
    raise NotImplementedError

  def SavePage(self, page_number):
    """Saves the given page.

    Args:
      page_number: Page number of the page to save.

    Returns:
      True if navigation is successful; False otherwise.

    Note: The derived class must implement this method.
    """
    raise NotImplementedError

  def SetMode(self, mode, band=None):
    """Sets the mode.

    Args:
      mode: must be one of the modes listed in __init__()
      band: the band to select

    Note: The derived class must implement this method
    """
    raise NotImplementedError

  def SetRadio(self, enabled=True):
    """Turns the radio on and off.

    Args:
      enabled: True to turn on the radio; False otherwise

    Note: The derived class must implement this method.
    """
    raise NotImplementedError

  def SetSSID(self, ssid):
    """Sets the SSID of the wireless network.

    Args:
      ssid: Name of the wireless network

    Note: The derived class must implement this method.
    """
    raise NotImplementedError

  def SetChannel(self, channel):
    """Sets the channel of the wireless network.

    Args:
      channel: Integer value of the channel

    Note: The derived class must implement this method.
    """
    raise NotImplementedError

  def SetBand(self, band):
    """Sets the band of the wireless network.

    Currently there are only two possible values for band 2kGHz and 5kGHz.

    Args:
      band: Constant describing the band type

    Note: The derived class must implement this method.
    """
    raise NotImplementedError

  def SetSecurityDisabled(self):
    """Disables the security of the wireless network.

    Note: The derived class must implement this method.
    """
    raise NotImplementedError

  def SetSecurityWEP(self, key_value, authentication):
    """Enabled WEP security for the wireless network.

    Args:
      key_value: encryption key to use
      authentication: one of two supported authentication types:
                      wep_authentication_open or wep_authentication_shared

    Note: The derived class must implement this method.
    """
    raise NotImplementedError

  def SetSecurityWPAPSK(self, shared_key, update_interval=1800):
    """Enabled WPA using a private security key for the wireless network.

    Args:
      shared_key: shared encryption key to use
      update_interval: number of seconds to wait before updating

    Note: The derived class must implement this method.
    """
    raise NotImplementedError

  def SetVisibility(self, visible=True):
    """Set the visibility of the wireless network.

    Args:
      visible: True for visible; False otherwise

    Note: The derived class must implement this method.
    """
    raise NotImplementedError

  def ApplySettings(self):
    """Apply all settings to the access point."""
    # Pull items by page and then sort
    if self.GetNumberOfPages() == -1:
      self.fail(msg='Number of pages is not set.')
    page_range = range(1, self.GetNumberOfPages() + 1)
    for i in page_range:
      page_commands = []
      for command in self._command_list:
        if command['page'] == i:
          page_commands.append(command)
      # Sort the commands in this page by priority
      sorted_page_commands = sorted(page_commands, key=lambda k: k['priority'])
      if sorted_page_commands and self.NavigateToPage(i):
        for command in sorted_page_commands:
          command['method'](*command['args'])
        self.SavePage(i)
    self._command_list = []

