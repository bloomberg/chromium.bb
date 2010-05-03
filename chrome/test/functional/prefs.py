#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os

import pyauto_functional  # Must be imported before pyauto
import pyauto


class PrefsTest(pyauto.PyUITest):
  """TestCase for Preferences."""

  def testSessionRestore(self):
    """Test session restore preference."""
    url1 = 'http://www.google.com/'
    url2 = 'http://news.google.com/'
    self.NavigateToURL(url1)
    self.AppendTab(pyauto.GURL(url2))
    num_tabs = self.GetTabCount()
    # Set pref to restore session on startup
    self.SetPrefs(pyauto.kRestoreOnStartup, 1)
    logging.debug('Setting %s to 1' % pyauto.kRestoreOnStartup)
    self.RestartBrowser(clear_profile=False)
    # Verify
    self.assertEqual(self.GetPrefsInfo().Prefs(pyauto.kRestoreOnStartup), 1)
    self.assertEqual(num_tabs, self.GetTabCount())
    self.ActivateTab(0)
    self.assertEqual(url1, self.GetActiveTabURL().spec())
    self.ActivateTab(1)
    self.assertEqual(url2, self.GetActiveTabURL().spec())

  def Debug(self):
    """Test method for experimentation.

    This method will not run automatically.
    """
    import pprint
    pp = pprint.PrettyPrinter(indent=2)
    while True:
      raw_input('Interact with the browser and hit <enter> to dump prefs... ')
      pp.pprint(self.GetPrefsInfo().Prefs())

  def testSessionRestoreURLs(self):
    """Verify restore URLs preference."""
    url1 = self.GetFileURLForPath(os.path.join(self.DataDir(), 'title1.html'))
    url2 = self.GetFileURLForPath(os.path.join(self.DataDir(), 'title2.html'))
    # Set pref to restore given URLs on startup
    self.SetPrefs(pyauto.kRestoreOnStartup, 4)  # 4 is for restoring URLs
    self.SetPrefs(pyauto.kURLsToRestoreOnStartup, [url1, url2])
    self.RestartBrowser(clear_profile=False)
    # Verify
    self.assertEqual(self.GetPrefsInfo().Prefs(pyauto.kRestoreOnStartup), 4)
    self.assertEqual(2, self.GetTabCount())
    self.ActivateTab(0)
    self.assertEqual(url1, self.GetActiveTabURL().spec())
    self.ActivateTab(1)
    self.assertEqual(url2, self.GetActiveTabURL().spec())

  def testSessionRestoreShowBookmarkBar(self):
    """Verify restore for bookmark bar visibility."""
    assert not self.GetPrefsInfo().Prefs(pyauto.kShowBookmarkBar)
    self.SetPrefs(pyauto.kShowBookmarkBar, True)
    self.assertEqual(True, self.GetPrefsInfo().Prefs(pyauto.kShowBookmarkBar))
    self.RestartBrowser(clear_profile=False)
    self.assertEqual(True, self.GetPrefsInfo().Prefs(pyauto.kShowBookmarkBar))
    self.assertTrue(self.GetBookmarkBarVisibility())


if __name__ == '__main__':
  pyauto_functional.Main()

