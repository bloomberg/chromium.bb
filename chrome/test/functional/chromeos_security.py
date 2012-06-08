#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

import pyauto_functional
import pyauto


class ChromeosSecurity(pyauto.PyUITest):
  """Security tests for chrome on ChromeOS.

  Requires ChromeOS to be logged in.
  """

  def ExtraChromeFlags(self):
    """Override default list of extra flags typically used with automation.

    See the default flags used with automation in pyauto.py.
    Chrome flags for this test should be as close to reality as possible.
    """
    return [
      '--homepage=about:blank',
    ]

  def testCannotViewLocalFiles(self):
    """Verify that local files cannot be accessed from the browser."""
    urls_and_titles = {
       'file:///': 'Index of /',
       'file:///etc/': 'Index of /etc/',
       self.GetFileURLForDataPath('title2.html'): 'Title Of Awesomeness',
    }
    for url, title in urls_and_titles.iteritems():
      self.NavigateToURL(url)
      self.assertNotEqual(title, self.GetActiveTabTitle(),
          msg='Could access local file %s.' % url)


if __name__ == '__main__':
  pyauto_functional.Main()
