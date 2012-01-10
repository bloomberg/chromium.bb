#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import shutil
import time

import pyauto_functional  # Must be imported before pyauto
import pyauto
from webdriver_pages import settings
from webdriver_pages.settings import Behaviors, ContentTypes

class PrefsTest(pyauto.PyUITest):
  """TestCase for Preferences."""

  def setUp(self):
    pyauto.PyUITest.setUp(self)
    self.driver = self.NewWebDriver()

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

  def _SetContentSettingsDefaultBehavior(self, content_type, option):
    """Set the Content Settings default behavior.

    Args:
      content_type: The string content type to manage.
      option: The string option allow, deny or ask.
    """
    page = settings.ContentSettingsPage.FromNavigation(self.driver)
    page.SetContentTypeOption(content_type, option)

  def _VerifyContentException(self, content_type, hostname_pattern, behavior):
    """Find hostname pattern and behavior on content Exceptions page.

    Args:
      content_type: The string content type to manage.
      hostname_pattern: The URL or pattern associated with the behavior.
      behavior: The exception to allow or block the hostname.
    """
    page = settings.ManageExceptionsPage.FromNavigation(
        self.driver, content_type)
    self.assertTrue(page.GetExceptions().has_key(hostname_pattern),
                     msg=('No displayed host name matches pattern "%s"'
                          % hostname_pattern))
    self.assertEqual(behavior, page.GetExceptions()[hostname_pattern],
                     msg=('Displayed behavior "%s" does not match behavior "%s"'
                          % (page.GetExceptions()[hostname_pattern], behavior)))

  def _VerifyDismissInfobar(self, content_type):
    """Verify only Ask line entry is shown when Infobar is dismissed.

    Args:
      content_type: The string content type to manage.
    """
    page = settings.ManageExceptionsPage.FromNavigation(
        self.driver, content_type)
    self.assertEqual(0, len(page.GetExceptions()))

  def _GetGeolocationTestPageURL(self):
    """Get the HTML test page for geolocation."""
    return self.GetFileURLForDataPath('geolocation', 'geolocation_on_load.html')

  def _GetGeolocationTestPageHttpURL(self):
    """Get HTML test page for geolocation served by the local http server."""
    return self.GetHttpURLForDataPath('geolocation', 'geolocation_on_load.html')

  def testBlockAllGeoTracking(self):
    """Verify web page is blocked when blocking tracking of all sites."""
    self._SetContentSettingsDefaultBehavior(
        ContentTypes.GEOLOCATION, Behaviors.BLOCK)
    self.NavigateToURL(self._GetGeolocationTestPageHttpURL())
    self.assertFalse(self.GetBrowserInfo()['windows'][0]['tabs'][0]['infobars'])
    self.driver.set_script_timeout(10)
    # Call the js function, which returns whether geolocation is blocked.
    behavior = self.driver.execute_async_script(
          'triggerGeoWithCallback(arguments[arguments.length - 1]);')
    self.assertEqual(
          behavior, Behaviors.BLOCK,
          msg='Blocking tracking of all sites was not set')

  def testAllowAllGeoTracking(self):
    """Verify that infobar does not appear when allowing all sites to track."""
    self._SetContentSettingsDefaultBehavior(
        ContentTypes.GEOLOCATION, Behaviors.ALLOW)  # Allow all sites to track.
    self.NavigateToURL(self._GetGeolocationTestPageHttpURL())
    self.assertFalse(self.GetBrowserInfo()['windows'][0]['tabs'][0]['infobars'])
    self.driver.set_script_timeout(10)
    # Call the js function, which returns whether geolocation is blocked.
    behavior = self.driver.execute_async_script(
          'triggerGeoWithCallback(arguments[arguments.length - 1]);')
    self.assertEqual(
          behavior, Behaviors.ALLOW,
          msg='Allowing tracking of all sites was not set')

  def testAllowSelectedGeoTracking(self):
    """Verify location prefs to allow selected sites to track location."""
    self._SetContentSettingsDefaultBehavior(
        ContentTypes.GEOLOCATION, Behaviors.ASK)
    self.NavigateToURL(self._GetGeolocationTestPageHttpURL())
    self.assertTrue(self.WaitForInfobarCount(1))
    self.assertTrue(self.GetBrowserInfo()['windows'][0]['tabs'][0]['infobars'])
    self.PerformActionOnInfobar('accept', infobar_index=0)
    # Allow selected hosts.
    self._VerifyContentException(
        ContentTypes.GEOLOCATION,
        # Get the hostname pattern (e.g. http://127.0.0.1:57622).
        '/'.join(self.GetHttpURLForDataPath('').split('/')[0:3]),
        Behaviors.ALLOW)

  def testDenySelectedGeoTracking(self):
    """Verify location prefs to deny selected sites to track location."""
    self._SetContentSettingsDefaultBehavior(
        ContentTypes.GEOLOCATION, Behaviors.ASK)
    self.NavigateToURL(self._GetGeolocationTestPageHttpURL())
    self.assertTrue(self.WaitForInfobarCount(1))
    self.assertTrue(self.GetBrowserInfo()['windows'][0]['tabs'][0]['infobars'])
    self.PerformActionOnInfobar('cancel', infobar_index=0)
    # Block selected hosts.
    self._VerifyContentException(
        ContentTypes.GEOLOCATION,
        # Get the hostname pattern (e.g. http://127.0.0.1:57622).
        '/'.join(self.GetHttpURLForDataPath('').split('/')[0:3]),
        Behaviors.BLOCK)

  def testCancelSelectedGeoTracking(self):
    """Verify infobar canceled for sites to track location."""
    self._SetContentSettingsDefaultBehavior(
        ContentTypes.GEOLOCATION, Behaviors.ASK)
    self.NavigateToURL(self._GetGeolocationTestPageURL())
    self.assertTrue(self.WaitForInfobarCount(1))
    self.assertTrue(self.GetBrowserInfo()['windows'][0]['tabs'][0]['infobars'])
    self.PerformActionOnInfobar('dismiss', infobar_index=0)
    self._VerifyDismissInfobar(ContentTypes.GEOLOCATION)

  def testAddNewException(self):
    """Verify new exception added for hostname pattern and behavior."""
    content_type = ContentTypes.PLUGINS
    page = settings.ManageExceptionsPage.FromNavigation(
        self.driver, content_type)

    pattern, behavior = ('bing.com', Behaviors.BLOCK)
    page.AddNewException(pattern, behavior)
    self.assertEqual(page.GetExceptions()[pattern], Behaviors.BLOCK,
                     msg='The behavior "%s" was not added for pattern "%s"'
                     % (behavior, pattern))

  def testChangeExceptionBehavior(self):
    """Verify behavior for hostname pattern is changed."""
    content_type = ContentTypes.PLUGINS
    page = settings.ManageExceptionsPage.FromNavigation(
        self.driver, content_type)

    pattern, behavior = ('bing.com', Behaviors.BLOCK)
    page.AddNewException(pattern, behavior)
    new_behavior = Behaviors.ALLOW
    page.SetBehaviorForPattern(pattern, new_behavior)
    self.assertEqual(page.GetExceptions()[pattern], Behaviors.ALLOW,
                     msg='The behavior for "%s" did not change: "%s"'
                     % (pattern, behavior))

  def testDeleteException(self):
    """Verify exception deleted for hostname pattern and behavior."""
    content_type = ContentTypes.PLUGINS
    page = settings.ManageExceptionsPage.FromNavigation(
        self.driver, content_type)

    pattern, behavior = ('bing.com', Behaviors.BLOCK)
    page.AddNewException(pattern, behavior)
    self.assertEqual(page.GetExceptions()[pattern], Behaviors.BLOCK,
                     msg='The behavior "%s" was not added for pattern "%s"'
                     % (behavior, pattern))
    page.DeleteException(pattern)
    self.assertEqual(page.GetExceptions().get(pattern, KeyError), KeyError,
                     msg='Pattern "%s" was not deleted' % pattern)

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


if __name__ == '__main__':
  pyauto_functional.Main()
