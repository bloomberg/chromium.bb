#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import re

import pyauto_functional  # Must be imported before pyauto
import pyauto
import test_utils


class InfobarTest(pyauto.PyUITest):
  """TestCase for Infobars."""

  def Debug(self):
    """Test method for experimentation.

    This method will not run automatically.
    To run:
      python chrome/test/functional/infobars.py infobars.InfobarTest.Debug
    """
    while True:
      raw_input('Hit <enter> to dump info.. ')
      info = self.GetBrowserInfo()
      for window in info['windows']:
        for tab in window['tabs']:
          print 'Window', window['index'], 'tab', tab['index']
          self.pprint(tab['infobars'])

  def setUp(self):
    pyauto.PyUITest.setUp(self)
    self._flash_plugin_type = 'Plug-in'
    if self.GetBrowserInfo()['properties']['branding'] == 'Google Chrome':
      self._flash_plugin_type = 'Pepper Plugin'
    # Forcibly trigger all plugins to get registered.  crbug.com/94123
    # Sometimes flash files loaded too quickly after firing browser
    # ends up getting downloaded, which seems to indicate that the plugin
    # hasn't been registered yet.
    self.GetPluginsInfo()

  def _GetTabInfo(self, windex=0, tab_index=0):
    """Helper to return info for the given tab in the given window.

    Defaults to first tab in first window.
    """
    return self.GetBrowserInfo()['windows'][windex]['tabs'][tab_index]

  def testPluginCrashInfobar(self):
    """Verify the "plugin crashed" infobar."""
    flash_url = self.GetFileURLForContentDataPath('plugin', 'flash.swf')
    # Trigger flash plugin
    self.NavigateToURL(flash_url)
    child_processes = self.GetBrowserInfo()['child_processes']
    flash = [x for x in child_processes if
             x['type'] == self._flash_plugin_type and
             x['name'] == 'Shockwave Flash'][0]
    self.assertTrue(flash)
    logging.info('Killing flash plugin. pid %d' % flash['pid'])
    self.Kill(flash['pid'])
    self.assertTrue(self.WaitForInfobarCount(1))
    crash_infobar = self._GetTabInfo()['infobars']
    self.assertTrue(crash_infobar)
    self.assertEqual(1, len(crash_infobar))
    self.assertTrue('crashed' in crash_infobar[0]['text'])
    self.assertEqual('confirm_infobar', crash_infobar[0]['type'])
    # Dismiss the infobar
    self.PerformActionOnInfobar('dismiss', infobar_index=0)
    self.assertFalse(self._GetTabInfo()['infobars'])

  def _VerifyGeolocationInfobar(self, windex, tab_index):
    """Verify geolocation infobar properties.

    Assumes that geolocation infobar is showing up in the given tab in the
    given window.
    """
    # TODO(dyu): Remove this helper function when a function to identify
    # infobar_type and index of the type is implemented.
    tab_info = self._GetTabInfo(windex, tab_index)
    geolocation_infobar = tab_info['infobars']
    self.assertTrue(geolocation_infobar)
    self.assertEqual(1, len(geolocation_infobar))
    self.assertEqual('Learn more', geolocation_infobar[0]['link_text'])
    self.assertEqual(2, len(geolocation_infobar[0]['buttons']))
    self.assertEqual('Allow', geolocation_infobar[0]['buttons'][0])
    self.assertEqual('Deny', geolocation_infobar[0]['buttons'][1])

  def testGeolocationInfobar(self):
    """Verify geoLocation infobar."""
    url = self.GetHttpURLForDataPath('geolocation', 'geolocation_on_load.html')
    self.NavigateToURL(url)
    self.assertTrue(self.WaitForInfobarCount(1))
    self._VerifyGeolocationInfobar(windex=0, tab_index=0)
    # Accept, and verify that the infobar went away
    self.PerformActionOnInfobar('accept', infobar_index=0)
    self.assertFalse(self._GetTabInfo()['infobars'])

  def testGeolocationInfobarInMultipleTabsAndWindows(self):
    """Verify GeoLocation inforbar in multiple tabs."""
    url = self.GetFileURLForDataPath(  # triggers geolocation
        'geolocation', 'geolocation_on_load.html')
    for tab_index in range(1, 2):
      self.AppendTab(pyauto.GURL(url))
      self.assertTrue(
          self.WaitForInfobarCount(1, windex=0, tab_index=tab_index))
      self._VerifyGeolocationInfobar(windex=0, tab_index=tab_index)
    # Try in a new window
    self.OpenNewBrowserWindow(True)
    self.NavigateToURL(url, 1, 0)
    self.assertTrue(self.WaitForInfobarCount(1, windex=1, tab_index=0))
    self._VerifyGeolocationInfobar(windex=1, tab_index=0)
    # Incognito window
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
    self.NavigateToURL(url, 2, 0)
    self.assertTrue(self.WaitForInfobarCount(1, windex=2, tab_index=0))
    self._VerifyGeolocationInfobar(windex=2, tab_index=0)

  def testMultipleDownloadsInfobar(self):
    """Verify the mutiple downloads infobar."""
    zip_files = ['a_zip_file.zip']
    zip_files.append(zip_files[0].replace('.', ' (1).'))
    html_file = 'download-a_zip_file.html'
    assert pyauto.PyUITest.IsEnUS()
    file_url = self.GetFileURLForDataPath('downloads', html_file)
    match_text = 'This site is attempting to download multiple files. ' \
                 'Do you want to allow this?'
    self.NavigateToURL('chrome://downloads')  # trigger download manager
    for zip_file in zip_files:
      test_utils.RemoveDownloadedTestFile(self, zip_file)
    self.DownloadAndWaitForStart(file_url)
    self.assertTrue(self.WaitForInfobarCount(1))
    tab_info = self._GetTabInfo(0, 0)
    infobars = tab_info['infobars']
    self.assertTrue(infobars, 'Expected the multiple downloads infobar')
    self.assertEqual(1, len(infobars))
    self.assertEqual(match_text, infobars[0]['text'])
    self.assertEqual(2, len(infobars[0]['buttons']))
    self.assertEqual('Allow', infobars[0]['buttons'][0])
    self.assertEqual('Deny', infobars[0]['buttons'][1])
    self.WaitForAllDownloadsToComplete()
    for zip_file in zip_files:
      test_utils.RemoveDownloadedTestFile(self, zip_file)

  def _GetFlashCrashInfobarCount(self, windex=0, tab_index=0):
    """Returns the count of 'Shockwave Flash has crashed' infobars."""
    browser_window = self.GetBrowserInfo()['windows'][windex]
    infobars = browser_window['tabs'][tab_index]['infobars']
    flash_crash_infobar_count = 0
    for infobar in infobars:
      if (('text' in infobar) and
          infobar['text'].startswith('Shockwave Flash has crashed')):
        flash_crash_infobar_count += 1
    return flash_crash_infobar_count

  def testPluginCrashForMultiTabs(self):
    """Verify plugin crash infobar shows up only on the tabs using plugin."""
    non_flash_url = self.GetFileURLForDataPath('english_page.html')
    flash_url = self.GetFileURLForContentDataPath('plugin', 'FlashSpin.swf')
    # False = Non flash url, True = Flash url
    # We have set of these values to compare a flash page and a non-flash page
    urls_type = [False, True, False, True, False]
    for _ in range(2):
      self.AppendTab(pyauto.GURL(flash_url))
      self.AppendTab(pyauto.GURL(non_flash_url))
    # Killing flash process
    child_processes = self.GetBrowserInfo()['child_processes']
    flash = [x for x in child_processes if
             x['type'] == self._flash_plugin_type and
             x['name'] == 'Shockwave Flash'][0]
    self.assertTrue(flash)
    self.Kill(flash['pid'])
    # Crash plugin infobar should show up in the second tab of this window
    # so passing window and tab argument in the wait for an infobar.
    self.assertTrue(self.WaitForInfobarCount(1, windex=0, tab_index=1))
    for i in range(len(urls_type)):
      # Verify that if page doesn't have flash plugin,
      # it should not have infobar popped-up
      self.ActivateTab(i)
      if not urls_type[i]:
        self.assertEqual(
            self._GetFlashCrashInfobarCount(0, i), 0,
            msg='Did not expect crash infobar in tab at index %d' % i)
      elif urls_type[i]:
        self.assertEqual(
            self._GetFlashCrashInfobarCount(0, i), 1,
            msg='Expected crash infobar in tab at index %d' % i)
        infobar = self.GetBrowserInfo()['windows'][0]['tabs'][i]['infobars']
        self.assertEqual(infobar[0]['type'], 'confirm_infobar')
        self.assertEqual(len(infobar), 1)


class OneClickInfobarTest(pyauto.PyUITest):
  """Tests for one-click sign in infobar."""

  BLOCK_COOKIE_PATTERN = {'https://accounts.google.com/': {'cookies': 2}}
  OC_INFOBAR_TYPE = 'oneclicklogin_infobar'
  PW_INFOBAR_TYPE = 'password_infobar'
  URL = 'https://www.google.com/accounts/ServiceLogin'
  URL_LOGIN = 'https://www.google.com/accounts/Login'
  URL_LOGOUT = 'https://www.google.com/accounts/Logout'

  def setUp(self):
    pyauto.PyUITest.setUp(self)
    self._driver = self.NewWebDriver()

  def _LogIntoGoogleAccount(self, tab_index=0, windex=0):
    """Log into Google account.

    Args:
      tab_index: The tab index, default is 0.
      windex: The window index, default is 0.
    """
    creds = self.GetPrivateInfo()['test_google_account']
    username = creds['username']
    password = creds['password']
    test_utils.GoogleAccountsLogin(self, username, password, tab_index, windex)
    # TODO(dyu): Use WaitUntilNavigationCompletes after investigating
    # crbug.com/124877
    self.WaitUntil(
        lambda: self.GetDOMValue('document.readyState'),
        expect_retval='complete')

  def _PerformActionOnInfobar(self, action):
    """Perform an action on the infobar: accept, cancel, or dismiss.

    The one-click sign in infobar must show in the first tab of the first
    window. If action is 'accept' then the account is synced. If the action is
    'cancel' then the infobar should be dismissed and never shown again. The
    account will not be synced. If the action is 'dismiss' then the infobar will
    shown again after the next login.

    Args:
      action: The action to perform on the infobar.
    """
    infobar_index = test_utils.WaitForInfobarTypeAndGetIndex(
        self, self.OC_INFOBAR_TYPE)
    self.PerformActionOnInfobar(action, infobar_index)

  def _DisplayOneClickInfobar(self, tab_index=0, windex=0):
    """One-click sign in infobar appears after logging into google account.

    Args:
      tab_index: The tab index, default is 0.
      windex: The window index, default is 0.
    """
    self._LogIntoGoogleAccount(tab_index=tab_index, windex=windex)
    self.assertTrue(self.WaitUntil(
        lambda: test_utils.GetInfobarIndexByType(
            self, self.OC_INFOBAR_TYPE,
            tab_index=tab_index, windex=windex) is not None),
                    msg='The one-click login infobar did not appear.')

  def testDisplayOneClickInfobar(self):
    """Verify one-click infobar appears after login into google account.

    One-click infobar should appear after signing into a google account
    for the first time using a clean profile.
    """
    self._DisplayOneClickInfobar()

  def testNoOneClickInfobarAfterCancel(self):
    """Verify one-click infobar does not appear again after clicking cancel.

    The one-click infobar should not display again after logging into an
    account and selecting to reject sync the first time. The test covers
    restarting the browser with the same profile and verifying the one-click
    infobar does not show after login.

    This test also verifies that the password infobar displays.
    """
    self._DisplayOneClickInfobar()
    self._PerformActionOnInfobar(action='cancel')  # Click 'No thanks' button.
    self.NavigateToURL(self.URL_LOGOUT)
    self._LogIntoGoogleAccount()
    test_utils.WaitForInfobarTypeAndGetIndex(self, self.PW_INFOBAR_TYPE)
    test_utils.AssertInfobarTypeDoesNotAppear(self, self.OC_INFOBAR_TYPE)
    # Restart browser with the same profile.
    self.RestartBrowser(clear_profile=False)
    self.NavigateToURL(self.URL_LOGOUT)
    self._LogIntoGoogleAccount()
    test_utils.AssertInfobarTypeDoesNotAppear(self, self.OC_INFOBAR_TYPE)

  def testDisplayOneClickInfobarAfterDismiss(self):
    """Verify one-click infobar appears again after clicking dismiss button.

    The one-click infobar should display again after logging into an
    account and clicking to dismiss the infobar the first time.

    This test also verifies that the password infobar does not display.
    The one-click infobar should supersede the password infobar.
    """
    self._DisplayOneClickInfobar()
    self._PerformActionOnInfobar(action='dismiss')  # Click 'x' button.
    self.NavigateToURL(self.URL_LOGOUT)
    self._LogIntoGoogleAccount()
    test_utils.WaitForInfobarTypeAndGetIndex(self, self.OC_INFOBAR_TYPE)
    test_utils.AssertInfobarTypeDoesNotAppear(self, self.PW_INFOBAR_TYPE)

  def _OpenSecondProfile(self):
    """Create a second profile."""
    self.OpenNewBrowserWindowWithNewProfile()
    self.assertEqual(2, len(self.GetMultiProfileInfo()['profiles']),
        msg='The second profile was not created.')

  def testDisplayOneClickInfobarPerProfile(self):
    """Verify one-click infobar appears for each profile after sign-in."""
    # Default profile.
    self._DisplayOneClickInfobar()
    self._OpenSecondProfile()
    self._DisplayOneClickInfobar(windex=1)

  def testNoOneClickInfobarWhenCookiesBlocked(self):
    """Verify one-click infobar does not show when cookies are blocked.

    One-click sign in should not be enabled if cookies are blocked for Google
    accounts domain.

    This test verifies the following bug: crbug.com/117841
    """
    # Block cookies for Google accounts domain.
    self.SetPrefs(pyauto.kContentSettingsPatternPairs,
                  self.BLOCK_COOKIE_PATTERN)
    self._LogIntoGoogleAccount()
    test_utils.AssertInfobarTypeDoesNotAppear(self, self.OC_INFOBAR_TYPE)

  def testOneClickInfobarShownWhenWinLoseFocus(self):
    """Verify one-click infobar still shows when window loses focus.

    This test verifies the following bug: crbug.com/121739
    """
    self._LogIntoGoogleAccount()
    test_utils.WaitForInfobarTypeAndGetIndex(self, self.OC_INFOBAR_TYPE)
    # Open new window to shift focus away.
    self.OpenNewBrowserWindow(True)
    test_utils.GetInfobarIndexByType(self, self.OC_INFOBAR_TYPE)

  def testNoOneClickInfobarInIncognito(self):
    """Verify that one-click infobar does not show up in incognito mode."""
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
    self._LogIntoGoogleAccount(windex=1)
    test_utils.AssertInfobarTypeDoesNotAppear(
        self, self.OC_INFOBAR_TYPE, windex=1)


if __name__ == '__main__':
  pyauto_functional.Main()
