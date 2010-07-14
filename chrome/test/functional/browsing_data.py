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

  def testClearHistory(self):
    """Verify that clearing the history works."""
    url = self.GetFileURLForPath(os.path.join(self.DataDir(), 'title2.html'))
    self.NavigateToURL(url)

    history = self.GetHistoryInfo().History()
    self.assertEqual(1, len(history))

    self.ClearBrowsingData(['HISTORY'], 'EVERYTHING')
    history = self.GetHistoryInfo().History()
    self.assertEqual(0, len(history))

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
    url = self.GetFileURLForPath(os.path.join(self.DataDir(), 'title2.html'))
    self.NavigateToURL(url)

    test_dir = os.path.join(os.path.abspath(self.DataDir()), 'downloads')
    file_path = os.path.join(test_dir, 'a_zip_file.zip')
    file_url = self.GetFileURLForPath(file_path)
    downloaded_pkg = os.path.join(self.GetDownloadDirectory().value(),
                                  'a_zip_file.zip')
    os.path.exists(downloaded_pkg) and os.remove(downloaded_pkg)
    self.DownloadAndWaitForStart(file_url)
    self.WaitForAllDownloadsToComplete()

    # Verify that the history and download exist.
    self.assertEqual(1, len(self.GetHistoryInfo().History()))
    self.assertEqual(1, len(self.GetDownloadsInfo().Downloads()))

    # Clear the history and downloads and verify they're both gone.
    self.ClearBrowsingData(['HISTORY', 'DOWNLOADS'], 'EVERYTHING')
    history = self.GetHistoryInfo().History()
    downloads = self.GetDownloadsInfo().Downloads()
    self.assertEqual(0, len(history))
    self.assertEqual(0, len(downloads))


if __name__ == '__main__':
  pyauto_functional.Main()
