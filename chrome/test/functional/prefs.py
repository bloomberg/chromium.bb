#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import shutil

import pyauto_functional  # Must be imported before pyauto
import pyauto

class PrefsTest(pyauto.PyUITest):
  """TestCase for Preferences."""

  INFOBAR_TYPE = 'rph_infobar'

  def Debug(self):
    """Test method for experimentation.

    This method will not run automatically.
    """
    while True:
      raw_input('Interact with the browser and hit <enter> to dump prefs... ')
      self.pprint(self.GetPrefsInfo().Prefs())

  def testSessionRestore(self):
    """Test session restore preference."""
    url1 = 'http://www.google.com/'
    url2 = 'http://news.google.com/'
    self.NavigateToURL(url1)
    self.AppendTab(pyauto.GURL(url2))
    num_tabs = self.GetTabCount()
    # Set pref to restore session on startup.
    self.SetPrefs(pyauto.kRestoreOnStartup, 1)
    logging.debug('Setting %s to 1' % pyauto.kRestoreOnStartup)
    self.RestartBrowser(clear_profile=False)
    self.assertEqual(self.GetPrefsInfo().Prefs(pyauto.kRestoreOnStartup), 1)
    self.assertEqual(num_tabs, self.GetTabCount())
    self.ActivateTab(0)
    self.assertEqual(url1, self.GetActiveTabURL().spec())
    self.ActivateTab(1)
    self.assertEqual(url2, self.GetActiveTabURL().spec())

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
    # GetBrowserInfo() call seems to fail later on in this test. Call it early.
    # crbug.com/89000
    branding = self.GetBrowserInfo()['properties']['branding']
    from webdriver_pages import settings
    from webdriver_pages.settings import Behaviors, ContentTypes
    driver = self.NewWebDriver()
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
    # Fails on Win7/Vista Chromium bots.  crbug.com/89000
    if (self.IsWin7() or self.IsWinVista()) and branding == 'Chromium':
      return
    behavior = driver.execute_async_script(
        'triggerGeoWithCallback(arguments[arguments.length - 1]);')
    self.assertEqual(
        behavior, Behaviors.BLOCK,
        msg='Behavior is "%s" when it should be BLOCKED.'  % behavior)

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
        self.GetPrefsInfo().Prefs(pyauto.kWebKitGlobalJavascriptEnabled))
    url = self.GetFileURLForDataPath(
              os.path.join('javaScriptTitle.html'))
    title1 = 'Title from script javascript enabled'
    self.NavigateToURL(url)
    self.assertEqual(title1, self.GetActiveTabTitle())
    self.SetPrefs(pyauto.kWebKitGlobalJavascriptEnabled, False)
    title = 'This is html title'
    self.NavigateToURL(url)
    self.assertEqual(title, self.GetActiveTabTitle())

  def testHaveLocalStatePrefs(self):
    """Verify that we have some Local State prefs."""
    self.assertTrue(self.GetLocalStatePrefsInfo())

  def testAllowSelectedGeoTracking(self):
    """Verify hostname pattern and behavior for allowed tracking."""
    # Default location tracking option "Ask me".
    self.SetPrefs(pyauto.kGeolocationDefaultContentSetting, 3)
    self.NavigateToURL(
        self.GetHttpURLForDataPath('geolocation', 'geolocation_on_load.html'))
    self.assertTrue(self.WaitForInfobarCount(1))
    self.PerformActionOnInfobar('accept', infobar_index=0)  # Allow tracking.
    # Get the hostname pattern (e.g. http://127.0.0.1:57622).
    hostname_pattern = (
        '/'.join(self.GetHttpURLForDataPath('').split('/')[0:3]) + '/')
    self.assertEqual(
        {hostname_pattern: {hostname_pattern: 1}},  # Allow the hostname.
        self.GetPrefsInfo().Prefs(pyauto.kGeolocationContentSettings))

  def testDismissedInfobarSavesNoEntry(self):
    """Verify dismissing infobar does not save an exception entry."""
    # Default location tracking option "Ask me".
    self.SetPrefs(pyauto.kGeolocationDefaultContentSetting, 3)
    self.NavigateToURL(
        self.GetFileURLForDataPath('geolocation', 'geolocation_on_load.html'))
    self.assertTrue(self.WaitForInfobarCount(1))
    self.PerformActionOnInfobar('dismiss', infobar_index=0)
    self.assertEqual(
        {}, self.GetPrefsInfo().Prefs(pyauto.kGeolocationContentSettings))

  def testGeolocationBlockedWhenTrackingDenied(self):
    """Verify geolocations is blocked when tracking is denied.

    The test verifies the blocked hostname pattern entry on the Geolocations
    exceptions page.
    """
    from webdriver_pages import settings
    from webdriver_pages.settings import Behaviors, ContentTypes
    driver = self.NewWebDriver()
    # Ask for permission when site wants to track.
    self.SetPrefs(pyauto.kGeolocationDefaultContentSetting, 3)
    self.NavigateToURL(
        self.GetHttpURLForDataPath('geolocation', 'geolocation_on_load.html'))
    self.assertTrue(self.WaitForInfobarCount(1))
    self.PerformActionOnInfobar('cancel', infobar_index=0)  # Deny tracking.
    behavior = driver.execute_async_script(
        'triggerGeoWithCallback(arguments[arguments.length - 1]);')
    self.assertEqual(
        behavior, Behaviors.BLOCK,
        msg='Behavior is "%s" when it should be BLOCKED.'  % behavior)
    # Get the hostname pattern (e.g. http://127.0.0.1:57622).
    hostname_pattern = (
        '/'.join(self.GetHttpURLForDataPath('').split('/')[0:3]) + '/')
    self.assertEqual(
        {hostname_pattern: {hostname_pattern: 2}},  # Block the hostname.
        self.GetPrefsInfo().Prefs(pyauto.kGeolocationContentSettings))

  def testImageContentSettings(self):
    """Verify image content settings show or hide images."""
    # Checks whether an image is loaded by checking the area (width
    # and height) of the image. If the area is non zero then the image is
    # visible. If the area is zero then the image is not loaded.
    # Chrome zeros the |naturalWidth| and |naturalHeight|.
    script = """
      for (i=0; i < document.images.length; i++) {
        if ((document.images[i].naturalWidth != 0) &&
            (document.images[i].naturalHeight != 0)) {
          return true;
        }
      }
      return false;
    """
    url = self.GetFileURLForPath(os.path.join(
        self.DataDir(), 'settings', 'image_page.html'))
    self.NavigateToURL(url)
    driver = self.NewWebDriver()
    self.assertTrue(driver.execute_script(script),
                    msg='No visible images found.')
    self.SetPrefs(pyauto.kDefaultContentSettings, {'images': 2})
    driver.get(url)
    self.assertFalse(driver.execute_script(script),
                     msg='At least one visible image found.')

  def _GetInfobarIndexByType(self, infobar_type, windex, tab_index):
    # TODO(dyu): Move this helper function into test_util file.
    """Returns the index of the infobar of the given type.

    Args:
      infobar_type: The infobar type to look for.
      windex: Window index.
      tab_index: Tab index.

    Returns:
      Index of infobar for infobar type, or None if not found.
    """
    infobar_list = (
        self.GetBrowserInfo()['windows'][windex]['tabs'][tab_index] \
            ['infobars'])
    for infobar in infobar_list:
      if infobar_type == infobar['type']:
        return infobar_list.index(infobar)
    return None

  def _GetIndexForRPHInfobar(self, windex=0, tab_index=0):
    """Waits for RPH infobar to appear and returns its index.

    Args:
      windex: Window index. Defaults to 0 (first window).
      tab_index: Tab index. Defaults to 0 (first tab).

    Returns:
      Index of RPH infobar.
    """
    self.assertTrue(
        self.WaitUntil(lambda: self._GetInfobarIndexByType(
            self.INFOBAR_TYPE, windex, tab_index) is not None),
        msg='Registered Handler Protocol infobar did not appear.')
    # Return the infobar index.
    return self._GetInfobarIndexByType(
        self.INFOBAR_TYPE, windex, tab_index)

  def testProtocolHandlerRegisteredCorrectly(self):
    """Verify sites that ask to be default handlers registers correctly."""
    url = self.GetHttpURLForDataPath('settings', 'protocol_handler.html')
    self.NavigateToURL(url)
    self.PerformActionOnInfobar(
        'accept', infobar_index=self._GetIndexForRPHInfobar())
    driver = self.NewWebDriver()
    driver.find_element_by_id('test_protocol').click()
    self.assertTrue(
        driver.execute_script('return verifyQueryConformsToProtocol();'),
        msg='Protocol did not register correctly.')


if __name__ == '__main__':
  pyauto_functional.Main()
