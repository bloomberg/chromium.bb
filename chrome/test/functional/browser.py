#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import types

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

  def _VerifyUniqueRendererProcesses(self, browser_info):
    """Verify that each tab has a unique renderer process.

    This cannot be used for large number of renderers since there actually is
    a cap, depending on memory on the machine.

    Args:
      browser_info: browser info data dictionary as returned by GetBrowserInfo()
    """
    seen_pids = {}  # lookup table of PIDs we've seen
    for window in browser_info['windows']:
      for tab in window['tabs']:
        renderer_pid = tab['renderer_pid']
        self.assertEqual(types.IntType, type(renderer_pid))
        # Verify unique renderer pid
        self.assertFalse(renderer_pid in seen_pids, 'renderer pid not unique')
        seen_pids[renderer_pid] = True

  def testBasics(self):
    """Verify basic browser info at startup."""
    url = self.GetFileURLForPath(os.path.join(self.DataDir(), 'title2.html'))
    self.NavigateToURL(url)
    info = self.GetBrowserInfo()
    # Verify valid version string
    version_string = info['properties']['ChromeVersion']
    self.assertTrue(re.match('\d+\.\d+\.\d+.\.\d+', version_string))
    # Verify browser process
    self.assertEqual(types.IntType, type(info['browser_pid']))
    self.assertEqual(1, len(info['windows']))  # one window
    self.assertEqual(1, len(info['windows'][0]['tabs']))  # one tab
    self.assertEqual(0, info['windows'][0]['selected_tab'])  # 1st tab selected
    self.assertEqual(url, info['windows'][0]['tabs'][0]['url'])
    self.assertFalse(info['windows'][0]['fullscreen'])  # not fullscreen
    self._VerifyUniqueRendererProcesses(info)

  def testProcessesForMultipleWindowsAndTabs(self):
    """Verify processes for multiple windows and tabs"""
    url = self.GetFileURLForPath(os.path.join(self.DataDir(), 'title2.html'))
    self.NavigateToURL(url)
    for _ in range(2):
      self.AppendTab(pyauto.GURL(url))
    for windex in range(1, 3):  # 2 new windows
      self.OpenNewBrowserWindow(True)
      self.NavigateToURL(url, windex, 0)
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)  # incognito window
    self.NavigateToURL(url, 3, 0)
    self._VerifyUniqueRendererProcesses(self.GetBrowserInfo())

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

  def _GetFlashProcessesInfo(self):
    """Get info about flash processes, if any."""
    return [x for x in self.GetBrowserInfo()['child_processes']
            if x['type'] == 'Plug-in' and x['name'] == 'Shockwave Flash']

  def testSingleFlashPluginProcess(self):
    """Verify there's only one flash plugin process shared across all uses."""
    flash_url = self.GetFileURLForPath(os.path.join(self.DataDir(),
                                                    'plugin', 'flash.swf'))
    self.NavigateToURL(flash_url)
    for _ in range(2):
      self.AppendTab(pyauto.GURL(flash_url))
    # Open flash in new window
    self.OpenNewBrowserWindow(True)
    self.NavigateToURL(flash_url, 1, 0)
    # Open flash in new incognito window
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
    self.NavigateToURL(flash_url, 1, 0)
    # Verify there's only 1 flash process
    self.assertEqual(1, len(self._GetFlashProcessesInfo()))

  def testFlashLoadsAfterKill(self):
    """Verify that Flash process reloads after crashing (or being killed)."""
    flash_url = self.GetFileURLForPath(os.path.join(self.DataDir(),
                                                    'plugin', 'flash.swf'))
    self.NavigateToURL(flash_url)
    flash_process_id1 = self._GetFlashProcessesInfo()[0]['pid']
    self.Kill(flash_process_id1)
    self.GetBrowserWindow(0).GetTab(0).Reload()  # Reload
    flash_processes = self._GetFlashProcessesInfo()
    self.assertEqual(1, len(flash_processes))
    self.assertNotEqual(flash_process_id1, flash_processes[0]['pid'])


if __name__ == '__main__':
  pyauto_functional.Main()

