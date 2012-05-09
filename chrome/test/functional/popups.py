#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import logging

import pyauto_functional  # Must be imported before pyauto
import pyauto
import test_utils

class PopupsTest(pyauto.PyUITest):
  """TestCase for Popup blocking."""

  def Debug(self):
    """Test method for experimentation.

    This method will not run automatically.
    """
    while True:
      raw_input('Interact with the browser and hit <enter>')
      self.pprint(self.GetBlockedPopupsInfo())

  def testPopupBlockerEnabled(self):
    """Verify popup blocking is enabled."""
    self.assertFalse(self.GetBlockedPopupsInfo(),
                     msg='Should have no blocked popups on startup')
    file_url = self.GetFileURLForPath(os.path.join(
        self.DataDir(), 'popup_blocker', 'popup-window-open.html'))
    self.NavigateToURL(file_url)
    blocked_popups = self.GetBlockedPopupsInfo()
    self.assertEqual(1, len(blocked_popups), msg='Popup not blocked')
    # It might take a while for the title to get set. Don't need to check it.
    # self.assertEqual('Popup Success!', blocked_popups[0]['title'])

  def testLaunchBlockedPopup(self):
    """Verify that a blocked popup can be unblocked."""
    file_url = self.GetFileURLForPath(os.path.join(
        self.DataDir(), 'popup_blocker', 'popup-window-open.html'))
    self.NavigateToURL(file_url)
    self.assertEqual(1, len(self.GetBlockedPopupsInfo()))
    self.UnblockAndLaunchBlockedPopup(0)
    # Verify that no more popups are blocked
    self.assertFalse(self.GetBlockedPopupsInfo())
    # Verify that popup window was created
    self.assertEqual(2, self.GetBrowserWindowCount(),
                     msg='Popup could not be launched');
    # Wait until popup title is read correctly.
    self.assertTrue(self.WaitUntil(lambda:
        self.GetActiveTabTitle(1), expect_retval='Popup Success!'),
        msg='Popup title is wrong.')

  def testPopupBlockerInIncognito(self):
    """Verify popup blocking is enabled in incognito windows."""
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
    file_url = self.GetFileURLForPath(os.path.join(
        self.DataDir(), 'popup_blocker', 'popup-window-open.html'))
    self.NavigateToURL(file_url, 1, 0)
    blocked_popups = self.GetBlockedPopupsInfo(tab_index=0, windex=1)
    self.assertEqual(1, len(blocked_popups), msg='Popup not blocked')

  def testLaunchBlockedPopupInIncognito(self):
    """Verify that a blocked popup can be unblocked in incognito."""
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
    self.assertTrue(2, self.GetBrowserWindowCount())
    file_url = self.GetFileURLForPath(os.path.join(
        self.DataDir(), 'popup_blocker', 'popup-window-open.html'))
    self.NavigateToURL(file_url, 1, 0)
    self.assertEquals('Popup created using window.open',
                      self.GetActiveTabTitle(window_index=1))
    # Wait until the popup is blocked
    self.assertTrue(self.WaitUntil(lambda:
        len(self.GetBlockedPopupsInfo(tab_index=0, windex=1)) is 1),
        msg='Popup not blocked')
    self.UnblockAndLaunchBlockedPopup(0, tab_index=0, windex=1)
    # Verify that no more popups are blocked
    self.assertFalse(self.GetBlockedPopupsInfo(tab_index=0, windex=1))
    # Verify that popup window was created
    self.assertEqual(3, self.GetBrowserWindowCount(),
                     msg='Popup could not be launched');
    # Wait until popup title is read correctly.
    self.assertTrue(self.WaitUntil(lambda:
        self.GetActiveTabTitle(2), expect_retval='Popup Success!'),
        msg='Popup title is wrong.')

  def testMultiplePopups(self):
    """Verify multiple popups are blocked."""
    url = self.GetHttpURLForDataPath(
        os.path.join('pyauto_private', 'popup_blocker',
                     'PopupTest1.html'))
    self.NavigateToURL(url)
    self.assertEqual(6, len(self.GetBlockedPopupsInfo()),
                     msg='Did not block 6 popups.')

  def testPopupBlockedEverySec(self):
    """Verify that a popup is blocked every second."""
    url = self.GetHttpURLForDataPath(
        os.path.join('pyauto_private', 'popup_blocker',
                     'PopupTest4.html'))
    self.NavigateToURL(url)
    self.assertTrue(self.WaitUntil(lambda: len(self.GetBlockedPopupsInfo()),
                                   expect_retval=2))

  def _SetPopupsException(self):
    """Set an exception to allow popups from www.popuptest.com."""
    value = {'[*.]www.popuptest.com,*': {'popups': 1}}
    return self.SetPrefs(pyauto.kContentSettingsPatternPairs, value)

  def testAllowPopupsFromExternalSite(self):
    """Verify that popups are allowed from an external website."""
    self._SetPopupsException()
    self.NavigateToURL('http://www.popuptest.com/popuptest1.html')
    self.assertEqual(7, self.GetBrowserWindowCount(),
                     msg='Popups did not launch from the external site.')

  def testPopupsLaunchUponBrowserBackButton(self):
    """Verify that popups are launched on browser back button."""
    self._SetPopupsException()
    url = self.GetHttpURLForDataPath(
        os.path.join('popup_blocker', 'popup-blocked-to-post-blank.html'))
    self.NavigateToURL(url)
    self.NavigateToURL('http://www.popuptest.com/popuptest1.html')
    self.assertEqual(7, self.GetBrowserWindowCount(),
                     msg='Popups did not launch from the external site.')
    self.GetBrowserWindow(0).GetTab(0).GoBack()
    # Check if two additional popups launch when navigating away from the page.
    self.assertEqual(9, self.GetBrowserWindowCount(),
                     msg='Additional popups did not launch.')

  def testPopupsLaunchWhenTabIsClosed(self):
    """Verify popups are launched when closing a tab."""
    self._SetPopupsException()
    self.AppendTab(pyauto.GURL('http://www.popuptest.com/popuptest12.html'))
    self.assertEqual(4, self.GetBrowserWindowCount(),
                     msg='Popups did not launch from the external site.')
    self.GetBrowserWindow(0).GetTab(1).Close(True)
    # Check if last popup is launched when the tab is closed.
    self.assertEqual(5, self.GetBrowserWindowCount(),
                     msg='Last popup did not launch when the tab is closed.')

  def testUnblockedPopupShowsInHistory(self):
    """Verify that when you unblock popup, the popup shows in history."""
    file_url = self.GetFileURLForDataPath('popup_blocker',
                                          'popup-window-open.html')
    self.NavigateToURL(file_url)
    self.assertEqual(1, len(self.GetBlockedPopupsInfo()))
    self.UnblockAndLaunchBlockedPopup(0)
    self.assertTrue(self.WaitUntil(
        lambda: len(self.GetHistoryInfo().History()) == 2))
    self.assertEqual('Popup Success!',
        self.GetHistoryInfo().History()[0]['title'])

  def testBlockedPopupNotShowInHistory(self):
    """Verify that a blocked popup does not show up in history."""
    file_url = self.GetFileURLForDataPath('popup_blocker',
                                          'popup-window-open.html')
    self.NavigateToURL(file_url)
    self.assertEqual(1, len(self.GetBlockedPopupsInfo()))
    self.assertEqual(1, len(self.GetHistoryInfo().History()))

  def testUnblockedPopupAddedToOmnibox(self):
    """Verify that an unblocked popup shows up in omnibox suggestions."""
    file_url = self.GetFileURLForDataPath('popup_blocker',
                                          'popup-window-open.html')
    self.NavigateToURL(file_url)
    self.assertEqual(1, len(self.GetBlockedPopupsInfo()))
    self.UnblockAndLaunchBlockedPopup(0)
    search_string = 'data:text/html,<title>Popup Success!</title> \
                  you should not see this message if popup blocker is enabled'
    matches = test_utils.GetOmniboxMatchesFor(self, search_string)
    self.assertEqual(search_string, matches[0]['destination_url'])
    self.assertEqual(search_string, matches[0]['contents'])


if __name__ == '__main__':
  pyauto_functional.Main()
