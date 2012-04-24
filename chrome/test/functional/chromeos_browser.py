#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pyauto_functional # pyauto_functional must come before pyauto.
import pyauto
import test_utils


class ChromeosBrowserTest(pyauto.PyUITest):

  def testCannotCloseLastIncognito(self):
    """Verify that last incognito window cannot be closed if it's the
    last window"""
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
    self.assertTrue(self.GetBrowserInfo()['windows'][1]['incognito'],
                    msg='Incognito window is not displayed')

    self.CloseBrowserWindow(0)
    info = self.GetBrowserInfo()['windows']
    self.assertEqual(1, len(info))
    url = info[0]['tabs'][0]['url']
    self.assertEqual('chrome://newtab/', url,
                     msg='Unexpected URL: %s' % url)
    self.assertTrue(info[0]['incognito'],
                    msg='Incognito window is not displayed.')

  def testCrashBrowser(self):
    """Verify that after broswer crash is recovered, user can still navigate
    to other URL."""
    test_utils.CrashBrowser(self)
    self.RestartBrowser(clear_profile=False)
    url = self.GetHttpURLForDataPath('english_page.html')
    self.NavigateToURL(url)
    self.assertEqual('This page is in English', self.GetActiveTabTitle())

  def testFullScreen(self):
    """Verify that a browser window can enter and exit full screen mode."""
    self.ApplyAccelerator(pyauto.IDC_FULLSCREEN)
    self.assertTrue(self.WaitUntil(lambda:
                    self.GetBrowserInfo()['windows'][0]['fullscreen']),
                    msg='Full Screen is not displayed.')

    self.ApplyAccelerator(pyauto.IDC_FULLSCREEN)
    self.assertTrue(self.WaitUntil(lambda: not
                    self.GetBrowserInfo()['windows'][0]['fullscreen']),
                    msg='Normal screen is not displayed.')


if __name__ == '__main__':
  pyauto_functional.Main()
