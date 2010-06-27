#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pyauto_functional  # Must be imported before pyauto
import pyauto


class SpecialTabsTest(pyauto.PyUITest):
  """TestCase for Special Tabs like about:version, chrome://history, etc."""

  special_accelerator_tabs = {
    pyauto.IDC_SHOW_HISTORY: 'History',
    pyauto.IDC_MANAGE_EXTENSIONS: 'Extensions',
    pyauto.IDC_SHOW_DOWNLOADS: 'Downloads',
  }

  special_url_tabs = {
    'about:': 'About Version',
    'about:about': 'About Pages',
    'about:appcache-internals': 'AppCache Internals',
    'about:credits': 'Credits',
    'about:dns': 'About DNS',
    'about:histograms': 'About Histograms',
    'about:plugins': 'Plug-ins',
    'about:sync': 'About Sync',
    'about:version': 'About Version',
    'chrome://downloads': 'Downloads',
    'chrome://extensions': 'Extensions',
    'chrome://history': 'History',
    'chrome://newtab': 'New Tab',
  }

  def testSpecialAccleratorTabs(self):
    """Test special tabs created by acclerators like IDC_SHOW_HISTORY,
       IDC_SHOW_DOWNLOADS."""
    for accel, title in self.special_accelerator_tabs.iteritems():
      self.RunCommand(accel)
      self.assertEqual(title, self.GetActiveTabTitle())

  def testSpecialURLTabs(self):
    """Test special tabs created by URLs like chrome://downloads,
       chrome://extensions, chrome://history, etc."""
    for url, title in self.special_url_tabs.iteritems():
      self.NavigateToURL(url)
      self.assertEqual(title, self.GetActiveTabTitle())


if __name__ == '__main__':
  pyauto_functional.Main()

