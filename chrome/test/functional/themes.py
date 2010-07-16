#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

import pyauto_functional  # Must be imported before pyauto
import pyauto


class ThemesTest(pyauto.PyUITest):
  """TestCase for Themes."""

  def Debug(self):
    """Test method for experimentation.

    This method will not run automatically.
    """
    import pprint
    pp = pprint.PrettyPrinter(indent=2)
    while True:
      raw_input('Hit <enter> to dump info.. ')
      pp.pprint(self.GetThemeInfo())

  def testSetTheme(self):
    """Verify theme install."""
    self.assertFalse(self.GetThemeInfo())  # Verify there's no theme at startup
    crx_file = os.path.abspath(
        os.path.join(self.DataDir(), 'extensions', 'theme.crx'))
    self.assertTrue(self.SetTheme(pyauto.FilePath(crx_file)))
    theme = self.GetThemeInfo()
    self.assertEqual('camo theme', theme['name'])
    # TODO: verify "theme installed" infobar

  def testThemeReset(self):
    """Verify theme reset."""
    crx_file = os.path.abspath(
        os.path.join(self.DataDir(), 'extensions', 'theme.crx'))
    self.assertTrue(self.SetTheme(pyauto.FilePath(crx_file)))
    self.assertTrue(self.ResetToDefaultTheme())
    self.assertFalse(self.GetThemeInfo())


if __name__ == '__main__':
  pyauto_functional.Main()

