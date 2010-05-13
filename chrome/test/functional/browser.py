#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re

import pyauto_functional  # Must be imported before pyauto
import pyauto


class BrowserTest(pyauto.PyUITest):
  """TestCase for Browser info."""

  def Debug(self):
    """Test method for experimentation.

    This method will not run automatically.
    """
    import pprint
    pp = pprint.PrettyPrinter(indent=2)
    while True:
      raw_input('Hit <enter> to dump info.. ')
      properties = self.GetBrowserInfo()
      self.assertTrue(properties)
      pp.pprint(properties)

  def testGotVersion(self):
    """Verify there's a valid version string."""
    version_string = self.GetBrowserInfo()['ChromeVersion']
    self.assertTrue(re.match("\d+\.\d+\.\d+.\.\d+", version_string))


if __name__ == '__main__':
  pyauto_functional.Main()

