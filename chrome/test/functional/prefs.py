#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import shutil
import sys

import pyauto_functional  # Must be imported before pyauto
import pyauto
import test_utils


class PrefsTest(pyauto.PyUITest):
  """TestCase for Preferences."""

  def Debug(self):
    """Test method for experimentation.

    This method will not run automatically.
    """
    while True:
      raw_input('Interact with the browser and hit <enter> to dump prefs... ')
      self.pprint(self.GetPrefsInfo().Prefs())

  def testSessionRestore(self):
    """Test session restore preference."""

    pref_url = 'chrome://settings/browser'
    url1 = 'http://www.google.com/'
    url2 = 'http://news.google.com/'

    self.NavigateToURL(pref_url)
    # Set pref to restore session on startup.
    driver = self.NewWebDriver()
    restore_elem = driver.find_element_by_xpath(
        '//input[@metric="Options_Startup_LastSession"]')
    restore_elem.click()
    self.assertTrue(restore_elem.is_selected())
    self.RestartBrowser(clear_profile=False)
    self.NavigateToURL(url1)
    self.AppendTab(pyauto.GURL(url2))
    num_tabs = self.GetTabCount()
    self.RestartBrowser(clear_profile=False)
    # Verify tabs are properly restored.
    self.assertEqual(self.GetPrefsInfo().Prefs(pyauto.kRestoreOnStartup), 1)
    self.assertEqual(num_tabs, self.GetTabCount())
    self.ActivateTab(0)
    self.assertEqual(url1, self.GetActiveTabURL().spec())
    self.ActivateTab(1)
    self.assertEqual(url2, self.GetActiveTabURL().spec())
    # Verify session restore option is still selected.
    self.NavigateToURL(pref_url, 0, 0)
    driver = self.NewWebDriver()
    self.assertTrue(driver.find_element_by_xpath(
        '//input[@metric="Options_Startup_LastSession"]').is_selected())

  def testNavigationStateOnSessionRestore(self):
    """Verify navigation state is preserved on session restore."""
    urls = ('http://www.google.com/',
            'http://news.google.com/',
            'http://dev.chromium.org/',)
    for url in urls:
      self.NavigateToURL(url)
    tab = self.GetBrowserWindow(0).GetTab(0)
    tab.GoBack()
    self.assertEqual(self.GetActiveTabURL().spec(), urls[-2])
    self.SetPrefs(pyauto.kRestoreOnStartup, 1)  # set pref to restore session
    self.RestartBrowser(clear_profile=False)
    # Verify that navigation state (forward/back state) is restored.
    tab = self.GetBrowserWindow(0).GetTab(0)
    tab.GoBack()
    self.assertEqual(self.GetActiveTabURL().spec(), urls[0])
    for i in (-2, -1):
      tab.GoForward()
      self.assertEqual(self.GetActiveTabURL().spec(), urls[i])

  def testSessionRestoreURLs(self):
    """Verify restore URLs preference."""
    url1 = self.GetFileURLForPath(os.path.join(self.DataDir(), 'title1.html'))
    url2 = self.GetFileURLForPath(os.path.join(self.DataDir(), 'title2.html'))
    # Set pref to restore given URLs on startup
    self.SetPrefs(pyauto.kRestoreOnStartup, 4)  # 4 is for restoring URLs
    self.SetPrefs(pyauto.kURLsToRestoreOnStartup, [url1, url2])
    self.RestartBrowser(clear_profile=False)
    # Verify
    self.assertEqual(self.GetPrefsInfo().Prefs(pyauto.kRestoreOnStartup), 4)
    self.assertEqual(2, self.GetTabCount())
    self.ActivateTab(0)
    self.assertEqual(url1, self.GetActiveTabURL().spec())
    self.ActivateTab(1)
    self.assertEqual(url2, self.GetActiveTabURL().spec())

  def testSessionRestoreShowBookmarkBar(self):
    """Verify restore for bookmark bar visibility."""
    assert not self.GetPrefsInfo().Prefs(pyauto.kShowBookmarkBar)
    self.SetPrefs(pyauto.kShowBookmarkBar, True)
    self.assertEqual(True, self.GetPrefsInfo().Prefs(pyauto.kShowBookmarkBar))
    self.RestartBrowser(clear_profile=False)
    self.assertEqual(True, self.GetPrefsInfo().Prefs(pyauto.kShowBookmarkBar))
    self.assertTrue(self.GetBookmarkBarVisibility())

  def testDownloadDirPref(self):
    """Verify download dir pref."""
    test_dir = os.path.join(self.DataDir(), 'downloads')
    file_url = self.GetFileURLForPath(os.path.join(test_dir, 'a_zip_file.zip'))
    download_dir = self.GetDownloadDirectory().value()
    new_dl_dir = os.path.join(download_dir, 'My+Downloads Folder')
    downloaded_pkg = os.path.join(new_dl_dir, 'a_zip_file.zip')
    os.path.exists(new_dl_dir) and shutil.rmtree(new_dl_dir)
    os.makedirs(new_dl_dir)
    # Set pref to download in new_dl_dir
    self.SetPrefs(pyauto.kDownloadDefaultDirectory, new_dl_dir)
    self.DownloadAndWaitForStart(file_url)
    self.WaitForAllDownloadsToComplete()
    self.assertTrue(os.path.exists(downloaded_pkg))
    shutil.rmtree(new_dl_dir, ignore_errors=True)  # cleanup

  def testToolbarButtonsPref(self):
    """Verify toolbar buttons prefs."""
    # Assert defaults first
    self.assertFalse(self.GetPrefsInfo().Prefs(pyauto.kShowHomeButton))
    self.SetPrefs(pyauto.kShowHomeButton, True)
    self.RestartBrowser(clear_profile=False)
    self.assertTrue(self.GetPrefsInfo().Prefs(pyauto.kShowHomeButton))

  def testNetworkPredictionEnabledPref(self):
    """Verify DNS prefetching pref."""
    # Assert default
    self.assertTrue(self.GetPrefsInfo().Prefs(pyauto.kNetworkPredictionEnabled))
    self.SetPrefs(pyauto.kNetworkPredictionEnabled, False)
    self.RestartBrowser(clear_profile=False)
    self.assertFalse(self.GetPrefsInfo().Prefs(
        pyauto.kNetworkPredictionEnabled))

  def testHomepagePrefs(self):
    """Verify homepage prefs."""
    # "Use the New Tab page"
    self.SetPrefs(pyauto.kHomePageIsNewTabPage, True)
    logging.debug('Setting %s to 1' % pyauto.kHomePageIsNewTabPage)
    self.RestartBrowser(clear_profile=False)
    self.assertEqual(self.GetPrefsInfo().Prefs(pyauto.kHomePageIsNewTabPage),
                     True)
    # "Open this page"
    url = self.GetFileURLForPath(os.path.join(self.DataDir(), 'title1.html'))
    self.SetPrefs(pyauto.kHomePage, url)
    self.SetPrefs(pyauto.kHomePageIsNewTabPage, False)
    self.RestartBrowser(clear_profile=False)
    self.assertEqual(self.GetPrefsInfo().Prefs(pyauto.kHomePage), url)
    self.assertFalse(self.GetPrefsInfo().Prefs(pyauto.kHomePageIsNewTabPage))
    # TODO(nirnimesh): Actually verify that homepage loads.
    # This requires telling pyauto *not* to set about:blank as homepage.

  def testGeolocationPref(self):
    """Verify geolocation pref.

    Checks for the geolocation infobar.
    """
    url = self.GetFileURLForPath(os.path.join(  # triggers geolocation
        self.DataDir(), 'geolocation', 'geolocation_on_load.html'))
    self.assertEqual(3,  # default state
        self.GetPrefsInfo().Prefs(pyauto.kGeolocationDefaultContentSetting))
    self.NavigateToURL(url)
    self.assertTrue(self.WaitForInfobarCount(1))
    self.assertTrue(self.GetBrowserInfo()['windows'][0]['tabs'][0]['infobars'])
    # Disable geolocation
    self.SetPrefs(pyauto.kGeolocationDefaultContentSetting, 2)
    self.assertEqual(2,
        self.GetPrefsInfo().Prefs(pyauto.kGeolocationDefaultContentSetting))
    self.GetBrowserWindow(0).GetTab(0).Reload()
    self.assertTrue(self.WaitForInfobarCount(0))
    self.assertFalse(self.GetBrowserInfo()['windows'][0]['tabs'][0]['infobars'])

  def testUnderTheHoodPref(self):
    """Verify the security preferences for Under the Hood.
    The setting is enabled by default."""
    pref_list = [pyauto.kNetworkPredictionEnabled, pyauto.kSafeBrowsingEnabled,
                 pyauto.kAlternateErrorPagesEnabled,
                 pyauto.kSearchSuggestEnabled, pyauto.kShowOmniboxSearchHint]
    for pref in pref_list:
      # Verify the default value
      self.assertEqual(self.GetPrefsInfo().Prefs(pref), True)
      self.SetPrefs(pref, False)
    self.RestartBrowser(clear_profile=False)
    for pref in pref_list:
      self.assertEqual(self.GetPrefsInfo().Prefs(pref), False)

  def testJavaScriptEnableDisable(self):
    """Verify enabling disabling javascript prefs work """

    self.assertTrue(
        self.GetPrefsInfo().Prefs('webkit.webprefs.javascript_enabled'))
    url = self.GetFileURLForDataPath(
              os.path.join('javaScriptTitle.html'))
    title1 = 'Title from script javascript enabled'
    self.NavigateToURL(url)
    self.assertEqual(title1, self.GetActiveTabTitle())
    self.SetPrefs('webkit.webprefs.javascript_enabled', False)
    title = 'This is html title'
    self.NavigateToURL(url)
    self.assertEqual(title, self.GetActiveTabTitle())

  def testHaveLocalStatePrefs(self):
    """Verify that we have some Local State prefs."""
    self.assertTrue(self.GetLocalStatePrefsInfo())


if __name__ == '__main__':
  pyauto_functional.Main()

