#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pyauto_functional
import pyauto
import test_utils


class IndexedDBTest(pyauto.PyUITest):
  """Test of IndexedDB."""

  _SESSION_STARTUP_NTP = 5

  def _GetInnerText(self, selector, tab_index=0):
    """Return the value of the innerText property of the target node.
    The target node is identified by CSS selector, e.g. #id"""

    expression = 'document.querySelector("' + selector + '").innerText'
    return self.GetDOMValue(expression, tab_index=tab_index)

  def _WaitForAndAssertResult(self, expected, tab_index=0):
    """Wait for the element with id="result" to exist, and verify the value."""
    self.WaitForDomNode('id("result")', tab_index=tab_index)
    self.assertEqual(self._GetInnerText('#result', tab_index=tab_index),
                     expected)

  def _ClearResult(self, tab_index=0):
    """Delete the element with id="result" if it exists."""
    expression = """(function() {
                      var e = document.querySelector('#result');
                      if (e)
                        e.parentNode.removeChild(e);
                      return 'ok';
                    }())"""
    self.assertEqual(self.GetDOMValue(expression, tab_index=tab_index), 'ok')

  def _AssertNewTabPage(self):
    """Assert that the current tab is the new tab page, not a restored tab."""
    self.assertEqual(self.GetBrowserInfo()['windows'][0]['tabs'][0]['url'],
                     'chrome://newtab/')

  def testIndexedDBNullKeyPathPersistence(self):
    """Verify null key path persists after restarting browser."""

    # Don't restore tabs after restart
    self.SetPrefs(pyauto.kRestoreOnStartup, self._SESSION_STARTUP_NTP)

    url = self.GetHttpURLForDataPath('indexeddb', 'bug_90635.html')

    self.NavigateToURL(url + '#part1')
    self._WaitForAndAssertResult('pass - first run')

    self.RestartBrowser(clear_profile=False)
    self._AssertNewTabPage()

    self.NavigateToURL(url + '#part2')
    self._WaitForAndAssertResult('pass - second run')

  def testVersionChangeCrashResilience(self):
    """Verify that a VERSION_CHANGE transaction is rolled back
    after a renderer/browser crash"""

    # Don't restore tabs after restart
    self.SetPrefs(pyauto.kRestoreOnStartup, self._SESSION_STARTUP_NTP)

    url = self.GetHttpURLForDataPath('indexeddb', 'version_change_crash.html')

    self.NavigateToURL(url + '#part1')
    self._WaitForAndAssertResult('pass - part1 - complete')

    self.RestartBrowser(clear_profile=False)
    self._AssertNewTabPage()

    self.NavigateToURL(url + '#part2')
    self._WaitForAndAssertResult('pass - part2 - crash me')

    test_utils.CrashBrowser(self)

    self.RestartBrowser(clear_profile=False)
    self._AssertNewTabPage()

    self.NavigateToURL(url + '#part3')
    self._WaitForAndAssertResult('pass - part3 - rolled back')

  def testConnectionsClosedOnTabClose(self):
    """Verify that open DB connections are closed when a tab is destroyed."""

    url = self.GetHttpURLForDataPath('indexeddb', 'version_change_blocked.html')

    self.NavigateToURL(url + '#tab1')
    pid = self.GetBrowserInfo()['windows'][0]['tabs'][0]['renderer_pid']
    self._WaitForAndAssertResult('setVersion(1) complete')

    # Start on a different URL to force a new renderer process.
    self.AppendTab(pyauto.GURL('about:blank'))
    self.NavigateToURL(url + '#tab2')
    self._WaitForAndAssertResult('setVersion(2) blocked', tab_index=1)
    self._ClearResult(tab_index=1)

    self.KillRendererProcess(pid)
    self.assertEqual(self.GetTabCount(), 2)
    self.GetBrowserWindow(0).GetTab(0).Close(True)

    self._WaitForAndAssertResult('setVersion(2) complete')


if __name__ == '__main__':
  pyauto_functional.Main()
