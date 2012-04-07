#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pyauto_functional
import pyauto
import test_utils


class IndexedDBTest(pyauto.PyUITest):
  """Test of IndexedDB."""

  def testIndexedDBNullKeyPathPersistence(self):
    """Verify null key path persists after restarting browser."""

    url = self.GetHttpURLForDataPath('indexeddb', 'bug_90635.html')

    self.NavigateToURL(url + '#part1')
    self.assertTrue(self.WaitUntil(self.GetActiveTabTitle,
                                   expect_retval='pass - first run'),
                    msg='Key paths had unexpected values')

    self.RestartBrowser(clear_profile=False)

    self.NavigateToURL(url + '#part2')
    self.assertTrue(self.WaitUntil(self.GetActiveTabTitle,
                                    expect_retval='pass - second run'),
                     msg='Key paths had unexpected values')

  def testVersionChangeCrashResilience(self):
    """Verify that a VERSION_CHANGE transaction is rolled back
    after a renderer/browser crash"""

    url = self.GetHttpURLForDataPath('indexeddb', 'version_change_crash.html')

    self.NavigateToURL(url + '#part1')
    self.assertTrue(self.WaitUntil(self.GetActiveTabTitle,
                                   expect_retval='pass - part1 - complete'),
                    msg='Failed to prepare database')

    self.RestartBrowser(clear_profile=False)

    self.NavigateToURL(url + '#part2')
    self.assertTrue(self.WaitUntil(self.GetActiveTabTitle,
                                   expect_retval='pass - part2 - crash me'),
                    msg='Failed to start transaction')


    test_utils.CrashBrowser(self)

    self.RestartBrowser(clear_profile=False)

    self.NavigateToURL(url + '#part3')
    self.assertTrue(self.WaitUntil(self.GetActiveTabTitle,
                                   expect_retval='pass - part3 - rolled back'),
                    msg='VERSION_CHANGE not completely aborted')

  def testConnectionsClosedOnTabClose(self):
    """Verify that open DB connections are closed when a tab is destroyed."""

    url = self.GetHttpURLForDataPath('indexeddb', 'version_change_blocked.html')

    self.NavigateToURL(url + '#tab1')
    pid = self.GetBrowserInfo()['windows'][0]['tabs'][0]['renderer_pid']
    self.assertTrue(self.WaitUntil(self.GetActiveTabTitle,
                                   expect_retval='setVersion(1) complete'),
                    msg='Version change failed')

    # Start to a different URL to force a new renderer process
    self.AppendTab(pyauto.GURL('about:blank'))
    self.NavigateToURL(url + '#tab2')
    self.assertTrue(self.WaitUntil(self.GetActiveTabTitle,
                                   expect_retval='setVersion(2) blocked'),
                    msg='Version change not blocked as expected')
    self.KillRendererProcess(pid)
    self.assertTrue(self.WaitUntil(self.GetActiveTabTitle,
                                    expect_retval='setVersion(2) complete'),
                     msg='Version change never unblocked')


if __name__ == '__main__':
  pyauto_functional.Main()
