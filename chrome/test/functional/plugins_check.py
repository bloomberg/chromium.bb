#!/usr/bin/python
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

  def _ReadPluginsList(self):
    """Read test plugins list from a file based on the test platform

    File contains list of plugins like,
    [
    {u'name':'Shockwave Flash', u'enabled':True, u'version':u'10.2.154.9'}
    {u'name':'Chrome PDF Viewer', u'enabled':True, u'version':u''}
    ...
    ]
    """
    if self.IsMac():
      file_path = os.path.join(self.DataDir(), 'mac_plugins_list.txt')
      return self.EvalDataFrom(file_path)

  def testPluginsStates(self):
    """Verify plugins' versions and states."""
    if not self.IsMac():
      # TODO(rohitbm)
      # Need to find plugins list on Windows/Linux machines.
      return
    default_plugins_list = self._ReadPluginsList();
    for plugin in self.GetPluginsInfo().Plugins():
      test_plugin = [x['name'] for x in default_plugins_list \
          if x['version'] == plugin['version'] and \
             x['enabled'] == str(plugin['enabled']) and \
             x['name'] == plugin['name']]
      plugin_info = '[ NAME : %s, VERSION: %s, ENABLED: %s]' % \
          (plugin['name'], plugin['version'], str(plugin['enabled']))
      logging.debug(plugin_info)
      self.assertTrue(test_plugin, '%s - Failed to match with plugins'
          ' available in the default list' % plugin_info)


if __name__ == '__main__':
  pyauto_functional.Main()
