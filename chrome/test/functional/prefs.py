#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

import pyauto_functional  # Must be imported before pyauto
import pyauto


class PrefsTest(pyauto.PyUITest):
  """TestCase for Preferences."""

  def testSessionRestore(self):
    url1 = 'http://www.google.com/'
    url2 = 'http://news.google.com/'
    self.NavigateToURL(url1)
    self.AppendTab(pyauto.GURL(url2))
    num_tabs = self.GetTabCount()
    # Set pref to restore session on startup
    browser = self.GetBrowserWindow(0)
    browser.SetIntPreference(pyauto.kRestoreOnStartup, 1)
    logging.debug('Setting %s to 1' % pyauto.kRestoreOnStartup)
    self.CloseBrowserAndServer()   # Close browser
    self.set_clear_profile(False)  # Do not clear profile on next startup
    self.LaunchBrowserAndServer()  # Reopen browser
    self.assertEqual(num_tabs, self.GetTabCount())
    self.ActivateTab(0)
    self.assertEqual(url1, self.GetActiveTabURL().spec())
    self.ActivateTab(1)
    self.assertEqual(url2, self.GetActiveTabURL().spec())
    self.set_clear_profile(True)  # Restore the flag to clear profile next


if __name__ == '__main__':
  pyauto_functional.Main()

