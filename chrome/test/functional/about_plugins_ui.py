#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re

import pyauto_functional  # Must be imported before pyauto
import pyauto


class AboutPluginsTest(pyauto.PyUITest):
  """TestCase for chrome://plugins."""

  def Debug(self):
    """chrome://plugins test debug method.

    This method will not run automatically.
    """
    self.NavigateToURL('chrome://plugins/')
    driver = self.NewWebDriver()
    import pdb
    pdb.set_trace()

  def _ClearLocalDownloadState(self, path):
    """Prepare for downloading the given path.

    Clears the given path and the corresponding .crdownload, to prepare it to
    be downloaded.
    """
    if os.path.exists(path): os.remove(path)
    crdownload = path + '.crdownload'
    if os.path.exists(crdownload): os.remove(crdownload)

  def _IsEnabled(self, plugin_name):
    """Checks if plugin is enabled."""
    for plugin in self.GetPluginsInfo().Plugins():
      if re.search(plugin_name, plugin['name']):
        return plugin['enabled']

  def _GoPDFSiteAndVerify(self, pdf_name, driver, tab_index=0, windex=0,
                          is_pdf_enabled=True):
    """Navigate to PDF file site and verify PDF viewer plugin.

    When PDF Viewer plugin is disabled, navigating to the site should not
    trigger PDF file download and if PDF viewer plugin is enabled, PDF
    file should be viewable in browser.

    Args:
      pdf_name: the name of the PDF file.
                This should exist in the 'dangerous' directory.
      tab_index: tab index. Default 0.
      windex: window index. Default 0.
      is_pdf_enabled: the boolean flag check if PDF viewer plugin is enabled.
                      Default True.
    """
    file_url = self.GetHttpURLForDataPath('downloads', 'dangerous',
        'download-dangerous.html' + '?' + pdf_name)
    num_downloads = len(self.GetDownloadsInfo().Downloads())
    self.NavigateToURL(file_url, windex, tab_index)
    # It might take a while for the download to kick in, hold on until then.
    if is_pdf_enabled == False:
      self.assertTrue(self.WaitUntil(lambda:
          len(self.GetDownloadsInfo().Downloads()) == num_downloads + 1))
    else:
      self.assertTrue(self.WaitUntil(lambda: len(
          driver.find_elements_by_name('plugin')) == 1))

  def testAboutPluginDetailInfo(self):
    """Verify about:plugin page shows plugin details."""
    self.NavigateToURL('chrome://plugins/')
    driver = self.NewWebDriver()
    detail_link = driver.find_element_by_id('details-link')
    while not detail_link.is_displayed():
      pass
    detail_link.click()
    # Verify the detail info for Remote Viewer plugin show up.
    self.assertTrue(self.WaitUntil(lambda: len(driver.find_elements_by_xpath(
        '//*[@jscontent="path"][text()="internal-remoting-viewer"]'))))

  def testAboutPluginEnableAndDisablePDFPlugin(self):
    """Verify enable and disable plugins from about:plugins page."""
    self.NavigateToURL('chrome://plugins/')
    driver = self.NewWebDriver()
    # Override the animation for expanding detail info to make sure element
    # remain at the same location where web driver found it.
    override_animation_style_js = """
        style = document.createElement('style');
        style.innerHTML = "* { -webkit-transition: all 0s ease-in !important}";
        document.head.appendChild(style);
        """
    driver.execute_script(override_animation_style_js)

    # Click Details link to expand details.
    detail_link = driver.find_element_by_id('details-link')
    while not detail_link.is_displayed():
      pass
    detail_link.click()
    pdf_disable_path = '//*[@class="plugin-name"][text()="Chrome PDF Viewer"' \
        ']//ancestor::*[@class="plugin-text"]//a[text()="Disable"]'
    # Confirm Chrome PDF Viewer plugin is found.
    self.assertTrue(self.WaitUntil(lambda: len(driver.find_elements_by_xpath(
        pdf_disable_path)) == 1),
        msg='Failed to find Chrome PDF Viewer plugin')

    # Disable PDF viewer plugin in about:plugins.
    pdf_disable_link = driver.find_element_by_xpath(pdf_disable_path)
    pdf_disable_link.click()
    self.assertTrue(self.WaitUntil(lambda: not
        self._IsEnabled('Chrome PDF Viewer')))

    # Navigate to a PDF file and verify the pdf file is downloaded.
    self.OpenNewBrowserWindow(True)
    self._GoPDFSiteAndVerify('fw4.pdf', driver, tab_index=0, windex=1,
                             is_pdf_enabled=False)
    id = self.GetDownloadsInfo().Downloads()[0]['id']
    self.PerformActionOnDownload(id,
                                 'save_dangerous_download',
                                 window_index=1)
    self.WaitForAllDownloadsToComplete(windex=1)

    pdf_download = self.GetDownloadsInfo(1).Downloads()
    # Verify that download info exists in the correct profile.
    self.assertEqual(len(pdf_download), 1)
    download_pkg = os.path.join(self.GetDownloadDirectory().value(),
                                'fw4.pdf')
    self._ClearLocalDownloadState(download_pkg)

    # Navigate to plugins settings again and re-enable PDF viewer plugin.
    self.NavigateToURL('chrome://plugins/')
    pdf_enable_path = '//*[@class="plugin-name"][text()="Chrome PDF Viewer"]' \
        '//ancestor::*[@class="plugin-text"]//a[text()="Enable"]'
    self.assertTrue(self.WaitUntil(lambda: len(driver.find_elements_by_xpath(
         pdf_enable_path)) == 1))
    pdf_enable_link = driver.find_element_by_xpath(pdf_enable_path)
    pdf_enable_link.click()
    self.CloseBrowserWindow(0)
    self.OpenNewBrowserWindow(True)
    driver = self.NewWebDriver()
    self._GoPDFSiteAndVerify('fw4.pdf', driver, tab_index=0, windex=0,
                             is_pdf_enabled=True)


if __name__ == '__main__':
  pyauto_functional.Main()