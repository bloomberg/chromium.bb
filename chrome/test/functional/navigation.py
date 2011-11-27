#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

import pyauto_functional  # Must be imported before pyauto
import pyauto


class NavigationTest(pyauto.PyUITest):
  """TestCase for Navigation."""

  def _ObtainURLList(self):
    """Get a list of file:// urls for use in this test case."""
    urls = []
    for fname in ['title1.html', 'title2.html', 'title3.html']:
      urls.append(self.GetFileURLForPath(os.path.join(self.DataDir(), fname)))
    return urls

  def _OpenTabsInWindow(self, urls, windex):
    """Open, verify given urls in the window at the given index."""
    for url in self._ObtainURLList():
      self.AppendTab(pyauto.GURL(url), windex)
      self.assertEqual(url, self.GetActiveTabURL(windex).spec())
    self.assertEqual(len(urls) + 1, self.GetTabCount(windex))
    for i in range(len(urls)):
      self.ActivateTab(i + 1, windex)  # ignore first tab
      self.assertEqual(self.GetActiveTabURL(windex).spec(), urls[i])

  def testMultipleTabsAndWindows(self):
    """Verify multiple tabs and windows."""
    self.assertEqual(1, self.GetBrowserWindowCount())
    urls = self._ObtainURLList()
    self._OpenTabsInWindow(urls, 0)
    more_windows = 3
    for windex in range(1, more_windows + 1):
      self.OpenNewBrowserWindow(True)
      self.assertEqual(1 + windex, self.GetBrowserWindowCount())
      self._OpenTabsInWindow(urls, windex)

  def testTabsOpenClose(self):
    """Verify tabs open/close."""
    urls = self._ObtainURLList()
    def _OpenCloseTabsInWindow(windex):
      """Open/close tabs in window at given index."""
      self.AppendTab(pyauto.GURL(urls[0]), windex)
      self.assertEqual(2, self.GetTabCount(windex))
      self.AppendTab(pyauto.GURL(urls[1]), windex)
      self.assertEqual(3, self.GetTabCount(windex))
      self.GetBrowserWindow(windex).GetTab(2).Close(True)
      self.assertEqual(2, self.GetTabCount(windex))
      self.GetBrowserWindow(windex).GetTab(1).Close(True)
      self.assertEqual(1, self.GetTabCount(windex))
    _OpenCloseTabsInWindow(0)
    self.OpenNewBrowserWindow(True)
    _OpenCloseTabsInWindow(1)

  def testForwardBackward(self):
    """Verify forward/backward actions."""
    urls = self._ObtainURLList()
    assert len(urls) >= 3, 'Need at least 3 urls.'
    for url in urls:
      self.NavigateToURL(url)
    tab = self.GetBrowserWindow(0).GetTab(0)
    self.assertEqual(self.GetActiveTabURL().spec(), urls[-1])
    for i in [-2, -3]:
      tab.GoBack()
      self.assertEqual(self.GetActiveTabURL().spec(), urls[i])
    for i in [-2, -1]:
      tab.GoForward()
      self.assertEqual(self.GetActiveTabURL().spec(), urls[i])

  def testCanDuplicateTab(self):
    """Open a page, duplicate it and make sure the new tab was duplicated"""
    urls = self._ObtainURLList()
    assert len(urls) >= 3, 'Need at least 3 urls.'
    self.NavigateToURL(urls[0])
    self.ApplyAccelerator(pyauto.IDC_DUPLICATE_TAB)
    self.assertEqual(self.GetTabCount(), 2)
    self.assertEqual(urls[0], self.GetActiveTabURL().spec())

  def testBrutalTabsAndWindows(self):
    """Open "many" windows and tabs."""
    urls = self._ObtainURLList()
    num_windows = 10
    orig_num_windows = self.GetBrowserWindowCount()
    for windex in range(1, num_windows):
      self.OpenNewBrowserWindow(True)
      self.assertEqual(orig_num_windows + windex, self.GetBrowserWindowCount())
   # Open many tabs in 1st window
    num_tabs = 20
    orig_num_tabs = self.GetTabCount(windex)
    for tindex in range(1, num_tabs):
      self.AppendTab(pyauto.GURL(urls[0]))
      self.assertEqual(orig_num_tabs + tindex, self.GetTabCount())


if __name__ == '__main__':
  pyauto_functional.Main()
