#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import pyauto_functional
import pyauto


class ChromeosBasic(pyauto.PyUITest):
  """Basic tests for ChromeOS.

  Requires ChromeOS to be logged in.
  """

  def testAppendTabs(self):
    """Basic test for primary chrome on ChromeOS (named testing interface)."""
    self.AppendTab(pyauto.GURL('about:version'))
    self.assertEqual(self.GetTabCount(), 2, msg='Expected 2 tabs')

  def testRestart(self):
    """Basic test which involves restarting chrome on ChromeOS."""
    file_url = self.GetFileURLForDataPath('title2.html')
    self.NavigateToURL(file_url)
    self.assertEqual(1, len(self.GetHistoryInfo().History()))
    self.RestartBrowser(clear_profile=False)
    self.assertEqual(1, len(self.GetHistoryInfo().History()))

  def testNetworkInfo(self):
    """Get basic info on networks."""
    result = self.GetNetworkInfo()
    self.assertTrue(result)
    logging.debug(result)


if __name__ == '__main__':
  pyauto_functional.Main()
