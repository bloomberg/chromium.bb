#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import sys

import pyauto_functional  # Must be imported before pyauto
import pyauto


class PluginsCheck(pyauto.PyUITest):
  """TestCase for Plugins."""

  def _ReadPluginsList(self, plugins_list_file):
    """Read test plugins list from a file based on the test platform

    File contains list of plugins like,
    [
    {u'name':'Shockwave Flash', u'enabled':True, u'version':u'10.2.154.9'}
    {u'name':'Chrome PDF Viewer', u'enabled':True, u'version':u''}
    ...
    ]
    """
    file_path = os.path.join(self.DataDir(), plugins_list_file)
    return self.EvalDataFrom(file_path)

  def _RemoveNonApplicablePlugins(self, plugins_list):
    """Removes plugins that do not apply to the given ChromeOS board.
    Note: This function is only run on ChromeOS.
    """
    new_list = []
    for plugin in plugins_list:
      if self.ChromeOSBoard() in set(plugin['boards']):
        plugin.pop('boards')
        new_list.append(plugin)
    return new_list

  def testPluginsStates(self):
    """Verify plugins' versions and states."""
    if self.GetBrowserInfo()['properties']['branding'] != 'Google Chrome':
      return
    if self.IsWin():
      plugins_list = self._ReadPluginsList('win_plugins_list.txt')
    elif self.IsMac():
      plugins_list = self._ReadPluginsList('mac_plugins_list.txt')
    elif self.IsChromeOS():
      plugins_list = self._ReadPluginsList('chromeos_plugins_list.txt')
      plugins_list = self._RemoveNonApplicablePlugins(plugins_list)
    elif self.IsLinux():
      # TODO(rohitbm)
      # Add plugins_check support for Linux
      logging.debug('Not performing plugins check on linux')
      return

    browser_plugins_list = self.GetPluginsInfo().Plugins()
    for plugin in plugins_list:
      # We will compare the keys available in the plugin list
      found_plugin = False
      for browser_plugin in browser_plugins_list:
        whitelist_keys = plugin.keys()
        if 'unique_key' in plugin:
          unique_key = plugin['unique_key']
          whitelist_keys.remove('unique_key')
        else:
          unique_key = 'name'
        if browser_plugin[unique_key] == plugin[unique_key]:
          found_plugin = True
          for key in whitelist_keys:
            if browser_plugin[key] != plugin[key]:
              self.assertEqual(browser_plugin[key], plugin[key], 'The following'
                  ' plugin attributes do not match the whitelist:'
                  '\n\tplugin:\n%s\n\tlist:\n%s'
                  % (self.pformat(browser_plugin), self.pformat(plugin)))
          break;
      self.assertTrue(found_plugin, 'The following plugin in the whitelist '
          'does not match any on the system:\n%s' % self.pformat(plugin))
    if self.IsChromeOS():
      self.assertEqual(len(plugins_list), len(browser_plugins_list),
          'The number of plugins on the system do not match the number in the '
          'whitelist.\n\tSystem list: %s\n\tWhitelist: %s' %
          (self.pformat(browser_plugins_list), self.pformat(plugins_list)))


if __name__ == '__main__':
  pyauto_functional.Main()
