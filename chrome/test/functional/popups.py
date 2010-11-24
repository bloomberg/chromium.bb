#!/usr/bin/python

# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

import pyauto_functional  # Must be imported before pyauto
import pyauto


class PopupsTest(pyauto.PyUITest):
  """TestCase for Popup blocking."""

  def Debug(self):
    """Test method for experimentation.

    This method will not run automatically.
    """
    import pprint
    pp = pprint.PrettyPrinter(indent=2)
    while True:
      raw_input('Interact with the browser and hit <enter>')
      pp.pprint(self.GetBlockedPopupsInfo())

  def testPopupBlockerEnabled(self):
    """Verify popup blocking is enabled."""
    self.assertFalse(self.GetBlockedPopupsInfo(),
                     msg='Should have no blocked popups on startup')
    file_url = self.GetFileURLForPath(os.path.join(
        self.DataDir(), 'popup_blocker', 'popup-blocked-to-post-blank.html'))
    self.NavigateToURL(file_url)
    blocked_popups = self.GetBlockedPopupsInfo()
    self.assertEqual(1, len(blocked_popups), msg='Popup not blocked')
    # It might take a while for the title to get set. Don't need to check it.
    # self.assertEqual('Popup Success!', blocked_popups[0]['title'])

  def testLaunchBlockedPopup(self):
    """Verify that a blocked popup can be unblocked."""
    file_url = self.GetFileURLForPath(os.path.join(
        self.DataDir(), 'popup_blocker', 'popup-blocked-to-post-blank.html'))
    self.NavigateToURL(file_url)
    self.assertEqual(1, len(self.GetBlockedPopupsInfo()))
    self.UnblockAndLaunchBlockedPopup(0)
    # Verify that no more popups are blocked
    self.assertFalse(self.GetBlockedPopupsInfo())
    # Verify that popup window was created
    self.assertEqual(2, self.GetBrowserWindowCount(),
                     msg='Popup could not be launched');
    self.assertEqual('Popup Success!', self.GetActiveTabTitle(1))

  def testPopupBlockerInIncognito(self):
    """Verify popup blocking is enabled in incognito windows."""
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
    file_url = self.GetFileURLForPath(os.path.join(
        self.DataDir(), 'popup_blocker', 'popup-blocked-to-post-blank.html'))
    self.NavigateToURL(file_url, 1, 0)
    blocked_popups = self.GetBlockedPopupsInfo(tab_index=0, windex=1)
    self.assertEqual(1, len(blocked_popups), msg='Popup not blocked')

  def testLaunchBlockedPopupInIncognito(self):
    """Verify that a blocked popup can be unblocked in incognito."""
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
    file_url = self.GetFileURLForPath(os.path.join(
        self.DataDir(), 'popup_blocker', 'popup-blocked-to-post-blank.html'))
    self.NavigateToURL(file_url, 1, 0)
    self.assertEqual(1, len(self.GetBlockedPopupsInfo(tab_index=0, windex=1)))
    self.UnblockAndLaunchBlockedPopup(0, tab_index=0, windex=1)
    # Verify that no more popups are blocked
    self.assertFalse(self.GetBlockedPopupsInfo(tab_index=0, windex=1))
    # Verify that popup window was created
    self.assertEqual(3, self.GetBrowserWindowCount(),
                     msg='Popup could not be launched');
    self.assertEqual('Popup Success!', self.GetActiveTabTitle(2))


if __name__ == '__main__':
  pyauto_functional.Main()
