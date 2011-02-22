#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

import pyauto_functional
import pyauto


class ShortcutsTest(pyauto.PyUITest):
  """Test for browser shortcuts.

  No tests for print, save page as... shortcuts as they involve interaction
  with OS native dialogs.
  """

  def testNewTabShortcut(self):
    """Verify new tab shortcut."""
    self.RunCommand(pyauto.IDC_NEW_TAB)
    self.assertEqual(2, self.GetTabCount(), msg='Can not open a new tab.')

  def testCloseTabShortcut(self):
    """Verify close tab shortcut."""
    self.RunCommand(pyauto.IDC_NEW_TAB)
    self.assertEqual(2, self.GetTabCount(), msg='Can not open a new tab.')
    self.RunCommand(pyauto.IDC_CLOSE_TAB)
    self.assertEqual(1, self.GetTabCount(), msg='Can not close a tab.')

  def testReopenClosedTabShortcut(self):
    """Verify reopen closed tab shortcut opens recently closed tab."""
    self.RunCommand(pyauto.IDC_NEW_TAB)
    url = self.GetFileURLForPath(os.path.join(self.DataDir(), 'title2.html'))
    self.NavigateToURL(url)
    title = self.GetActiveTabTitle()
    self.GetBrowserWindow(0).GetTab(1).Close()
    self.assertEqual(1, self.GetTabCount(), msg='Can not close a tab.')
    # Verify shortcut reopens the correct tab.
    self.RunCommand(pyauto.IDC_RESTORE_TAB)
    self.assertEqual(2, self.GetTabCount(), msg='Can not restore a tab.')
    self.assertEqual(title, self.GetActiveTabTitle())

  def testNewWindowShortcut(self):
    """Verify new window shortcut."""
    self.RunCommand(pyauto.IDC_NEW_WINDOW)
    self.assertEquals(2, self.GetBrowserWindowCount())

  def testNewIncognitoWindowShortcut(self):
    """Verify new incognito window shortcut launches incognito window."""
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
    self.assertEqual(2, self.GetBrowserWindowCount())
    # Check if it is incognito by checking history.
    assert not self.GetHistoryInfo().History(), 'Expecting clean history.'
    url = self.GetFileURLForPath(os.path.join(self.DataDir(), 'title2.html'))
    self.NavigateToURL(url, 1, 0)
    self.assertEqual(0, len(self.GetHistoryInfo().History()))

  def testFindShortcut(self):
    """Verify find in box shortcut."""
    self.ApplyAccelerator(pyauto.IDC_FIND)
    self.assertTrue(self.WaitUntil(lambda: self.IsFindInPageVisible()),
                    msg='Find in box is not visible.')

  def testAlwaysShowBookmarksBarShortcut(self):
    """Verify always show bookmarks bar shortcut."""
    # Show bookmark bar.
    self.ApplyAccelerator(pyauto.IDC_SHOW_BOOKMARK_BAR)
    self.assertTrue(self.WaitUntil(lambda: self.GetBookmarkBarVisibility()),
                    msg='Bookmarks bar is not visible.')
    # Hide bookmark bar.
    self.ApplyAccelerator(pyauto.IDC_SHOW_BOOKMARK_BAR)
    self.assertTrue(self.WaitUntil(lambda:
        self.GetBookmarkBarVisibility() is False),
        msg='Bookmarks bar is visible.')

  # TODO: Task Manager Shortcut. crbug.com/73454

  def testViewSourceShortcut(self):
    """Verify view source shortcut."""
    self.ApplyAccelerator(pyauto.IDC_VIEW_SOURCE)
    self.assertEqual(2, self.GetTabCount(), msg='Cannot View Source.')
    self.assertEqual('view-source:about:blank', self.GetActiveTabURL().spec(),
                      msg='View Source URL is not correct.')

  def testDeveloperToolsShortcut(self):
    """Verify developer tools shortcut opens developer tools window.."""
    # Setting the pref to undock devtools so that it can be seen
    # as a separate window.
    self.SetPrefs(pyauto.kDevToolsOpenDocked, False)
    self.ApplyAccelerator(pyauto.IDC_DEV_TOOLS)
    self.assertEqual(2, self.GetBrowserWindowCount(),
                     msg='DevTools window is not open.')

  def testJavaScriptConsoleShortcut(self):
    """Verify javascript console shortcut opens developer tools window.
    We can not check if console is open or not.
    We are making sure at least the shortcut launches developer tools window.
    """
    # Setting the pref to undock devtools so that it can be seen
    # as a separate window.
    self.SetPrefs(pyauto.kDevToolsOpenDocked, False)
    self.ApplyAccelerator(pyauto.IDC_DEV_TOOLS_CONSOLE)
    self.assertEqual(2, self.GetBrowserWindowCount(),
                     msg='DevTools window is not open.')

  def testHistoryShortcut(self):
    """Verify history shortcut opens history page."""
    self.RunCommand(pyauto.IDC_SHOW_HISTORY)
    self.assertEqual('History', self.GetActiveTabTitle(),
                     msg='History page was not opened.')

  def testDownloadsShortcut(self):
    """Verify downloads shortcut opens downloads page."""
    self.RunCommand(pyauto.IDC_SHOW_DOWNLOADS)
    self.assertEqual('Downloads', self.GetActiveTabTitle(),
                     msg='Downloads page was not opened.')

  def testHelpShortcut(self):
    """Verify help shortcut opens help page."""
    self.ApplyAccelerator(pyauto.IDC_HELP_PAGE)
    self.assertTrue(self.WaitUntil(lambda: self.GetActiveTabTitle(),
                    expect_retval='Google Chrome Help'),
                    msg='Google Chrome help page has not opened.')


if __name__ == '__main__':
  pyauto_functional.Main()
