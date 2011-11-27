#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import time

import pyauto_functional  # Must be imported before pyauto
import pyauto
import test_utils


class BrowsingDataTest(pyauto.PyUITest):
  """Tests that clearing browsing data works correctly."""

  def testClearHistory(self):
    """Verify that clearing the history works."""
    self.NavigateToURL(self.GetFileURLForDataPath('title2.html'))
    history = self.GetHistoryInfo().History()
    self.assertEqual(1, len(history))

    self.ClearBrowsingData(['HISTORY'], 'EVERYTHING')
    history = self.GetHistoryInfo().History()
    self.assertFalse(history)

  def testClearCookies(self):
    """Verify clearing cookies."""
    # First build up some data with cookies.
    cookie_url = pyauto.GURL(self.GetFileURLForDataPath('title2.html'))
    cookie_val = 'foo=bar'
    self.SetCookie(cookie_url, cookie_val)
    self.assertEqual(cookie_val, self.GetCookie(cookie_url))
    # Then clear the cookies.
    self.ClearBrowsingData(['COOKIES'], 'EVERYTHING')
    cookie_data = self.GetCookie(cookie_url)
    self.assertFalse(cookie_data)

  def testClearDownloads(self):
    """Verify clearing downloads."""
    zip_file = 'a_zip_file.zip'
    # First build up some data with downloads.
    test_utils.DownloadFileFromDownloadsDataDir(self, zip_file)
    self.assertEqual(1, len(self.GetDownloadsInfo().Downloads()))
    # Clear the downloads and verify they're gone.
    self.ClearBrowsingData(['DOWNLOADS'], 'EVERYTHING')
    downloads = self.GetDownloadsInfo().Downloads()
    self.assertFalse(downloads)

  def testClearHistoryPastHour(self):
    """Verify that clearing the history of the past hour works and does not
       clear history older than one hour.
    """
    title = 'Google'
    num_secs_in_hour = 3600

    # Forge a history item for two hours ago
    now = time.time()
    last_hour = now - (2 * num_secs_in_hour)
    self.AddHistoryItem({'title': title,
                         'url': 'http://www.google.com',
                         'time': last_hour})

    # Forge a history item for right now
    self.AddHistoryItem({'title': 'This Will Be Cleared',
                         'url': 'http://www.dev.chromium.org',
                         'time': now})

    history = self.GetHistoryInfo().History()
    self.assertEqual(2, len(history))

    self.ClearBrowsingData(['HISTORY'], 'LAST_HOUR')
    history = self.GetHistoryInfo().History()
    self.assertEqual(1, len(history))
    self.assertEqual(title, history[0]['title'])

  def testClearHistoryAndDownloads(self):
    """Verify that we can clear history and downloads at the same time."""
    zip_file = 'a_zip_file.zip'
    # First build up some history and download something.
    self.NavigateToURL(self.GetFileURLForDataPath('title2.html'))
    test_utils.DownloadFileFromDownloadsDataDir(self, zip_file)

    # Verify that the history and download exist.
    self.assertEqual(1, len(self.GetHistoryInfo().History()))
    self.assertEqual(1, len(self.GetDownloadsInfo().Downloads()))

    # Clear the history and downloads and verify they're both gone.
    self.ClearBrowsingData(['HISTORY', 'DOWNLOADS'], 'EVERYTHING')
    history = self.GetHistoryInfo().History()
    downloads = self.GetDownloadsInfo().Downloads()
    self.assertFalse(history)
    self.assertFalse(downloads)

  def testClearingAccuracy(self):
    """Verify that clearing one thing does not clear another."""
    zip_file = 'a_zip_file.zip'
    # First build up some history and download something.
    self.NavigateToURL(self.GetFileURLForDataPath('title2.html'))
    test_utils.DownloadFileFromDownloadsDataDir(self, zip_file)
    # Verify that the history and download exist.
    self.assertEqual(1, len(self.GetHistoryInfo().History()))
    self.assertEqual(1, len(self.GetDownloadsInfo().Downloads()))

    # Clear history and verify that downloads exist.
    self.ClearBrowsingData(['HISTORY'], 'EVERYTHING')
    history = self.GetHistoryInfo().History()
    downloads = self.GetDownloadsInfo().Downloads()
    self.assertFalse(history)
    self.assertEqual(1, len(downloads))

  def testClearAutofillData(self):
    """Verify that clearing autofill form data works."""

    # Add new profiles
    profiles = [{'NAME_FIRST': ['Bill',],
                 'NAME_LAST': ['Ding',],
                 'ADDRESS_HOME_CITY': ['Mountain View',],
                 'ADDRESS_HOME_STATE': ['CA',],
                 'ADDRESS_HOME_ZIP': ['94043',],}]
    credit_cards = [{'CREDIT_CARD_NAME': 'Bill Ding',
                     'CREDIT_CARD_NUMBER': '4111111111111111',
                     'CREDIT_CARD_EXP_MONTH': '01',
                     'CREDIT_CARD_EXP_4_DIGIT_YEAR': '2012'}]

    self.FillAutofillProfile(profiles=profiles, credit_cards=credit_cards)

    # Verify that the added profiles exist.
    profile = self.GetAutofillProfile()
    self.assertEqual(profiles, profile['profiles'])
    self.assertEqual(credit_cards, profile['credit_cards'])

    # Clear the browser's autofill form data.
    self.ClearBrowsingData(['FORM_DATA'], 'EVERYTHING')

    def _ProfileCount(type):
      profile = self.GetAutofillProfile()
      return len(profile[type])

    # Verify that all profiles have been cleared.
    self.assertTrue(self.WaitUntil(lambda: 0 == _ProfileCount('profiles')))
    self.assertTrue(self.WaitUntil(lambda: 0 == _ProfileCount('credit_cards')))


if __name__ == '__main__':
  pyauto_functional.Main()
