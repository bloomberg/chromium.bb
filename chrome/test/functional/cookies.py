#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

import pyauto_functional  # Must be imported before pyauto
import pyauto


class CookiesTest(pyauto.PyUITest):
  """Tests for Cookies."""

  def _CookieCheckIncognitoWindow(self, url):
    """Check the cookie for the given URL in an incognito window."""
    # Navigate to the URL in an incognito window and verify no cookie is set.
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
    self.NavigateToURL(url, 1, 0)
    self.assertFalse(self.GetCookie(pyauto.GURL(url)))

  def testSetCookies(self):
    """Test setting cookies and getting the value."""
    cookie_url = pyauto.GURL(self.GetFileURLForDataPath('title1.html'))
    cookie_val = 'foo=bar'
    self.SetCookie(cookie_url, cookie_val)
    self.assertEqual(cookie_val, self.GetCookie(cookie_url))

  def testCookiesHttp(self):
    """Test cookies set over HTTP for incognito and regular windows."""
    http_url = 'http://www.google.com'
    self.assertFalse(self.GetCookie(pyauto.GURL(http_url)))
    # Incognito window
    self._CookieCheckIncognitoWindow(http_url)
    # Regular window
    self.NavigateToURL(http_url)
    cookie_data = self.GetCookie(pyauto.GURL(http_url))
    self.assertTrue(cookie_data)
    # Restart and verify that the cookie persists.
    self.assertEqual(cookie_data, self.GetCookie(pyauto.GURL(http_url)))

  def testCookiesHttps(self):
    """Test cookies set over HTTPS for incognito and regular windows."""
    https_url = 'https://www.google.com'
    self.assertFalse(self.GetCookie(pyauto.GURL(https_url)))
    # Incognito window
    self._CookieCheckIncognitoWindow(https_url)
    # Regular window
    self.NavigateToURL(https_url)
    cookie_data = self.GetCookie(pyauto.GURL(https_url))
    self.assertTrue(cookie_data)
    # Restart and verify that the cookie persists.
    self.assertEqual(cookie_data, self.GetCookie(pyauto.GURL(https_url)))

  def testCookiesFile(self):
    """Test cookies set from file:// url for incognito and regular windows."""
    file_url = self.GetFileURLForDataPath('setcookie.html')
    self.assertFalse(self.GetCookie(pyauto.GURL(file_url)))
    # Incognito window
    self._CookieCheckIncognitoWindow(file_url)
    # Regular window
    self.NavigateToURL(file_url)
    self.assertEqual('name=Good', self.GetCookie(pyauto.GURL(file_url)))
    # Restart and verify that cookie persists
    self.RestartBrowser(clear_profile=False)
    self.assertEqual('name=Good', self.GetCookie(pyauto.GURL(file_url)))

  def testBlockCookies(self):
    """Verify that cookies are being blocked."""
    file_url = self.GetFileURLForDataPath('setcookie.html')
    self.assertFalse(self.GetCookie(pyauto.GURL(file_url)))

    # Set the preference to block all cookies.
    self.SetPrefs(pyauto.kDefaultContentSettings, {u'cookies': 2})
    # Regular window
    self.NavigateToURL('http://www.google.com')
    self.AppendTab(pyauto.GURL('https://www.google.com'))
    self.AppendTab(pyauto.GURL(file_url))
    self.assertFalse(self.GetCookie(pyauto.GURL(file_url)),
                     msg='Cookies are not blocked.')

    # Incognito window
    self._CookieCheckIncognitoWindow('http://www.google.com')
    self.GetBrowserWindow(1).GetTab(0).Close(True)

    # Restart and verify that cookie setting persists and there are no cookies.
    self.SetPrefs(pyauto.kRestoreOnStartup, 1)
    self.RestartBrowser(clear_profile=False)
    self.assertEquals({u'cookies': 2},
        self.GetPrefsInfo().Prefs(pyauto.kDefaultContentSettings))
    self.assertFalse(self.GetCookie(pyauto.GURL(file_url)))

  def testClearCookiesOnEndingSession(self):
    """Verify that cookies are cleared when the browsing session is closed."""
    file_url = self.GetFileURLForDataPath('setcookie.html')
    self.assertFalse(self.GetCookie(pyauto.GURL(file_url)))

    # Set the option to clear cookies when the browser session is closed.
    self.SetPrefs(pyauto.kClearSiteDataOnExit, True)

    self.NavigateToURL(file_url)
    self.assertEqual('name=Good', self.GetCookie(pyauto.GURL(file_url)))

    # Restart and verify that cookie does not persist
    self.RestartBrowser(clear_profile=False)
    self.assertFalse(self.GetCookie(pyauto.GURL(file_url)))

  def testAllowCookiesUsingExceptions(self):
    """Verify that cookies can be allowed and set using exceptions for
    particular website(s) when all others are blocked."""
    file_url = self.GetFileURLForDataPath('setcookie.html')
    self.assertFalse(self.GetCookie(pyauto.GURL(file_url)))

    # Set the preference to block all cookies.
    self.SetPrefs(pyauto.kDefaultContentSettings, {u'cookies': 2})

    self.NavigateToURL(file_url)
    # Check that no cookies are stored.
    self.assertFalse(self.GetCookie(pyauto.GURL(file_url)))

    # Creating an exception to allow cookies from http://www.google.com.
    self.SetPrefs(pyauto.kContentSettingsPatternPairs,
                 {'[*.]google.com,*': { 'cookies': 1}})
    # Navigate to google.com and check if cookies are set.
    self.NavigateToURL('http://www.google.com')
    self.assertTrue(self.GetCookie(pyauto.GURL('http://www.google.com')),
                    msg='Cookies are not set for the exception.')

  def testBlockCookiesUsingExceptions(self):
    """Verify that cookies can be blocked for a specific website
    using exceptions."""
    file_url = self.GetFileURLForDataPath('setcookie.html')
    self.assertFalse(self.GetCookie(pyauto.GURL(file_url)))

    # Create an exception to block cookies from http://www.google.com
    self.SetPrefs(pyauto.kContentSettingsPatternPairs,
                 {'[*.]google.com,*': { 'cookies': 2}})

    # Navigate to google.com and check if cookies are blocked.
    self.NavigateToURL('http://www.google.com')
    self.assertFalse(self.GetCookie(pyauto.GURL('http://www.google.com')),
                     msg='Cookies are being set for the exception.')

    # Check if cookies are being set for other websites/webpages.
    self.AppendTab(pyauto.GURL(file_url))
    self.assertEqual('name=Good', self.GetCookie(pyauto.GURL(file_url)))

  def testAllowCookiesForASessionUsingExceptions(self):
    """Verify that cookies can be allowed and set using exceptions for
    particular website(s) only for a session when all others are blocked."""
    file_url = self.GetFileURLForPath('setcookie.html')
    self.assertFalse(self.GetCookie(pyauto.GURL(file_url)))

    # Set the preference to block all cookies.
    self.SetPrefs(pyauto.kDefaultContentSettings, {u'cookies': 2})

    self.NavigateToURL(file_url)
    # Check that no cookies are stored.
    self.assertFalse(self.GetCookie(pyauto.GURL(file_url)))

    # Creating an exception to allow cookies for a session for google.com.
    self.SetPrefs(pyauto.kContentSettingsPatternPairs,
                 {'[*.]google.com,*': { 'cookies': 4}})

    # Navigate to google.com and check if cookies are set.
    self.NavigateToURL('http://www.google.com')
    self.assertTrue(self.GetCookie(pyauto.GURL('http://www.google.com')),
                    msg='Cookies are not set for the exception.')

    # Restart the browser to check that the cookie doesn't persist.
    self.RestartBrowser(clear_profile=False)
    self.assertFalse(self.GetCookie(pyauto.GURL('http://www.google.com')),
                     msg='Cookie persisted after restarting session.')


if __name__ == '__main__':
  pyauto_functional.Main()
