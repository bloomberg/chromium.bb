#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import types

import pyauto_functional
import pyauto

class FlashTest(pyauto.PyUITest):
  """Test Cases for Flash."""

  def setUp(self):
    pyauto.PyUITest.setUp(self)
    self._flash_plugin_type = 'Plug-in'
    if (self.IsLinux() and
        self.GetBrowserInfo()['properties']['branding'] == 'Google Chrome'):
      self._flash_plugin_type = 'Pepper Plugin'

  def _AssertFlashProcessPresent(self):
    child_processes = self.GetBrowserInfo()['child_processes']
    self.assertTrue([x for x in child_processes
        if x['type'] == self._flash_plugin_type and
        x['name'] == 'Shockwave Flash'])

  def _AssertFlashProcessNotPresent(self):
    child_processes = self.GetBrowserInfo()['child_processes']
    self.assertFalse([x for x in child_processes
        if x['type'] == self._flash_plugin_type and
        x['name'] == 'Shockwave Flash'])

  def _GetFlashProcessesInfo(self):
    """Get info about Flash processes, if any."""
    return [x for x in self.GetBrowserInfo()['child_processes']
            if x['type'] == self._flash_plugin_type and
            x['name'] == 'Shockwave Flash']

  def testCanLoadFlash(self):
    """Verify that we can play Flash.

    We merely check that the Flash process kicks in.
    """
    flash_url = self.GetFileURLForDataPath('plugin', 'flash.swf')
    self.NavigateToURL(flash_url)
    self._AssertFlashProcessPresent()

  def testSingleFlashPluginProcess(self):
    """Verify there's only one Flash plugin process shared across all uses."""
    flash_url = self.GetFileURLForDataPath('plugin', 'flash.swf')
    self.NavigateToURL(flash_url)
    for _ in range(2):
      self.AppendTab(pyauto.GURL(flash_url))
    # Open flash in new window.
    self.OpenNewBrowserWindow(True)
    self.NavigateToURL(flash_url, 1, 0)
    # Open flash in new incognito window.
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
    self.NavigateToURL(flash_url, 1, 0)
    # Verify there's only 1 flash process.
    self.assertEqual(1, len(self._GetFlashProcessesInfo()))

  def testFlashLoadsAfterKill(self):
    """Verify that Flash process reloads after crashing (or being killed)."""
    flash_url = self.GetFileURLForDataPath('plugin', 'flash.swf')
    self.NavigateToURL(flash_url)
    flash_process_id1 = self._GetFlashProcessesInfo()[0]['pid']
    self.Kill(flash_process_id1)

    def _GotFlashProcess(pid):
      flash_processes = self._GetFlashProcessesInfo()
      return len(flash_processes) == 1 and flash_processes[0]['pid'] == pid

    self.assertTrue(self.WaitUntil(
        lambda: not _GotFlashProcess(flash_process_id1)),
        msg='Flash process did not go away after killing.')
    self.ReloadActiveTab()
    flash_processes = self._GetFlashProcessesInfo()
    self.assertEqual(1, len(flash_processes))
    self.assertNotEqual(flash_process_id1, flash_processes[0]['pid'])

  def testDisableFlashPlugin(self):
    """Verify that we can disable the Flash plugin."""
    # Helper function to wait until the flash plugins get registered.
    def _GotFlashPluginInfo():
      for plugin in self.GetPluginsInfo().Plugins():
        for mime_type in plugin['mimeTypes']:
          if mime_type['mimeType'] == 'application/x-shockwave-flash':
            return True
      return False
    self.assertTrue(self.WaitUntil(_GotFlashPluginInfo))
    for plugin in self.GetPluginsInfo().Plugins():
      if re.search('Shockwave Flash', plugin['name']):
        self.assertTrue(plugin['enabled'])
        # Toggle plugin to disable flash.
        self.DisablePlugin(plugin['path'])
    flash_url = self.GetFileURLForDataPath('plugin', 'flash.html')
    self.NavigateToURL(flash_url)
    # Verify shockwave flash process not present.
    self._AssertFlashProcessNotPresent()

  def testFlashIncognitoMode(self):
    """Verify we can play flash on an incognito window."""
    # Verify no flash process is currently running
    self._AssertFlashProcessNotPresent()
    flash_url = self.GetFileURLForDataPath('plugin', 'flash.swf')
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
    self.NavigateToURL(flash_url, 1, 0)
    self._AssertFlashProcessPresent()

  def testFlashWithMultipleTabs(self):
    """Verify we can play flash in multiple tabs."""
    flash_url = self.GetFileURLForDataPath('plugin', 'flash.swf')
    # Verify no flash process is currently running
    self._AssertFlashProcessNotPresent()
    self.NavigateToURL(flash_url)
    # Open 5 new tabs
    for _ in range(5):
      self.AppendTab(pyauto.GURL(flash_url))
    self._AssertFlashProcessPresent()


if __name__ == '__main__':
  pyauto_functional.Main()
