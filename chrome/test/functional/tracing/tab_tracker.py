# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import uuid


class TabTracker(object):
  """Uniquely track tabs within a window.

  This allows the creation of tabs whose indices can be
  determined even after lower indexed tabs have been closed, therefore changing
  that tab's index.

  This is accomplished via a containing window which is created and tracked via
  the window's index. As a result of this, all calls to open and close tabs in
  this TabTracker's window must go through the appropriate instance of the
  TabTracker. Also note that if a lower indexed window is closed after this
  TabTracker is instantiated, this TabTracker will lose track of its window
  """

  def __init__(self, browser, visible=False):
    """
    Args:
      browser: an instance of PyUITest
      visible: whether or not this TabTracker's window will be visible
    """
    # A binary search tree would be faster, but this is easier to write.
    # If this needs to become faster, understand that the important operations
    # here are append, arbitrary deletion and searching.
    self._uuids = [None]
    self._window_idx = browser.GetBrowserWindowCount()
    self._browser = browser
    browser.OpenNewBrowserWindow(visible)
    # We leave the 0'th tab empty to have something to close on __del__

  def __del__(self):
    self._browser.CloseBrowserWindow(self._window_idx)

  def CreateTab(self, url='about:blank'):
    """Create a tracked tab and return its uuid.

    Args:
      url: a URL to navigate to

    Returns:
      a uuid uniquely identifying that tab within this TabTracker
    """
    self._browser.AppendTab(url, self._window_idx)
    # We use uuids here rather than a monotonic integer to prevent confusion
    # with the tab index.
    tab_uuid = uuid.uuid4()
    self._uuids.append(tab_uuid)
    return tab_uuid

  def ReleaseTab(self, tab_uuid):
    """Release and close a tab tracked by this TabTracker.

    Args:
      tab_uuid: the uuid of the tab to close
    """
    idx = self.GetTabIndex(tab_uuid)
    self._browser.CloseTab(tab_index=idx, windex=self._window_idx)
    del self._uuids[idx]

  def GetTabIndex(self, tab_uuid):
    """Get the index of a tracked tab within this TabTracker's window.

    Args:
      tab_uuid: the uuid of the tab to close

    Returns:
      the index of the tab within this TabTracker's window
    """
    return self._uuids.index(tab_uuid)

  def GetWindowIndex(self):
    """Get the index of this TabTracker's window.

    Returns:
      the index of this TabTracker's window
    """
    return self._window_idx
