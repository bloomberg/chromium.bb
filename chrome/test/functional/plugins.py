#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pyauto_functional  # Must be imported before pyauto
import pyauto


class PluginsTest(pyauto.PyUITest):
  """TestCase for Plugins."""

  def Debug(self):
    """Test method for experimentation.

    This method will not run automatically.
    """
    import pprint
    pp = pprint.PrettyPrinter(indent=2)
    while True:
      raw_input('Interact with the browser and hit <enter> to list plugins... ')
      pp.pprint(self.GetPluginsInfo().Plugins())

  def testHasFlash(self):
    """Verify that Flash plugin loads and is enabled."""
    flash = self.GetPluginsInfo().FirstPluginForName('Shockwave Flash')
    self.assertTrue(flash)
    self.assertTrue(flash['enabled'])

  def testEnableDisableFlash(self):
    """Verify that flash plugin can be enabled/disabled."""
    flash = self.GetPluginsInfo().FirstPluginForName('Shockwave Flash')
    self.assertTrue(flash['enabled'])
    self.DisablePlugin(flash['path'])
    flash = self.GetPluginsInfo().FirstPluginForName('Shockwave Flash')
    self.assertFalse(flash['enabled'])
    self.EnablePlugin(flash['path'])
    flash = self.GetPluginsInfo().FirstPluginForName('Shockwave Flash')
    self.assertTrue(flash['enabled'])


if __name__ == '__main__':
  pyauto_functional.Main()

