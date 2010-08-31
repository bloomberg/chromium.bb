#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import time

import pyauto_functional  # Must be imported before pyauto
import pyauto


class BrowsingDataTest(pyauto.PyUITest):
  """Tests that clearing browsing data works correctly."""

  def _DownloadTestFile(self, file_name):
    """Downloads the specified file from the 'downloads' directory and waits
       for downloads to complete.
    """
    test_dir = os.path.join(self.DataDir(), 'downloads')
    file_path = os.path.join(test_dir, file_name)
    file_url = self.GetFileURLForPath(file_path)
    downloaded_pkg = os.path.join(self.GetDownloadDirectory().value(),
                                  file_name)
    os.path.exists(downloaded_pkg) and os.remove(downloaded_pkg)
    self.DownloadAndWaitForStart(file_url)
    self.WaitForAllDownloadsToComplete()
    os.path.exists(downloaded_pkg) and os.remove(downloaded_pkg)

  def _GetURLForFile(self, file_name):
    """Returns the url for the file in the 'data' directory."""
    return self.GetFileURLForPath(os.path.join(self.DataDir(), file_name))

  def testClearHistory(self):
    """Verify that clearing the history works."""
    self.NavigateToURL(self._GetURLForFile('title2.html'))
    history = self.GetHistoryInfo().History()
    self.assertEqual(1, len(history))

    self.ClearBrowsingData(['HISTORY'], 'EVERYTHING')
    history = self.GetHistoryInfo().History()
    self.assertFalse(history)

  def testClearCookies(self):
    """Verify clearing cookies."""
    # First build up some data with cookies.
    cookie_url = pyauto.GURL(self._GetURLForFile('title2.html'))
    cookie_val = 'foo=bar'
    self.SetCookie(cookie_url, cookie_val)
    self.assertEqual(cookie_val, self.GetCookie(cookie_url))
    # Then clear the cookies.
    self.ClearBrowsingData(['COOKIES'], 'EVERYTHING')
    cookie_data = self.GetCookie(cookie_url)
    self.assertFalse(cookie_data)

  def testClearDownloads(self):
    """Verify clearing downloads."""
    # First build up some data with downloads.
    self._DownloadTestFile('a_zip_file.zip')
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
    # First build up some history and download something.
    self.NavigateToURL(self._GetURLForFile('title2.html'))
    self._DownloadTestFile('a_zip_file.zip')

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
    # First build up some history and download something.
    self.NavigateToURL(self._GetURLForFile('title2.html'))
    self._DownloadTestFile('a_zip_file.zip')

    # Verify that the history and download exist.
    self.assertEqual(1, len(self.GetHistoryInfo().History()))
    self.assertEqual(1, len(self.GetDownloadsInfo().Downloads()))

    # Clear history and verify that downloads exist.
    self.ClearBrowsingData(['HISTORY'], 'EVERYTHING')
    history = self.GetHistoryInfo().History()
    downloads = self.GetDownloadsInfo().Downloads()
    self.assertFalse(history)
    self.assertEqual(1, len(downloads))


if __name__ == '__main__':
  pyauto_functional.Main()
