#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

import pyauto_functional  # must be imported before pyauto
import pyauto
import test_utils


class CroshTest(pyauto.PyUITest):
  """Tests for crosh."""

  def setUp(self):
    """Close all windows at startup."""
    pyauto.PyUITest.setUp(self)
    for _ in range(self.GetBrowserWindowCount()):
      self.CloseBrowserWindow(0)

  def testBasic(self):
    """Verify crosh basic flow."""
    test_utils.OpenCroshVerification(self)

    # Verify crosh prompt.
    self.WaitForHtermText(text='crosh> ',
        msg='Could not find "crosh> " prompt')
    self.assertTrue(
        self.GetHtermRowsText(start=0, end=2).endswith('crosh> '),
        msg='Could not find "crosh> " prompt')

    # Run a crosh command.
    self.SendKeysToHterm('help\\n')
    self.WaitForHtermText(text='help_advanced',
        msg='Could not find "help_advanced" in help output.')

    # Exit crosh and close tab.
    self.SendKeysToHterm('exit\\n')
    self.WaitForHtermText(text='command crosh completed with exit code 0',
        msg='Could not exit crosh.')

  def testAddBookmark(self):
    """Test crosh URL can be bookmarked"""
    test_utils.OpenCroshVerification(self)

    # Add bookmark.
    bookmarks = self.GetBookmarkModel()
    bar_id = bookmarks.BookmarkBar()['id']
    name = 'crosh'
    url = self.GetActiveTabURL()
    count = bookmarks.NodeCount()
    self.AddBookmarkURL(bar_id, 0, name, url.spec())
    bookmarks = self.GetBookmarkModel()
    node = bookmarks.BookmarkBar()['children'][0]
    self.assertEqual(count + 1, bookmarks.NodeCount())
    self.assertEqual(node['type'], 'url')
    self.assertEqual(node['name'], name)
    self.assertEqual(url.spec(), node['url'])

  def testMultipleWindowCrosh(self):
    """Test that crosh can be opened in multiple windows."""
    test_utils.OpenCroshVerification(self)

    for windex in range (1, 4):  # 3 new windows
      self.OpenNewBrowserWindow(True)
      self.OpenCrosh()
      self.assertEqual('crosh', self.GetActiveTabTitle())

      # Verify crosh prompt.
      self.WaitForHtermText(text='crosh> ', tab_index=1, windex=windex,
          msg='Could not find "crosh> " prompt')
      self.assertTrue(
        self.GetHtermRowsText(start=0, end=2, tab_index=1,
                              windex=windex).endswith('crosh> '),
        msg='Could not find "crosh> " prompt')

      # Exit crosh.
      self.SendKeysToHterm('exit\\n', tab_index=1, windex=windex)
      self.WaitForHtermText(text='command crosh completed with exit code 0',
          tab_index=1, windex=windex,
          msg='Could not exit crosh.')

  def testShell(self):
    """Test shell can be opened in crosh."""
    test_utils.OpenCroshVerification(self)

    # Verify crosh prompt.
    self.WaitForHtermText(text='crosh> ',
        msg='Could not find "crosh> " prompt')
    self.assertTrue(
        self.GetHtermRowsText(start=0, end=2).endswith('crosh> '),
        msg='Could not find "crosh> " prompt')

    # Run a shell command.
    self.SendKeysToHterm(r'shell\n')
    self.WaitForHtermText(text='chronos@localhost',
        msg='Could not find "chronos@localhost" in shell output.')

  def testConnectToAnotherhost(self):
    """Test ssh to another host."""
    test_utils.OpenCroshVerification(self)

    # Verify crosh prompt.
    self.WaitForHtermText(text='crosh> ',
        msg='Could not find "crosh> " prompt')
    self.assertTrue(
        self.GetHtermRowsText(start=0, end=2).endswith('crosh> '),
        msg='Could not find "crosh> " prompt')

    # Ssh to another host: chronos@localhost.
    self.SendKeysToHterm(r'ssh chronos@localhost\n')
    self.WaitForHtermText(text='Password',
        msg='Could not find "Password" in shell output.')
    self.SendKeysToHterm(r'test0000\n')
    self.WaitForHtermText(text='chronos@localhost',
        msg='Could not find "chronos@localhost" in shell output.')

  def testTabSwitching(self):
    """Test tab can be switched in crosh."""
    test_utils.OpenCroshVerification(self)

    # Open 6 tabs
    for x in xrange(3):
      self.AppendTab(self.GetHttpURLForDataPath('title2.html'))
      self.assertEqual('Title Of Awesomeness', self.GetActiveTabTitle(),
                       msg='Unable to navigate to title2.html and '
                           'verify tab title.')
      self.OpenCrosh()
    self.assertEqual(7, len(self.GetBrowserInfo()['windows'][0]['tabs']))

    # Select tab 5
    self.ApplyAccelerator(pyauto.IDC_SELECT_TAB_4)
    self.assertEqual('crosh', self.GetActiveTabTitle(),
                     msg='Unable to naviage to crosh.')

    # Run a crosh command.
    self.SendKeysToHterm('help\\n', tab_index=4, windex=0)
    self.WaitForHtermText(text='help_advanced', tab_index=4, windex=0,
        msg='Could not find "help_advanced" in help output.')

  def testLargefileCrosh(self):
    """Test large file is displayed in crosh."""
    test_utils.OpenCroshVerification(self)

    # Verify crosh prompt.
    self.WaitForHtermText(text='crosh> ',
        msg='Could not find "crosh> " prompt')
    self.assertTrue(
        self.GetHtermRowsText(start=0, end=2).endswith('crosh> '),
        msg='Could not find "crosh> " prompt')

    # Login to localhost.
    self.SendKeysToHterm(r'ssh chronos@localhost\n')
    self.WaitForHtermText(text='Password',
        msg='Could not find "Password" in shell output.')
    self.SendKeysToHterm(r'test0000\n')
    self.WaitForHtermText(text='chronos@localhost',
        msg='Could not find "chronos@localhost" in shell output.')

    # Create a file with 140 characters per line, 50000 lines.
    bigfn = '/tmp/bigfile.txt'
    with open(bigfn, 'w') as file:
        file.write(('0' * 140 + '\n') * 50000 + 'complete\n')

    # Cat a large file.
    self.SendKeysToHterm(r'cat %s\n' % bigfn)
    self.WaitForHtermText(text='complete',
        msg='Could not find "complete" in shell output.')
    os.remove(bigfn)


if __name__ == '__main__':
  pyauto_functional.Main()
