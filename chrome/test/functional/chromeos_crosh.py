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


if __name__ == '__main__':
  pyauto_functional.Main()
