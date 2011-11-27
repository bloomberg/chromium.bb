#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import time
import unittest

import pyauto_functional
import pyauto

class BookmarkBarTest(pyauto.PyUITest):
  """Test of bookmark bar toggling, visibility, and animation."""

  def testBookmarkBarVisible(self):
    """Open and close the bookmark bar, confirming visibility at each step."""
    self.assertFalse(self.GetBookmarkBarVisibility())
    self.ApplyAccelerator(pyauto.IDC_SHOW_BOOKMARK_BAR)
    self.assertTrue(self.WaitUntil(self.GetBookmarkBarVisibility))
    self.ApplyAccelerator(pyauto.IDC_SHOW_BOOKMARK_BAR)
    self.assertTrue(self.WaitUntil(lambda:
                                   self.GetBookmarkBarVisibility() == False))

  def _timeAndWaitForBookmarkBarVisibilityChange(self, wait_for_open):
    """Wait for a bookmark bar visibility change and print the wait time.

    We cannot use timeit since we need to reference self.
    """
    start = time.time()
    self.assertTrue(self.WaitForBookmarkBarVisibilityChange(wait_for_open))
    end = time.time()
    print 'Wait for bookmark bar animation complete: %2.2fsec' % (end - start)

  def testBookmarkBarVisibleWait(self):
    """Test waiting for the animation to finish."""
    self.assertFalse(self.GetBookmarkBarVisibility())
    self.ApplyAccelerator(pyauto.IDC_SHOW_BOOKMARK_BAR)
    self._timeAndWaitForBookmarkBarVisibilityChange(True);
    self.assertTrue(self.GetBookmarkBarVisibility())
    self.ApplyAccelerator(pyauto.IDC_SHOW_BOOKMARK_BAR)
    self._timeAndWaitForBookmarkBarVisibilityChange(False);
    self.assertFalse(self.GetBookmarkBarVisibility())

if __name__ == '__main__':
  pyauto_functional.Main()
