#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re

import pyauto_functional  # Must be imported before pyauto
import pyauto
import pyauto_utils


class AboutPluginsUITest(pyauto.PyUITest):
  """Testcase for chrome://plugins UI."""

  def testAboutPluginDetailInfo(self):
    """Verify chrome://plugins page shows plugin details."""
    self.NavigateToURL('chrome://plugins/')
    driver = self.NewWebDriver()
    detail_link = driver.find_element_by_id('details-link')
    self.assertTrue(self.WaitUntil(lambda: detail_link.is_displayed()),
        msg='Details link could not be found.')
    detail_link.click()
    # Verify that detail info for Remote Viewer plugin shows up.
    # Remote Viewer plugin is expected to be present on all platforms.
    self.assertTrue(self.WaitUntil(lambda: len(driver.find_elements_by_xpath(
        '//*[@jscontent="path"][text()="internal-remoting-viewer"]'))))


class ChromeAboutPluginsUITest(pyauto.PyUITest):
  """Testcase for official build only plugins in chrome://plugins UI."""

  def Debug(self):
    """chrome://plugins test debug method.

    This method will not run automatically.
    """
    self.NavigateToURL('chrome://plugins/')
    driver = self.NewWebDriver()
    import pdb
    pdb.set_trace()

  def _IsEnabled(self, driver, plugin_name):
    """Checks if plugin is enabled.

    Args:
      driver: A Chrome driver object.
      plugin_name: Plugin name to verify.

    Returns:
      True, if plugin is enabled, or False otherwise.
    """
    check_plugin_enabled_js = 'return navigator.plugins["%s"] != undefined' % \
        plugin_name
    return driver.execute_script(check_plugin_enabled_js)

  def _ExpandDetailInfoLink(self, driver):
    """Expand detail info link.

    Args:
      driver: A Chrome driver object.
    """
    detail_link = driver.find_element_by_id('details-link')
    self.assertTrue(self.WaitUntil(lambda: detail_link.is_displayed()),
        msg='Details link could not be found.')
    detail_link.click()

  def _OverridePluginPageAnimation(self, driver):
    """Override the animation for expanding detail info to make sure element
    remain at the same location where web driver found it.

    Args:
      driver: A Chrome driver object.
    """
    override_animation_style_js = """
        style = document.createElement('style');
        style.innerHTML = "* { -webkit-transition: all 0s ease-in !important}";
        document.head.appendChild(style);
        """
    driver.execute_script(override_animation_style_js)

  def testAboutPluginEnableAndDisablePDFPlugin(self):
    """Verify enable and disable pdf plugins from about:plugins page."""
    self.NavigateToURL('chrome://plugins/')
    driver = self.NewWebDriver()

    self._OverridePluginPageAnimation(driver)
    self._ExpandDetailInfoLink(driver)

    pdf_disable_path = '//*[@class="plugin-name"][text()="Chrome PDF Viewer"' \
        ']//ancestor::*[@class="plugin-text"]//a[text()="Disable"]'
    pdf_enable_path = '//*[@class="plugin-name"][text()="Chrome PDF Viewer"' \
        ']//ancestor::*[@class="plugin-text"]//a[text()="Enable"]'

    # Confirm Chrome PDF Viewer plugin is found and find disable PDF link.
    pdf_disable_link = pyauto_utils.WaitForDomElement(self, driver,
                                                      pdf_disable_path)

    # Disable PDF viewer plugin in about:plugins.
    pdf_disable_link.click()
    self.assertTrue(self.WaitUntil(lambda: not
        self._IsEnabled(driver, 'Chrome PDF Viewer')))

    # Re-enable PDF viewer plugin.
    pdf_enable_link = driver.find_element_by_xpath(pdf_enable_path)
    pdf_enable_link.click()
    self.assertTrue(self.WaitUntil(lambda:
        self._IsEnabled(driver, 'Chrome PDF Viewer')))

  def testEnableAndDisableFlashPlugin(self):
    """Verify enable and disable flash plugins from about:plugins page."""
    self.NavigateToURL('chrome://plugins/')
    driver = self.NewWebDriver()

    self._OverridePluginPageAnimation(driver)
    self._ExpandDetailInfoLink(driver)
    flash_plugins_elem = driver.find_element_by_xpath(
        '//*[@jscontent="name"][text()="Flash"]//ancestor' \
        '::*[@class="plugin-text"]')

    # Disable flash plugin from flash detail info.
    flash_disable_link = flash_plugins_elem.find_element_by_xpath(
        './/a[text()="Disable"]')
    flash_disable_link.click()
    self.assertTrue(self.WaitUntil(lambda: not
        self._IsEnabled(driver, 'Shockwave Flash')))

    # Re-enable Flash plugin from flash detail info.
    flash_enable_link = flash_plugins_elem.find_element_by_xpath(
        './/a[text()="Enable"]')
    flash_enable_link.click()
    self.assertTrue(self.WaitUntil(lambda:
        self._IsEnabled(driver, 'Shockwave Flash')))


if __name__ == '__main__':
  pyauto_functional.Main()
