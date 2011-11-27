#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

import pyauto_functional  # Must be imported before pyauto
import pyauto

class HTTPSTest(pyauto.PyUITest):
  """TestCase for SSL."""

  def Debug(self):
    """Test method for experimentation.

    This method will not run automatically.
    Use: python chrome/test/functional/ssl.py ssl.SSLTest.Debug
    """
    while True:
      raw_input('Hit <enter> to dump info.. ')
      self.pprint(self.GetNavigationInfo())

  def testSSLPageBasic(self):
    """Verify the navigation state in an https page."""
    self.NavigateToURL('https://www.google.com')
    self.assertTrue(self.WaitUntil(
        lambda: self.GetNavigationInfo()['ssl']['security_style'],
                expect_retval='SECURITY_STYLE_AUTHENTICATED'))
    ssl = self.GetNavigationInfo()['ssl']
    self.assertFalse(ssl['displayed_insecure_content'])
    self.assertFalse(ssl['ran_insecure_content'])


if __name__ == '__main__':
  pyauto_functional.Main()
