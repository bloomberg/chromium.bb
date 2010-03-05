#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import pyauto

class BookmarkBarTest(pyauto.PyUITest):
  """Test of bookmark bar toggling, visibility, and animation."""

  def testBookmarkBarVisible(self):
    """Open and close the bookmark bar, confirming visibility at each step."""
    self.assertFalse(self.GetBookmarkBarVisibility())
    self.ApplyAccelerator(pyauto.IDC_SHOW_BOOKMARK_BAR)
    self.assertTrue(self.GetBookmarkBarVisibility())
    self.ApplyAccelerator(pyauto.IDC_SHOW_BOOKMARK_BAR)
    self.assertFalse(self.GetBookmarkBarVisibility())


if __name__ == '__main__':
  unittest.main()
