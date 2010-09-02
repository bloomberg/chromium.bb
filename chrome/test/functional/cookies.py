#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
    cookie_url = pyauto.GURL(self.GetFileURLForPath(
        os.path.join(self.DataDir(), 'title1.html')))
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
    file_url = self.GetFileURLForPath(
        os.path.join(self.DataDir(), 'setcookie.html'))
    self.assertFalse(self.GetCookie(pyauto.GURL(file_url)))
    # Incognito window
    self._CookieCheckIncognitoWindow(file_url)
    # Regular window
    self.NavigateToURL(file_url)
    self.assertEqual('name=Good', self.GetCookie(pyauto.GURL(file_url)))
    # Restart and verify that cookie persists
    self.RestartBrowser(clear_profile=False)
    self.assertEqual('name=Good', self.GetCookie(pyauto.GURL(file_url)))


if __name__ == '__main__':
  pyauto_functional.Main()
