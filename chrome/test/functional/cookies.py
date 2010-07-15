#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

import pyauto_functional  # Must be imported before pyauto
import pyauto


class CookiesTest(pyauto.PyUITest):
  """Tests for Cookies."""

  def testCookies(self):
    """Test setting cookies and getting the value."""
    cookie_url = pyauto.GURL(self.GetFileURLForPath(
        os.path.join(self.DataDir(), 'title1.html')))
    cookie_val = 'foo=bar'
    self.SetCookie(cookie_url, cookie_val)
    self.assertEqual(cookie_val, self.GetCookie(cookie_url))


if __name__ == '__main__':
  pyauto_functional.Main()
