#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

import pyauto_functional  # Must be imported before pyauto
import pyauto
import test_utils


class ContentTest(pyauto.PyUITest):
  """TestCase for getting html content from the browser."""

  def _DataDirURL(self, filename):
    """Return a URL in the data dir for the given filename."""
    return self.GetFileURLForPath(os.path.join(self.DataDir(), filename))

  def _FileContentCheck(self, filename, have_list, nothave_list):
    """String check in local file.

       For each local filename, tell the browser to load it as a file
       URL from the DataDir.  Ask the browser for the loaded html.
       Confirm all strings in have_list are found in it.  Confirm all
       strings in nothave_list are NOT found in it.  Assumes only one
       window/tab is open.
    """
    self.NavigateToURL(self._DataDirURL(filename))
    test_utils.StringContentCheck(self, self.GetTabContents(),
                                  have_list, nothave_list)

  def testLocalFileBasics(self):
    """For a few local files, do some basic has / not has."""
    self._FileContentCheck('title1.html',
                           ['<html>', '</html>', 'page has no title'],
                           ['Title Of Awesomeness', '<b>'])
    self._FileContentCheck('title2.html',
                           ['<html>', '</html>', 'Title Of Awesomeness'],
                           ['plastic flower', '<b>'])
    self._FileContentCheck('title3.html',
                           ['<html>', '</html>', 'Title Of More Awesomeness'],
                           ['dinfidnfid', 'Title Of Awesomeness', '<b>'])

  def testTwoTabs(self):
    """Test content when we have 2 tabs."""
    self.NavigateToURL(self._DataDirURL('title1.html'))
    self.AppendTab(pyauto.GURL(self._DataDirURL('title2.html')), 0)
    test_utils.StringContentCheck(self, self.GetTabContents(0, 0),
                                  ['page has no title'],
                                  ['Awesomeness'])
    test_utils.StringContentCheck(self, self.GetTabContents(1, 0),
                                  ['Awesomeness'],
                                  ['page has no title'])

  def testThreeWindows(self):
    """Test content when we have 3 windows."""
    self.NavigateToURL(self._DataDirURL('title1.html'))
    for (window_index, url) in ((1, 'title2.html'), (2, 'title3.html')):
      self.OpenNewBrowserWindow(True)
      self.GetBrowserWindow(window_index).BringToFront()
      self.NavigateToURL(self._DataDirURL(url), window_index, 0)

    test_utils.StringContentCheck(self, self.GetTabContents(0, 0),
                                  ['page has no title'],
                                  ['Awesomeness'])
    test_utils.StringContentCheck(self, self.GetTabContents(0, 1),
                                  ['Title Of Awesomeness'],
                                  ['page has no title'])
    test_utils.StringContentCheck(self, self.GetTabContents(0, 2),
                                  ['Title Of More Awesomeness'],
                                  ['page has no title'])

  def testAboutVersion(self):
    """Confirm about:version contains some expected content."""
    self.NavigateToURL('about:version')
    test_utils.StringContentCheck(self, self.GetTabContents(),
                                  ['WebKit', 'os_version', 'js_version'],
                                  ['odmomfodfm disfnodugdzuoufgbn ifdnf fif'])

  def testHttpsPage(self):
    """Test content in https://www.google.com"""
    url = 'https://www.google.com'
    self.NavigateToURL(url)
    html_regular = self.GetTabContents()
    self.assertTrue('Google Search' in html_regular)
    self.assertTrue('Feeling Lucky' in html_regular)
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
    self.NavigateToURL(url, 1, 0)
    html_incognito = self.GetTabContents(0, 1)
    self.assertTrue('Google Search' in html_incognito)
    self.assertTrue('Feeling Lucky' in html_incognito)


if __name__ == '__main__':
  pyauto_functional.Main()
