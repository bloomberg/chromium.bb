#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pyauto_functional
import pyauto


class IndexedDBTest(pyauto.PyUITest):
  """Test of IndexedDB."""

  def _CrashBrowser(self):
    """Crashes the browser by navigating to special URL"""
    crash_url = 'about:inducebrowsercrashforrealz'
    self.NavigateToURL(crash_url)

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

    self._CrashBrowser()

    self.RestartBrowser(clear_profile=False)

    self.NavigateToURL(url + '#part3')
    self.assertTrue(self.WaitUntil(self.GetActiveTabTitle,
                                   expect_retval='pass - part3 - rolled back'),
                    msg='VERSION_CHANGE not completely aborted')

if __name__ == '__main__':
  pyauto_functional.Main()
