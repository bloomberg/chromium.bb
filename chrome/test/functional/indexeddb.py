#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pyauto_functional
import pyauto


class IndexedDBTest(pyauto.PyUITest):
  """Test of IndexedDB."""

  def _GetTestResult(self):
    """Returns the result of an asynchronous test"""
    js = """
        window.domAutomationController.send(window.testResult);
    """
    return self.ExecuteJavascript(js)

  def _WaitForTestResult(self):
    """Waits until a non-empty asynchronous test result is recorded"""
    self.assertTrue(self.WaitUntil(lambda: self._GetTestResult() != '',
                                   timeout=120),
                    msg='Test did not finish')

  def testIndexedDBNullKeyPathPersistence(self):
    """Verify null key path persists after restarting browser."""

    url = self.GetFileURLForDataPath('indexeddb', 'bug_90635.html')

    self.NavigateToURL(url)
    self._WaitForTestResult()
    self.assertEqual(self._GetTestResult(),
                     'pass - first run',
                     msg='Key paths had unexpected values')

    self.RestartBrowser(clear_profile=False)

    self.NavigateToURL(url)
    self._WaitForTestResult()
    self.assertEqual(self._GetTestResult(),
                     'pass - second run',
                     msg='Key paths had unexpected values')

if __name__ == '__main__':
  pyauto_functional.Main()
