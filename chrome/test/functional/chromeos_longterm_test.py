#!/usr/bin/env python

# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import time

import pyauto_functional
import pyauto
import pyauto_utils
import timer_queue


class ChromeOSLongTerm(pyauto.PyUITest):
  """Set of long running tests for ChromeOS.

  This class is comprised of several tests that perform long term tests.
  """

  def _ActivateTabWithURL(self, url):
    """Activates the window that has the given tab url.

    Args:
      url: The url of the tab to find.

    Returns:
      An array of the index values of the tab and window.  Returns None if the
      tab connot be found.
    """
    info = self.GetBrowserInfo()
    windows = info['windows']
    for window_index, window in enumerate(windows):
      tabs = window['tabs']
      for tab_index, tab in enumerate(tabs):
        tab['url'] = tab['url'].strip('/')
        if tab['url'] == url:
          self.ActivateTab(tab_index, window_index)
          return [tab_index, window_index]
    return None

  def _SetupLongTermWindow(self, long_term_pages):
    """Appends a list of tab to the current active window.

    Args:
      long_term_pages: The list of urls to open.
    """
    for url in long_term_pages:
      self.AppendTab(pyauto.GURL(url))

  def _RefreshLongTermWindow(self, long_term_pages):
    """ Refreshes all of the tabs from the given list.

    Args:
      long_term_pages: The list of urls to refresh.
    """
    for page in long_term_pages:
      long_index = self._ActivateTabWithURL(page)
      if not long_index:
        logging.info('Unable to find page with url: %s.')
      else:
        self.ActivateTab(long_index[0], long_index[1])
        self.ReloadActiveTab(long_index[1])

  def _ConfigureNewWindow(self, pages, incognito=False):
    """Setups a windows with multiple tabs running.

    This method acts as a state machine.  If a window containing a tab with the
    url of the first item of pages it closes that window.  If that window
    cannot be found then a new window with the urls in pages is opened.

    Args:
      pages: The list of urls to load.
    """
    page_index = self._ActivateTabWithURL(pages[0])
    if not page_index:
      # This means the pages do not exist, load them
      if incognito:
        self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
      else:
        self.OpenNewBrowserWindow(True)
      for url in pages:
        self.AppendTab(pyauto.GURL(url), self.GetBrowserWindowCount() - 1)
      # Cycle through the pages to make sure they render
      win = self.GetBrowserInfo()['windows'][self.GetBrowserWindowCount() - 1]
      for tab in win['tabs']:
        self.ActivateTab(tab['index'], self.GetBrowserWindowCount() - 1)
        # Give the plugin time to activate
        time.sleep(1.5)
    else:
      self.CloseBrowserWindow(page_index[1])

  def testLongTerm(self):
    """Main entry point for the long term tests.

    This method will spin in a while loop forever until it encounters a keyboard
    interrupt.  Other worker methods will be managed by the TimerQueue.
    """
    long_term_pages = ['http://news.google.com', 'http://www.engadget.com',
                       'http://www.washingtonpost.com']

    flash_pages = [
       'http://www.craftymind.com/factory/guimark2/FlashChartingTest.swf',
       'http://www.craftymind.com/factory/guimark2/FlashGamingTest.swf',
       'http://www.craftymind.com/factory/guimark2/FlashTextTest.swf']

    incognito_pages = ['http://www.msn.com', 'http://www.ebay.com',
                       'http://www.bu.edu', 'http://www.youtube.com']

    start_time = time.time()
    self._SetupLongTermWindow(long_term_pages)
    timers = timer_queue.TimerQueue()
    timers.AddTimer(self._ConfigureNewWindow, 90, args=(flash_pages,))
    timers.AddTimer(self._RefreshLongTermWindow, 30, args=(long_term_pages,))
    timers.AddTimer(self._ConfigureNewWindow, 15, args=(incognito_pages, True))
    timers.start()
    try:
      while True:
        if not timers.is_alive():
          logging.error('Timer queue died, shutting down.')
          return
        time.sleep(1)

    except KeyboardInterrupt:
      # Kill the timers
      timers.Stop()


if __name__ == '__main__':
  pyauto_functional.Main()
