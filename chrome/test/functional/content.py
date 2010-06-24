#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

import pyauto_functional  # Must be imported before pyauto
import pyauto


class ContentTest(pyauto.PyUITest):
  """TestCase for getting html content from the browser."""

  def _DataDirURL(self, filename):
    """Return a URL in the data dir for the given filename."""
    return self.GetFileURLForPath(os.path.join(self.DataDir(), filename))

  def _StringContentCheck(self, content_string, have_list, nothave_list):
    """Look for the presence or absence of strings in content.

       Confirm all strings in have_list are found in content_string.
       Confirm all strings in nothave_list are NOT found in content_string.
    """
    for s in have_list:
      self.assertTrue(s in content_string)
    for s in nothave_list:
      self.assertTrue(s not in content_string)

  def _FileContentCheck(self, filename, have_list, nothave_list):
    """String check in local file.

       For each local filename, tell the browser to load it as a file
       UEL from the DataDir.  Ask the browser for the loaded html.
       Confirm all strings in have_list are found in it.  Confirm all
       strings in nothave_list are NOT found in it.  Assumes only one
       window/tab is open.
    """
    self.NavigateToURL(self._DataDirURL(filename))
    self._StringContentCheck(self.GetTabContents(), have_list, nothave_list)

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
    self._StringContentCheck(self.GetTabContents(0, 0),
                             ['page has no title'],
                             ['Awesomeness'])
    self._StringContentCheck(self.GetTabContents(1, 0),
                             ['Awesomeness'],
                             ['page has no title'])

  def testThreeWindows(self):
    """Test content when we have 3 windows."""
    self.NavigateToURL(self._DataDirURL('title1.html'))
    for (window_index, url) in ((1, 'title2.html'), (2, 'title3.html')):
      self.OpenNewBrowserWindow(True)
      self.GetBrowserWindow(window_index).BringToFront()
      self.NavigateToURL(self._DataDirURL(url), window_index, 0)

    self._StringContentCheck(self.GetTabContents(0, 0),
                             ['page has no title'],
                             ['Awesomeness'])
    self._StringContentCheck(self.GetTabContents(0, 1),
                             ['Awesomeness'],
                             ['page has no title'])
    self._StringContentCheck(self.GetTabContents(0, 2),
                             ['Title Of More Awesomeness'],
                             ['page has no title'])

  def testAboutVersion(self):
    """Confirm about:version contains some expected content."""
    self.NavigateToURL('about:version')
    self._StringContentCheck(self.GetTabContents(),
                             ['User Agent', 'Command Line'],
                             ['odmomfodfm disfnodugdzuoufgbn ifdnf fif'])


if __name__ == '__main__':
  pyauto_functional.Main()
