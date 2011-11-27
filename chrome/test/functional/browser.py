#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
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
    while True:
      raw_input('Hit <enter> to dump info.. ')
      self.pprint(self.GetBrowserInfo())

  def setUp(self):
    pyauto.PyUITest.setUp(self)
    self._flash_plugin_type = 'Plug-in'
    if (self.IsChromeOS() and
        self.GetBrowserInfo()['properties']['branding'] == 'Google Chrome'):
      self._flash_plugin_type = 'Pepper Plugin'

  def _GetUniqProcesses(self, total_tabs, renderer_processes):
    """ Returns a count of uniq processes of opened tabs

    Args:
      total_tabs: opened tabs count
      renderer_processes: opened renderers info data dictionary
    """
    pid_list = []
    for tab_index in range(total_tabs):
      pid = renderer_processes[tab_index]['renderer_pid']
      if pid not in pid_list:
        pid_list.append(pid)
    return len(pid_list)

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
    url = self.GetFileURLForDataPath('title2.html')
    self.NavigateToURL(url)
    info = self.GetBrowserInfo()
    # Verify valid version string
    version_string = info['properties']['ChromeVersion']
    logging.info('ChromeVersion: %s' % version_string)
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
    url = self.GetFileURLForDataPath('title2.html')
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
      self.assertEqual(x, info['windows'][0]['x'],
                       msg='Window x value should be %d, but was %d.' % (
                           x, info['windows'][0]['x']))
      self.assertEqual(y, info['windows'][0]['y'],
                       msg='Window y value should be %d, but was %d.' % (
                           y, info['windows'][0]['y']))
      self.assertEqual(width, info['windows'][0]['width'],
                       msg='Window width value should be %d, but was %d.' % (
                           width, info['windows'][0]['width']))
      self.assertEqual(height, info['windows'][0]['height'],
                       msg='Window height value should be %d, but was %d.' % (
                           height, info['windows'][0]['height']))

    self.SetWindowDimensions(x=20, y=40, width=600, height=300)
    _VerifySize(20, 40, 600, 300)
    self.RestartBrowser(clear_profile=False)
    _VerifySize(20, 40, 600, 300)

  def testMaxProcess(self):
    """Verify that opening 100 tabs doesn't create 100 child processes"""
    total_tabs = 100
    test_url = self.GetFileURLForDataPath('english_page.html')
    # Opening tabs
    for tab_index in range(total_tabs - 1):
      self.AppendTab(pyauto.GURL(test_url))
      tabs = self.GetBrowserInfo()['windows'][0]['tabs']
      # For the first time we have 2 tabs opened, so sending the tab_index as +2
      unique_renderers = self._GetUniqProcesses(len(tabs), tabs)
      # We verify that opening a new tab should not create a new process after
      # Chrome reaches to a maximum process limit.
      if len(tabs) > unique_renderers:
        return
    # In case if we create 100 processes for 100 tabs, then we are failing.
    self.fail(msg='Got 100 renderer processes')

  def testKillAndReloadRenderer(self):
    """Verify that reloading of renderer is possible,
       after renderer is killed"""
    test_url = self.GetFileURLForDataPath('english_page.html')
    self.NavigateToURL(test_url)
    pid1 = self.GetBrowserInfo()['windows'][0]['tabs'][0]['renderer_pid']
    self.KillRendererProcess(pid1)
    self.ReloadActiveTab()
    pid2 = self.GetBrowserInfo()['windows'][0]['tabs'][0]['renderer_pid']
    self.assertNotEqual(pid1, pid2)

  def testPopupSharesProcess(self):
    """Verify that parent tab and popup share a process."""
    file_url = self.GetFileURLForDataPath(
        'popup_blocker', 'popup-window-open.html')
    self.NavigateToURL(file_url)
    blocked_popups = self.GetBlockedPopupsInfo()
    self.assertEqual(1, len(blocked_popups), msg='Popup not blocked')
    self.UnblockAndLaunchBlockedPopup(0)
    self.assertEquals(2, self.GetBrowserWindowCount())
    parent_pid = self.GetBrowserInfo()['windows'][0]['tabs'][0]['renderer_pid']
    popup_pid = self.GetBrowserInfo()['windows'][1]['tabs'][0]['renderer_pid']
    self.assertEquals(popup_pid, parent_pid,
                      msg='Parent and popup are not sharing a process.')

  def testPopupSharesSameProcessInIncognito(self):
    """Verify parent incognito and popup share same process id"""
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
    file_url = self.GetFileURLForDataPath('popup_blocker',
                                          'popup-window-open.html')
    self.NavigateToURL(file_url, 1, 0)
    self.UnblockAndLaunchBlockedPopup(0, tab_index=0, windex=1)
    self.assertEquals(
          self.GetBrowserInfo()['windows'][1]['tabs'][0]['renderer_pid'],
          self.GetBrowserInfo()['windows'][2]['tabs'][0]['renderer_pid'],
          msg='Incognito window and popup are not sharing a process id.')

  def testKillAndReloadSharedProcess(self):
    """Verify that killing a shared process kills all associated renderers.
    In this case we are killing a process shared by a parent and
    its popup process. Reloading both should share a process again.
    """
    file_url = self.GetFileURLForDataPath(
        'popup_blocker', 'popup-window-open.html')
    self.NavigateToURL(file_url)
    blocked_popups = self.GetBlockedPopupsInfo()
    self.assertEqual(1, len(blocked_popups), msg='Popup not blocked')
    self.UnblockAndLaunchBlockedPopup(0)
    self.assertEquals(2, self.GetBrowserWindowCount())
    # Check that the renderers are alive.
    self.assertEquals(1, self.FindInPage('pop-up')['match_count'])
    self.assertEquals(1,
        self.FindInPage('popup', tab_index=0, windex=1)['match_count'])
    # Check if they are sharing a process id.
    self.assertEquals(
        self.GetBrowserInfo()['windows'][0]['tabs'][0]['renderer_pid'],
        self.GetBrowserInfo()['windows'][1]['tabs'][0]['renderer_pid'])
    shared_pid = self.GetBrowserInfo()['windows'][0]['tabs'][0]['renderer_pid']
    # This method would fail if the renderers are not killed.
    self.KillRendererProcess(shared_pid)

    # Reload the parent and popup windows.
    self.GetBrowserWindow(0).GetTab(0).Reload()
    self.GetBrowserWindow(1).GetTab(0).Reload()
    # Check if both are sharing a process id.
    self.assertEquals(
        self.GetBrowserInfo()['windows'][0]['tabs'][0]['renderer_pid'],
        self.GetBrowserInfo()['windows'][1]['tabs'][0]['renderer_pid'])
    # The shared process id should be different from the previous one.
    self.assertNotEqual(shared_pid,
        self.GetBrowserInfo()['windows'][0]['tabs'][0]['renderer_pid'])

if __name__ == '__main__':
  pyauto_functional.Main()
