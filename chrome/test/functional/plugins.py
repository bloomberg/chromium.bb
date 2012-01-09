#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import re

import pyauto_functional  # Must be imported before pyauto
import pyauto
import test_utils


class PluginsTest(pyauto.PyUITest):
  """TestCase for Plugins."""

  def Debug(self):
    """Test method for experimentation.

    This method will not run automatically.
    """
    while True:
      raw_input('Interact with the browser and hit <enter> to list plugins...')
      self.pprint(self.GetPluginsInfo().Plugins())

  def setUp(self):
    pyauto.PyUITest.setUp(self)
    self._flash_plugin_type = 'Plug-in'
    if (self.IsChromeOS() and
        self.GetBrowserInfo()['properties']['branding'] == 'Google Chrome'):
      self._flash_plugin_type = 'Pepper Plugin'

  def _ObtainPluginsList(self):
    """Obtain a list of plugins for each platform.

    Produces warnings for plugins which are not installed on the machine.

    Returns:
      a list of 2-tuple, corresponding to the html file used for test and the
      name of the plugin
    """
    plugins = [('flash-clicktoplay.html', 'Shockwave Flash'),
               ('java_new.html', 'Java'),]    # common to all platforms
    if self.IsWin() or self.IsMac():
      plugins = plugins + [
         ('silverlight_new.html', 'Silverlight'),
         ('quicktime.html', 'QuickTime'),
         ('wmp_new.html', 'Windows Media'),
         ('real.html', 'RealPlayer'),
      ]

    out = []
    # Emit warnings for plugins that are not installed on the machine and
    # therefore cannot be tested.
    plugins_info = self.GetPluginsInfo()
    for fname, name in plugins:
      for a_plugin in plugins_info.Plugins():
        is_installed = False
        if re.search(name, a_plugin['name']):
          is_installed = True
          break
      if not is_installed:
        logging.warn('%s plugin is not installed and cannot be tested' % name)
      else:
        out.append((fname, name))
    return out

  def _GetPluginPID(self, plugin_name):
    """Fetch the pid of the plugin process with name |plugin_name|."""
    child_processes = self.GetBrowserInfo()['child_processes']
    plugin_type = 'Plug-in'
    if plugin_name == 'Shockwave Flash':
      plugin_type = self._flash_plugin_type
    for x in child_processes:
      if x['type'] == plugin_type and re.search(plugin_name, x['name']):
        return x['pid']
    return None

  def _TogglePlugin(self, plugin_name):
    """Toggle a plugin's status.

    If enabled, disable it.
    If disabled, enable it.
    """
    for plugin in self.GetPluginsInfo().Plugins():
      if re.search(plugin_name, plugin['name']):
        if plugin['enabled']:
          self.DisablePlugin(plugin['path'])
        else:
          self.EnablePlugin(plugin['path'])

  def _IsEnabled(self, plugin_name):
    """Checks if plugin is enabled."""
    for plugin in self.GetPluginsInfo().Plugins():
      if re.search(plugin_name, plugin['name']):
         return plugin['enabled']

  def _PluginNeedsAuthorization(self, plugin_name):
    # These plug-ins seek permission to run
    return plugin_name in ['Java', 'QuickTime', 'Windows Media', 'RealPlayer']

  def testKillAndReloadAllPlugins(self):
    """Verify plugin processes and check if they can reload after killing."""
    for fname, plugin_name in self._ObtainPluginsList():
      if plugin_name == 'Shockwave Flash':
        continue  # cannot reload file:// flash URL - crbug.com/47249
      url = self.GetFileURLForPath(
          os.path.join(self.DataDir(), 'plugin', fname))
      self.NavigateToURL(url)
      if self._PluginNeedsAuthorization(plugin_name):
        self.assertTrue(self.WaitForInfobarCount(1))
        self.PerformActionOnInfobar('accept', 0)
      self.WaitUntil(
          lambda: self._GetPluginPID(plugin_name) is not None )
      pid = self._GetPluginPID(plugin_name)
      self.assertTrue(pid, 'No plugin process for %s' % plugin_name)
      self.Kill(pid)
      self.assertTrue(self.WaitUntil(
          lambda: self._GetPluginPID(plugin_name) is None),
          msg='Expected %s plugin to die after killing' % plugin_name)
      self.GetBrowserWindow(0).GetTab(0).Reload()
      self.assertTrue(self.WaitUntil(
          lambda: self._GetPluginPID(plugin_name)),
          msg='No plugin process for %s after reloading' % plugin_name)
      # Verify that it's in fact a new process.
      self.assertNotEqual(pid, self._GetPluginPID(plugin_name),
                          'Did not get new pid for %s after reloading' %
                          plugin_name)

  def testDisableEnableAllPlugins(self):
    """Verify if all the plugins can be disabled and enabled.

    This is equivalent to testing the enable/disable functionality in
    chrome://plugins
    """
    # Flash files loaded too quickly after firing browser end up getting
    # downloaded, which seems to indicate that the plugin hasn't been
    # registered yet.
    # Hack to register Flash plugin on all platforms.  crbug.com/94123
    self.GetPluginsInfo()

    for fname, plugin_name in self._ObtainPluginsList():
      # Verify initial state
      self.assertTrue(self._IsEnabled(plugin_name),
                      '%s not enabled initially.' % plugin_name)
      # Disable
      self._TogglePlugin(plugin_name)
      self.assertFalse(self._IsEnabled(plugin_name))
      # Attempt to load a page that triggers the plugin and verify that it
      # indeed could not be loaded.
      url = self.GetFileURLForPath(
          os.path.join(self.DataDir(), 'plugin', fname))
      self.NavigateToURL(url)
      self.assertTrue(self.WaitUntil(
          lambda: self._GetPluginPID(plugin_name) is None ))
      self.assertFalse(self._GetPluginPID(plugin_name=plugin_name))
      if plugin_name == 'Shockwave Flash':
        continue  # cannot reload file:// flash URL - crbug.com/47249
      if plugin_name == 'Java':
        continue  # crbug.com/71223
      # Enable
      self._TogglePlugin(plugin_name)
      self.GetBrowserWindow(0).GetTab(0).Reload()
      if self._PluginNeedsAuthorization(plugin_name):
        self.assertTrue(self.WaitForInfobarCount(1))
        self.PerformActionOnInfobar('accept', 0)
      self.assertTrue(self.WaitUntil(
          lambda: self._GetPluginPID(plugin_name=plugin_name)))
      self.assertTrue(self._IsEnabled(plugin_name), plugin_name)

  def testBlockAllPlugins(self):
    """Verify that all the plugins can be blocked.
    Verifying by checking that flash plugin was blocked.
    """
    flash_url = self.GetFileURLForPath(os.path.join(
        self.DataDir(), 'plugin', 'flash-clicktoplay.html'))
    self.NavigateToURL(flash_url)
    flash_pid = self._GetPluginPID('Shockwave Flash')
    self.assertTrue(flash_pid, msg='No plugin process for Shockwave Flash')
    # Killing the flash process as it takes a while before the plugin
    # process is terminated even though there are no tabs using it.
    self.Kill(flash_pid)
    self.assertTrue(self.WaitUntil(
        lambda: self._GetPluginPID('Shockwave Flash') is None),
        msg='Expected Shockwave Flash plugin to die after killing')

    # Set the preference to block all plugins.
    self.SetPrefs(pyauto.kDefaultContentSettings, {'plugins': 2})

    self.GetBrowserWindow(0).GetTab(0).Reload()
    self.assertFalse(self._GetPluginPID('Shockwave Flash'),
                     msg='Plug-in not blocked.')

  def testAllowPluginException(self):
    """Verify that plugins can be allowed on a domain by adding
    an exception(s)."""
    # Set the preference to block all plugins.
    self.SetPrefs(pyauto.kDefaultContentSettings, {'plugins': 2})

    flash_url = self.GetFileURLForPath(os.path.join(
        self.DataDir(), 'plugin', 'flash-clicktoplay.html'))
    self.NavigateToURL(flash_url)
    # Check that plugins are blocked.
    self.assertFalse(self._GetPluginPID('Shockwave Flash'),
                     msg='Plug-in not blocked.')

    # Add an exception to allow plugins on hulu.com.
    self.SetPrefs(pyauto.kContentSettingsPatterns,
                 {'[*.]hulu.com,*': {'plugins': 1}})
    self.AppendTab(pyauto.GURL('http://www.hulu.com'))
    self.assertTrue(self._GetPluginPID('Shockwave Flash'),
                    msg='No plugin process for Shockwave Flash')

  def testBlockPluginException(self):
    """Verify that plugins can be blocked on a domain by adding
    an exception(s)."""
    url = 'http://www.hulu.com'
    self.NavigateToURL(url)
    # Wait until Shockwave Flash plugin process loads.
    self.assertTrue(self.WaitUntil(
        lambda: self._GetPluginPID('Shockwave Flash') is not None),
        msg='No plugin process for Shockwave Flash')
    self.Kill(self._GetPluginPID('Shockwave Flash'))
    self.assertTrue(self.WaitUntil(
        lambda: self._GetPluginPID('Shockwave Flash') is None),
        msg='Expected Shockwave Flash plugin to die after killing')

    # Add an exception to block plugins on localhost.
    self.SetPrefs(pyauto.kContentSettingsPatterns,
                 {'[*.]hulu.com,*': {'plugins': 2}})
    self.GetBrowserWindow(0).GetTab(0).Reload()
    self.assertFalse(self._GetPluginPID('Shockwave Flash'),
                     msg='Shockwave Flash Plug-in not blocked.')

  def testAboutPluginDetailInfo(self):
    """Verify about:plugin page shows plugin details."""
    self.NavigateToURL('chrome://plugins/')
    driver = self.NewWebDriver()
    detail_link = driver.find_element_by_id('details-link')
    detail_link.click()
    # Verify the detail info for Remote Viewer plugin show up.
    self.assertTrue(self.WaitUntil(lambda: len(driver.find_elements_by_xpath(
        '//*[@jscontent="name"][text()="Remoting Viewer"]' +
        '//ancestor::*[@class="plugin-text"]//a[text()="Disable"]')) == 1))

  def _ClearLocalDownloadState(self, path):
    """Prepare for downloading the given path.

    Clears the given path and the corresponding .crdownload, to prepare it to
    be downloaded.
    """
    if os.path.exists(path): os.remove(path)
    crdownload = path + '.crdownload'
    if os.path.exists(crdownload): os.remove(crdownload)

  def _GoPDFSiteAndVerify(self, pdf_name, tab_index=0, windex=0,
                          is_pdf_enabled=True):
    """Navigate to PDF file site and verify PDF viewer plugin.

    When PDF Viewer plugin is enabled, navigate the site should not trigger
    PDF file downloade and if PDF viewer plugin is disabled, PDF file should
    be viewable in browser.

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
      driver = self.NewWebDriver()
      self.assertTrue(self.WaitUntil(lambda: len(
          driver.find_elements_by_name('plugin')) == 1))

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
    detail_link.click()
    pdf_disable_path = '//*[@class="plugin-name"][text()="Chrome PDF Viewer"' \
        ']//ancestor::*[@class="plugin-text"]//a[text()="Disable"]'
    # Confirm Chrome PDF Viewer plugin is found.
    self.assertTrue(self.WaitUntil(lambda: len(driver.find_elements_by_xpath(
        pdf_disable_path)) == 1),
        msg='Failed to find Chrome PDF Viewer plugin')

    # Disable PDF viewer plugin in about:plugins.
    pdf_disable_link = driver.find_element_by_xpath(pdf_disable_path)

    # Need to sleep off 100ms for the detail-info expansion webkit transition.
    pdf_disable_link.click()
    self.assertFalse(self.WaitUntil(lambda:
        self._IsEnabled('Chrome PDF Viewer')))

    # Navigate to a PDF file and verify the pdf file is downloaded.
    self.OpenNewBrowserWindow(True)
    self._GoPDFSiteAndVerify('fw4.pdf', tab_index=0, windex=1,
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
    driver = self.NewWebDriver()
    pdf_enable_path = '//*[@class="plugin-name"][text()="Chrome PDF Viewer"]' \
        '//ancestor::*[@class="plugin-text"]//a[text()="Enable"]'
    self.assertTrue(self.WaitUntil(lambda: len(driver.find_elements_by_xpath(
         pdf_enable_path)) == 1))
    pdf_enable_link = driver.find_element_by_xpath(pdf_enable_path)
    pdf_enable_link.click()
    self.CloseBrowserWindow(0)
    self.OpenNewBrowserWindow(True)
    self._GoPDFSiteAndVerify('fw4.pdf', tab_index=0, windex=0,
                             is_pdf_enabled=True)


if __name__ == '__main__':
  pyauto_functional.Main()
