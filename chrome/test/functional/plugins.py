#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import re

import pyauto_functional  # Must be imported before pyauto
import pyauto
import test_utils


class PluginsTest(pyauto.PyUITest):
  """TestCase for Plugins."""

  def Debug(self):
    """Test method for experimentation.

    This method will not run automatically.
    """
    import pprint
    pp = pprint.PrettyPrinter(indent=2)
    while True:
      raw_input('Interact with the browser and hit <enter> to list plugins...')
      pp.pprint(self.GetPluginsInfo().Plugins())

  def _ObtainPluginsList(self):
    """Obtain a list of plugins for each platform.

    Produces warnings for plugins which are not installed on the machine.

    Returns:
      a list of 2-tuple, corresponding to the html file used for test and the
      name of the plugin
    """
    plugins = [('flash-clicktoplay.html', 'Shockwave Flash'),
               ('java_new.html', 'Java'),]    # common to all platforms
    if self.IsWin() or self.IsMac():
      plugins = plugins + [
         ('silverlight_new.html', 'Silverlight'),
         ('quicktime.html', 'Quicktime'),
         ('wmp_new.html', 'Windows Media'),
         ('real.html', 'RealPlayer'),
      ]

    out = []
    # Emit warnings for plugins that are not installed on the machine and
    # therefore cannot be tested.
    plugins_info = self.GetPluginsInfo()
    for fname, name in plugins:
      for a_plugin in plugins_info.Plugins():
        is_installed = False
        if re.search(name, a_plugin['name']):
          is_installed = True
          break
      if not is_installed:
        logging.warn('%s plugin is not installed and cannot be tested' % name)
      else:
        out.append((fname, name))
    return out

  def _GetPluginPID(self, plugin_name):
    """Fetch the pid of the plugin process with name |plugin_name|."""
    child_processes = self.GetBrowserInfo()['child_processes']
    for x in child_processes:
       if x['type'] == 'Plug-in' and re.search(plugin_name, x['name']):
         return x['pid']
    return None

  def _TogglePlugin(self, plugin_name):
    """Toggle a plugin's status.

    If enabled, disable it.
    If disabled, enable it.
    """
    for plugin in self.GetPluginsInfo().Plugins():
      if re.search(plugin_name, plugin['name']):
        if plugin['enabled']:
          self.DisablePlugin(plugin['path'])
        else:
          self.EnablePlugin(plugin['path'])

  def _IsEnabled(self, plugin_name):
    """Checks if plugin is enabled."""
    for plugin in self.GetPluginsInfo().Plugins():
      if re.search(plugin_name, plugin['name']):
         return plugin['enabled']

  def testKillAndReloadAllPlugins(self):
    """Verify plugin processes and check if they can reload after killing."""
    for fname, plugin_name in self._ObtainPluginsList():
      if plugin_name == 'Java':  # crbug.com/71223
        continue
      url = self.GetFileURLForPath(
          os.path.join(self.DataDir(), 'plugin', fname))
      self.NavigateToURL(url)
      pid = self._GetPluginPID(plugin_name)
      self.assertTrue(pid, 'No plugin process for %s' % plugin_name)
      self.Kill(pid)
      self.assertTrue(self.WaitUntil(
          lambda: self._GetPluginPID(plugin_name) is None),
          msg='Expected %s plugin to die after killing' % plugin_name)
      self.GetBrowserWindow(0).GetTab(0).Reload()
      self.assertTrue(self.WaitUntil(
          lambda: self._GetPluginPID(plugin_name)),
          msg='No plugin process for %s after reloading' % plugin_name)
      # Verify that it's in fact a new process.
      self.assertNotEqual(pid, self._GetPluginPID(plugin_name),
                          'Did not get new pid for %s after reloading' %
                          plugin_name)

  def testDisableEnableAllPlugins(self):
    """Verify if all the plugins can be disabled and enabled.

    This is equivalent to testing the enable/disable functionality in
    chrome://plugins
    """
    for fname, plugin_name in self._ObtainPluginsList():
      # Verify initial state
      self.assertTrue(self._IsEnabled(plugin_name),
                      '%s not enabled initially.' % plugin_name)
      # Disable
      self._TogglePlugin(plugin_name)
      self.assertFalse(self._IsEnabled(plugin_name))
      # Attempt to load a page that triggers the plugin and verify that it
      # indeed could not be loaded.
      url = self.GetFileURLForPath(
          os.path.join(self.DataDir(), 'plugin', fname))
      self.NavigateToURL(url)
      self.assertFalse([x for x in self.GetBrowserInfo()['child_processes']
                        if x['type'] == 'Plug-in' and
                        re.search(plugin_name, x['name'])])
      if plugin_name == 'Shockwave Flash':
        continue  # cannot reload file:// flash URL - crbug.com/47249
      if plugin_name == 'Java':
        continue  # crbug.com/71223
      # Enable
      self._TogglePlugin(plugin_name)
      self.GetBrowserWindow(0).GetTab(0).Reload()
      self.assertTrue([x for x in self.GetBrowserInfo()['child_processes']
                       if x['type'] == 'Plug-in' and
                       re.search(plugin_name, x['name'])])
      self.assertTrue(self._IsEnabled(plugin_name), plugin_name)

  def testBlockAllPlugins(self):
    """Verify that all the plugins can be blocked.
    Verifying by checking that flash plugin was blocked.
    """
    flash_url = self.GetFileURLForPath(os.path.join(
        self.DataDir(), 'plugin', 'flash-clicktoplay.html'))
    self.NavigateToURL(flash_url)
    flash_pid = self._GetPluginPID('Shockwave Flash')
    self.assertTrue(flash_pid, msg='No plugin process for Shockwave Flash')
    # Killing the flash process as it takes a while before the plugin
    # process is terminated even though there are no tabs using it.
    self.Kill(flash_pid)
    self.assertTrue(self.WaitUntil(
        lambda: self._GetPluginPID('Shockwave Flash') is None),
        msg='Expected Shockwave Flash plugin to die after killing')

    # Set the preference to block all plugins.
    self.SetPrefs(pyauto.kDefaultContentSettings, {'plugins': 2})

    self.GetBrowserWindow(0).GetTab(0).Reload()
    self.assertFalse(self._GetPluginPID('Shockwave Flash'),
                     msg='Plug-in not blocked.')

  def testAllowPluginException(self):
    """Verify that plugins can be allowed on a domain by adding
    an exception(s)."""
    # Set the preference to block all plugins.
    self.SetPrefs(pyauto.kDefaultContentSettings, {'plugins': 2})

    flash_url = self.GetFileURLForPath(os.path.join(
        self.DataDir(), 'plugin', 'flash-clicktoplay.html'))
    self.NavigateToURL(flash_url)
    # Check that plugins are blocked.
    self.assertFalse(self._GetPluginPID('Shockwave Flash'),
                     msg='Plug-in not blocked.')

    # Add an exception to allow plugins on hulu.com.
    self.SetPrefs(pyauto.kContentSettingsPatterns,
                 {'[*.]hulu.com': {'plugins': 1}})
    self.AppendTab(pyauto.GURL('http://www.hulu.com'))
    self.assertTrue(self._GetPluginPID('Shockwave Flash'),
                    msg='No plugin process for Shockwave Flash')

  def testBlockPluginException(self):
    """Verify that plugins can be blocked on a domain by adding
    an exception(s)."""
    url = self.GetHttpURLForDataPath(os.path.join('plugin',
                                                  'flash-clicktoplay.html'))
    self.NavigateToURL(url)
    # Wait until Shockwave Flash plugin process loads.
    self.assertTrue(self.WaitUntil(
        lambda: self._GetPluginPID('Shockwave Flash') is not None),
        msg='No plugin process for Shockwave Flash')
    self.Kill(self._GetPluginPID('Shockwave Flash'))
    self.assertTrue(self.WaitUntil(
        lambda: self._GetPluginPID('Shockwave Flash') is None),
        msg='Expected Shockwave Flash plugin to die after killing')

    # Add an exception to block plugins on localhost.
    self.SetPrefs(pyauto.kContentSettingsPatterns,
                 {'[*.]127.0.0.1': {'plugins': 2}})
    self.GetBrowserWindow(0).GetTab(0).Reload()
    self.assertFalse(self._GetPluginPID('Shockwave Flash'),
                     msg='Shockwave Flash Plug-in not blocked.')


if __name__ == '__main__':
  pyauto_functional.Main()
