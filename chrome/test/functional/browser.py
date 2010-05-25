#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
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
      info = self.GetBrowserInfo()
      pp.pprint(info)

  def testGotVersion(self):
    """Verify there's a valid version string."""
    version_string = self.GetBrowserInfo()['properties']['ChromeVersion']
    self.assertTrue(re.match('\d+\.\d+\.\d+.\.\d+', version_string))

  def testWindowResize(self):
    """Verify window resizing and persistence after restart."""
    def _VerifySize(x, y, width, height):
      info = self.GetBrowserInfo()
      self.assertEqual(x, info['windows'][0]['x'])
      self.assertEqual(y, info['windows'][0]['y'])
      self.assertEqual(width, info['windows'][0]['width'])
      self.assertEqual(height, info['windows'][0]['height'])
    self.SetWindowDimensions(x=20, y=40, width=600, height=300)
    _VerifySize(20, 40, 600, 300)
    self.RestartBrowser(clear_profile=False)
    _VerifySize(20, 40, 600, 300)

  def testCanLoadFlash(self):
    """Verify that we can play Flash.

    We merely check that the flash process kicks in.
    """
    flash_url = self.GetFileURLForPath(os.path.join(self.DataDir(),
                                                    'plugin', 'flash.swf'))
    self.NavigateToURL(flash_url)
    child_processes = self.GetBrowserInfo()['child_processes']
    self.assertTrue([x for x in child_processes
        if x['type'] == 'Plug-in' and x['name'] == 'Shockwave Flash'])


if __name__ == '__main__':
  pyauto_functional.Main()

