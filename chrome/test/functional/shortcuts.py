#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pyauto_functional
import pyauto


class ShortcutsTest(pyauto.PyUITest):
  """Test for browser shortcuts."""

  def testShortcuts(self):
    """Verify shortcuts work."""
    # Open a new tab.
    self.RunCommand(pyauto.IDC_NEW_TAB)
    self.assertEqual(2, self.GetTabCount())

    # Close a tab.
    self.RunCommand(pyauto.IDC_CLOSE_TAB)
    self.assertEqual(1, self.GetTabCount())

    # Open a new window.
    self.RunCommand(pyauto.IDC_NEW_WINDOW)
    self.assertEquals(2, self.GetBrowserWindowCount())

    # Close a window.
    self.RunCommand(pyauto.IDC_CLOSE_WINDOW, 1)
    self.assertEquals(1, self.GetBrowserWindowCount())

    # Open a new incognito window.
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
    self.assertEqual(2, self.GetBrowserWindowCount())
    self.CloseBrowserWindow(1)

    # Open find in box.
    self.ApplyAccelerator(pyauto.IDC_FIND)
    self.assertTrue(self.WaitUntil(lambda: self.IsFindInPageVisible()),
                    msg='Find in box is not visible.')

    # Always show Bookmark bar.
    self.ApplyAccelerator(pyauto.IDC_SHOW_BOOKMARK_BAR)
    self.assertTrue(self.WaitUntil(lambda: self.GetBookmarkBarVisibility()),
                    msg='Bookmark bar not visible.')

    # View Source.
    self.ApplyAccelerator(pyauto.IDC_VIEW_SOURCE)
    self.assertEqual(2, self.GetTabCount(), msg='Can\'t View Source.')
    self.assertEqual('view-source:about:blank', self.GetActiveTabURL().spec(),
                      msg='View Source URL is not correct.')
    self.GetBrowserWindow(0).GetTab(1).Close()

    # Open Developer Tools.
    # Setting the pref to undock devtools so that it can be seen
    # as a separate window.
    self.SetPrefs(pyauto.kDevToolsOpenDocked, False)
    self.ApplyAccelerator(pyauto.IDC_DEV_TOOLS)
    self.assertEqual(2, self.GetBrowserWindowCount(),
                     msg='DevTools window is not open.')
    self.CloseBrowserWindow(1)

    # Open Javascript Console.
    self.ApplyAccelerator(pyauto.IDC_DEV_TOOLS_CONSOLE)
    self.assertEqual(2, self.GetBrowserWindowCount(),
                     msg='DevTools console is not open.')
    self.CloseBrowserWindow(1)

    # Open History.
    self.RunCommand(pyauto.IDC_SHOW_HISTORY)
    self.assertEqual('History', self.GetActiveTabTitle(),
                     msg='History page was not opened.')
    self.GetBrowserWindow(0).GetTab(1).Close()

    # Open Downloads.
    self.RunCommand(pyauto.IDC_SHOW_DOWNLOADS)
    self.assertEqual('Downloads', self.GetActiveTabTitle(),
                     msg='Downloads page was not opened.')
    self.GetBrowserWindow(0).GetTab(1).Close()

    # Open Help page.
    self.ApplyAccelerator(pyauto.IDC_HELP_PAGE)
    self.assertTrue(self.WaitUntil(lambda: self.GetActiveTabTitle(),
                    expect_retval='Google Chrome Help'),
                    msg='Google Chrome help page has not opened.')
    self.GetBrowserWindow(0).GetTab(1).Close()


if __name__ == '__main__':
  pyauto_functional.Main()
