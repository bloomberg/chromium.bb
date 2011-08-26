#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pyauto_functional # pyauto_functional must come before pyauto.
import pyauto


class ChromeosBrowserTest(pyauto.PyUITest):

  def testCloseAllTabs(self):
    """Close all tabs and verify 1 tab is still open on Chrome OS."""
    tab_count = self.GetTabCount()

    for tab_index in xrange(tab_count - 1, -1, -1):
      self.GetBrowserWindow(0).GetTab(tab_index).Close(True)

    info = self.GetBrowserInfo()
    self.assertEqual(1, len(info['windows']))
    self.assertEqual(1, len(info['windows'][0]['tabs']))
    url = info['windows'][0]['tabs'][0]['url']
    self.assertEqual('chrome://newtab/', url,
                     msg='Unexpected URL: %s' % url)


if __name__ == '__main__':
  pyauto_functional.Main()
