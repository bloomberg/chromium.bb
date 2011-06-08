#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

import pyauto_functional  # Must be imported before pyauto
import pyauto
import test_utils


class SpecialTabsTest(pyauto.PyUITest):
  """TestCase for Special Tabs like about:version, chrome://history, etc."""

  special_accelerator_tabs = {
    pyauto.IDC_SHOW_HISTORY: 'History',
    pyauto.IDC_MANAGE_EXTENSIONS: 'Extensions',
    pyauto.IDC_SHOW_DOWNLOADS: 'Downloads',
  }

  special_url_tabs = {
    'about:': 'About Version',
    'about:about': 'Chrome URLs',
    'about:appcache-internals': 'AppCache Internals',
    'about:credits': 'Credits',
    'about:dns': 'About DNS',
    'about:histograms': 'About Histograms',
    'about:plugins': 'Plug-ins',
    'about:sync': 'Sync Internals',
    'about:sync-internals': 'Sync Internals',
    'about:version': 'About Version',
    'chrome://about': 'Chrome URLs',
    'chrome://downloads': 'Downloads',
    'chrome://extensions': 'Extensions',
    'chrome://history': 'History',
    'chrome://newtab': 'New Tab',
    'chrome://sync-internals': 'Sync Internals',
  }

  def _VerifyAppCacheInternals(self):
    """Confirm about:appcache-internals contains expected content for Caches.
       Also confirms that the about page populates Application Caches."""
    # Navigate to html page to activate DNS prefetching.
    self.NavigateToURL('http://static.webvm.net/appcache-test/simple.html')
    # Wait for page to load and display sucess or fail message.
    self.WaitUntil(
        lambda: self.GetDOMValue('document.getElementById("result").innerHTML'),
                                 expect_retval='SUCCESS')
    self.GetBrowserWindow(0).GetTab(0).GoBack()
    test_utils.StringContentCheck(self, self.GetTabContents(),
                                  ['Manifest', 'Remove this AppCache'],
                                  [])

  def _VerifyAboutDNS(self):
    """Confirm about:dns contains expected content related to DNS info.
       Also confirms that prefetching DNS records propogate."""
    # Navigate to a page to activate DNS prefetching.
    self.NavigateToURL('http://www.google.com')
    self.GetBrowserWindow(0).GetTab(0).GoBack()
    test_utils.StringContentCheck(self, self.GetTabContents(),
                                  ['Host name', 'How long ago', 'Motivation'],
                                  [])

  def testSpecialURLTabs(self):
    """Test special tabs created by URLs like chrome://downloads,
       chrome://extensions, chrome://history, etc."""
    for url, title in self.special_url_tabs.iteritems():
      self.NavigateToURL(url)
      self.assertEqual(title, self.GetActiveTabTitle())

  def testAboutAppCacheTab(self):
    """Test App Cache tab to confirm about page populates caches."""
    self.NavigateToURL('about:appcache-internals')
    self._VerifyAppCacheInternals()
    self.assertEqual('AppCache Internals', self.GetActiveTabTitle())

  def testAboutDNSTab(self):
    """Test DNS tab to confirm DNS about page propogates records."""
    self.NavigateToURL('about:dns')
    self._VerifyAboutDNS()
    self.assertEqual('About DNS', self.GetActiveTabTitle())

  def testSpecialAcceratorTabs(self):
    """Test special tabs created by acclerators like IDC_SHOW_HISTORY,
       IDC_SHOW_DOWNLOADS."""
    for accel, title in self.special_accelerator_tabs.iteritems():
      self.RunCommand(accel)
      self.assertEqual(title, self.GetActiveTabTitle())


if __name__ == '__main__':
  pyauto_functional.Main()
