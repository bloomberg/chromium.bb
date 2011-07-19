#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
    import pprint
    pp = pprint.PrettyPrinter(indent=2)
    while True:
      raw_input('Hit <enter> to dump info.. ')
      info = self.GetNavigationInfo()
      pp.pprint(info)

  def testSSLPageBasic(self):
    """Verify the navigation state in an https page."""
    self.NavigateToURL('https://www.google.com')
    ssl = self.GetNavigationInfo()['ssl']
    security_style = ssl['security_style']
    self.assertEqual('SECURITY_STYLE_AUTHENTICATED', security_style)
    self.assertFalse(ssl['displayed_insecure_content'])
    self.assertFalse(ssl['ran_insecure_content'])


if __name__ == '__main__':
  pyauto_functional.Main()
