#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import logging

import pyauto_functional  # Must be imported before pyauto
import pyauto


class CookiesTest(pyauto.PyUITest):
  """Tests for Cookies."""

  def __init__(self, methodName='runTest'):
    super(CookiesTest, self).__init__(methodName)
    self.test_host = os.environ.get('COOKIES_TEST_HOST', 'google.com')

  def setUp(self):
    pyauto.PyUITest.setUp(self);
    # Set the startup preference to "open the new tab page", if the startup
    # preference is "continue where I left off", session cookies will be saved.
    self.SetPrefs(pyauto.kRestoreOnStartup, 5);

  def _CookieCheckIncognitoWindow(self, url, cookies_enabled=True):
    """Check the cookie for the given URL in an incognito window."""
    # Navigate to the URL in an incognito window and verify no cookie is set.
    self.assertFalse(self.GetCookie(pyauto.GURL(url)),
                     msg='Cannot run with pre-existing cookies')
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
    self.assertFalse(self.GetCookie(pyauto.GURL(url), 1),
                     msg='Fresh incognito window should not have cookies')
    self.NavigateToURL(url, 1, 0)
    if cookies_enabled:
      self.assertTrue(self.GetCookie(pyauto.GURL(url), 1),
                      msg='Cookies not set in incognito window')
    else:
      self.assertFalse(self.GetCookie(pyauto.GURL(url), 1),
                       msg='Cookies not blocked in incognito window')
    self.assertFalse(self.GetCookie(pyauto.GURL(url)),
                     msg='Incognito mode cookies leaking to regular profile')
    self.CloseBrowserWindow(1);
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
    self.assertFalse(self.GetCookie(pyauto.GURL(url), 1),
                    msg='Cookies persisting between incognito sessions')
    self.CloseBrowserWindow(1);

  def testSetCookies(self):
    """Test setting cookies and getting the value."""
    cookie_url = pyauto.GURL(self.GetFileURLForDataPath('title1.html'))
    cookie_val = 'foo=bar'
    self.assertFalse(self.GetCookie(cookie_url),
                     msg='There should be no cookies for %s' % cookie_url)
    self.SetCookie(cookie_url, cookie_val)
    self.assertEqual(cookie_val, self.GetCookie(cookie_url),
                     msg='Could not find the cookie value foo=bar')

  def testCookiesHttp(self):
    """Test cookies set over HTTP for incognito and regular windows."""
    http_url = 'http://%s' % self.test_host
    self.assertFalse(self.GetCookie(pyauto.GURL(http_url)),
                     msg='There should be no cookies for %s' % http_url)
    # Incognito window
    self._CookieCheckIncognitoWindow(http_url)
    # Regular window
    self.NavigateToURL(http_url)
    cookie_data = self.GetCookie(pyauto.GURL(http_url))
    self.assertTrue(cookie_data,
                    msg='Cookie did not exist after loading %s' % http_url)
    # Restart and verify that the cookie persists.
    self.RestartBrowser(clear_profile=False)
    self.assertTrue(self.GetCookie(pyauto.GURL(http_url)),
                    msg='Cookie did not persist after restarting session.')

  def testCookiesHttps(self):
    """Test cookies set over HTTPS for incognito and regular windows."""
    https_url = 'https://%s' % self.test_host
    self.assertFalse(self.GetCookie(pyauto.GURL(https_url)),
                     msg='There should be no cookies for %s' % https_url)
    # Incognito window
    self._CookieCheckIncognitoWindow(https_url)
    # Regular window
    self.NavigateToURL(https_url)
    cookie_data = self.GetCookie(pyauto.GURL(https_url))
    self.assertTrue(cookie_data,
                    msg='Cookie did not exist after loading %s' % https_url)
    # Restart and verify that the cookie persists.
    self.RestartBrowser(clear_profile=False)
    self.assertTrue(self.GetCookie(pyauto.GURL(https_url)),
                    msg='Cookie did not persist after restarting session.')

  def testCookiesFile(self):
    """Test cookies set from file:// url for incognito and regular windows."""
    file_url = self.GetFileURLForDataPath('setcookie.html')
    self.assertFalse(self.GetCookie(pyauto.GURL(file_url)),
                     msg='There should be no cookie for file url %s' % file_url)
    # Incognito window
    self._CookieCheckIncognitoWindow(file_url)
    # Regular window
    self.NavigateToURL(file_url)
    self.assertEqual('name=Good', self.GetCookie(pyauto.GURL(file_url)),
                     msg='Cookie does not exist after navigating to the page.')
    # Restart and verify that cookie persists
    self.RestartBrowser(clear_profile=False)
    self.assertEqual('name=Good', self.GetCookie(pyauto.GURL(file_url)),
                     msg='Cookie did not persist after restarting session.')

  def testBlockCookies(self):
    """Verify that cookies are being blocked."""
    file_url = self.GetFileURLForDataPath('setcookie.html')
    http_url = 'http://%s' % self.test_host
    https_url = 'https://%s' % self.test_host
    self.assertFalse(self.GetCookie(pyauto.GURL(file_url)),
                     msg='There should be no cookie for file url %s' % file_url)

    # Set the preference to block all cookies.
    self.SetPrefs(pyauto.kDefaultContentSettings, {u'cookies': 2})
    # Regular window
    self.NavigateToURL(http_url)
    self.AppendTab(pyauto.GURL(https_url))
    self.AppendTab(pyauto.GURL(file_url))
    self.assertFalse(self.GetCookie(pyauto.GURL(file_url)),
                     msg='Cookies are not blocked.')
    self.assertFalse(self.GetCookie(pyauto.GURL(http_url)),
                     msg='Cookies are not blocked.')
    self.assertFalse(self.GetCookie(pyauto.GURL(https_url)),
                     msg='Cookies are not blocked.')

    # Incognito window
    self._CookieCheckIncognitoWindow(http_url, cookies_enabled=False)

    # Restart and verify that cookie setting persists and there are no cookies.
    self.SetPrefs(pyauto.kRestoreOnStartup, 1)
    self.RestartBrowser(clear_profile=False)
    self.assertEquals({u'cookies': 2},
        self.GetPrefsInfo().Prefs(pyauto.kDefaultContentSettings),
        msg='Cookie setting did not persist after restarting session.')
    self.assertFalse(self.GetCookie(pyauto.GURL(file_url)),
                     msg='Cookies are not blocked.')
    self.assertFalse(self.GetCookie(pyauto.GURL(http_url)),
                     msg='Cookies are not blocked.')
    self.assertFalse(self.GetCookie(pyauto.GURL(https_url)),
                     msg='Cookies are not blocked.')

  def testAllowCookiesUsingExceptions(self):
    """Verify that cookies can be allowed and set using exceptions for
    particular website(s) when all others are blocked."""
    http_url = 'http://%s' % self.test_host
    self.assertFalse(self.GetCookie(pyauto.GURL(http_url)),
                     msg='There should be no cookies on %s' % http_url)

    # Set the preference to block all cookies.
    self.SetPrefs(pyauto.kDefaultContentSettings, {u'cookies': 2})

    self.NavigateToURL(http_url)
    # Check that no cookies are stored.
    self.assertFalse(self.GetCookie(pyauto.GURL(http_url)),
                     msg='A cookie was found when it should not have been.')

    # Creating an exception to allow cookies from http://www.google.com.
    self.SetPrefs(pyauto.kContentSettingsPatternPairs,
                 {'[*.]%s,*' % self.test_host: { 'cookies': 1}})
    # Navigate to google.com and check if cookies are set.
    self.NavigateToURL(http_url)
    self.assertTrue(self.GetCookie(pyauto.GURL(http_url)),
                    msg='Cookies are not set for the exception.')

  def testBlockCookiesUsingExceptions(self):
    """Verify that cookies can be blocked for a specific website
    using exceptions."""
    http_url = 'http://%s' % self.test_host
    file_url = self.GetFileURLForDataPath('setcookie.html')
    self.assertFalse(self.GetCookie(pyauto.GURL(http_url)),
                     msg='There should be no cookies on %s' % http_url)
    self.assertFalse(self.GetCookie(pyauto.GURL(file_url)),
                     msg='There should be no cookies on %s' % file_url)

    # Create an exception to block cookies from http://www.google.com
    self.SetPrefs(pyauto.kContentSettingsPatternPairs,
                 {'[*.]%s,*' % self.test_host: { 'cookies': 2}})

    # Navigate to google.com and check if cookies are blocked.
    self.NavigateToURL(http_url)
    self.assertFalse(self.GetCookie(pyauto.GURL(http_url)),
                     msg='Cookies are being set for the exception.')

    # Check if cookies are being set for other websites/webpages.
    self.AppendTab(pyauto.GURL(file_url))
    self.assertEqual('name=Good', self.GetCookie(pyauto.GURL(file_url)),
                     msg='Unable to find cookie name=Good')

  def testAllowCookiesForASessionUsingExceptions(self):
    """Verify that cookies can be allowed and set using exceptions for
    particular website(s) only for a session when all others are blocked."""
    http_url = 'http://%s' % self.test_host
    self.assertFalse(self.GetCookie(pyauto.GURL(http_url)),
                     msg='There should be no cookies on %s' % http_url)

    # Set the preference to block all cookies.
    self.SetPrefs(pyauto.kDefaultContentSettings, {u'cookies': 2})

    self.NavigateToURL(http_url)
    # Check that no cookies are stored.
    self.assertFalse(self.GetCookie(pyauto.GURL(http_url)),
                     msg='Cookies were found for the url %s' % http_url)

    # Creating an exception to allow cookies for a session for google.com.
    self.SetPrefs(pyauto.kContentSettingsPatternPairs,
                 {'[*.]%s,*' % self.test_host: { 'cookies': 4}})

    # Navigate to google.com and check if cookies are set.
    self.NavigateToURL(http_url)
    self.assertTrue(self.GetCookie(pyauto.GURL(http_url)),
                    msg='Cookies are not set for the exception.')
    # Restart the browser to check that the cookie doesn't persist.
    # (This fails on ChromeOS because kRestoreOnStartup is ignored and
    # the startup preference is always "continue where I left off.")
    if not self.IsChromeOS():
      self.RestartBrowser(clear_profile=False)
      self.assertFalse(self.GetCookie(pyauto.GURL(http_url)),
                       msg='Cookie persisted after restarting session.')

if __name__ == '__main__':
  pyauto_functional.Main()
