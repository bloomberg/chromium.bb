#!/usr/bin/python

# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Pyauto powered ui action runner.

Developed primarily to verify validity of the model based action generator.
"""

import time
import sys

import pyauto_functional
import pyauto
import ui_model

class Runner(pyauto.PyUITest):

  def setUp(self):
    self.debug_mode = False
    pyauto.PyUITest.setUp(self)

  def RunActionList(self):
    """Runs actions from a file."""
    f = open('list')
    actions = f.readlines()
    self.browser = ui_model.BrowserState(advanced_actions=True)
    count = 0
    for action in actions:
      count += 1
      sys.stdout.write('%d > ' % count)
      action = action.strip()
      self.ApplyAction(action)
    raw_input('Press key to continue.')


  def DebugUIActions(self):
    """Run testUIActions with debug mode on.

    Allows inspection of the browser after unexpected state is encountered.
    """
    self.debug_mode = True
    self.testUIActions()

  def testUIActions(self):
    """Generates and runs actions forever."""
    self.browser = ui_model.BrowserState(advanced_actions=True)
    count = 0
    start_time = time.time()
    while True:
      count += 1
      sys.stdout.write('%d:%.3f > ' % (count, time.time() - start_time))
      action = ui_model.GetRandomAction(self.browser)
      self.ApplyAction(action)

  def ApplyAction(self, action):
    sys.stdout.write('%s, ' % action)
    if self._DoAction(action):
      ui_model.UpdateState(self.browser, action)
    self._CheckState()

  def Error(self, msg=''):
    """Called when an unexpected state is encountered."""
    if msg:
      print 'Error: %s' % msg
    else:
      print 'Error'
    while self.debug_mode:
      raw_input('Press key to continue.')
    assertTrue(False, msg)

  def _CheckState(self):
    """Check some basic properties of the browser against expected state."""
    active_window = self.browser.window_position
    active_tab = self.GetActiveTabIndex(active_window)
    expected_tab = self.browser.window.tab_position
    print 'win: %d tab: %d navs: %d backs: %d' % (
        active_window, active_tab, self.browser.window.tab.navs,
        self.browser.window.tab.backs)
    if active_tab != expected_tab:
      self.Error('active index out of sync: expected %d' % expected_tab)
    tab_count = self.GetTabCount(active_window)
    expected_count = self.browser.window.num_tabs
    if tab_count != expected_count:
      self.Error('tab count out of sync: count: %d expected: %d' % (
          tab_count, expected_count))
    window_count = self.GetBrowserWindowCount()
    expected_count = self.browser.num_windows
    if window_count != expected_count:
      self.Error('window count out of sync: count: %d expected %d' % (
          window_count, expected_count))

  def _GrabTab(self):
    active_window = self.browser.window_position
    window = self.GetBrowserWindow(active_window)
    tab_count = self.GetTabCount(self.browser.window_position)
    active_tab = self.browser.window.tab_position
    if active_tab >= tab_count:
      self.Error('active tab out of bounds: count: %d expected active: %d' % (
                 (tab_count, active_tab)))
    return window.GetTab(self.browser.window.tab_position)

  def _RunInActiveWindow(self, command):
    active_window = self.browser.window_position
    self.RunCommand(command, active_window)

  def _RunAsyncInActiveWindow(self, command):
    active_window = self.browser.window_position
    self.ApplyAccelerator(command, active_window)

  def _Zoom(self, command):
    active_window = self.browser.window_position
    title = self.GetActiveTabTitle(active_window)
    model_active_tab = self.browser.window.tab_position
    active_tab = self.GetActiveTabIndex(active_window)
    num_tabs = self.GetTabCount(active_window)
    self._RunAsyncInActiveWindow(command)
    if title == 'New Tab':
      self.Error('zoom called on new tab')

  def _WaitFor(self, test):
    start = time.time()
    test_result, detail = test()
    while not test_result:
      if time.time() - start > self.action_max_timeout_ms():
        self.Error('TIMEOUT: %s' % detail)
      time.sleep(.1)
      test_result, detail = test()

  def _DoAction(self, action):
    """Execute action in the browser.

    Attempts to simulate synchronous execution for most actions.

    Args:
      action: action string.
    """
    a = action.split(';')[0]
    if a == 'showbookmarks':
      self._RunAsyncInActiveWindow(pyauto.IDC_SHOW_BOOKMARK_BAR)
    if a == 'openwindow' or a == 'goofftherecord':
      def NewWindowHasTab():
        result = self.GetTabCount(self.browser.num_windows) == 1
        return (result, 'NewWindowHasTab')
      def TabLoaded():
        result = self.GetActiveTabTitle(self.browser.num_windows) == 'New Tab'
        return (result, 'TabLoaded')
      if a == 'openwindow':
        self.OpenNewBrowserWindow(True)
      elif a == 'goofftherecord':
        self._RunAsyncInActiveWindow(pyauto.IDC_NEW_INCOGNITO_WINDOW)
      self._WaitFor(NewWindowHasTab)
      self._WaitFor(TabLoaded)
    if a == 'newtab':
      active_window = self.browser.window_position
      target = pyauto.GURL('chrome://newtab')
      self.AppendTab(target, active_window)
    if a == 'downloads':
      active_window = self.browser.window_position
      def TabLoaded():
        result = self.GetActiveTabTitle(active_window) == 'Downloads'
        return (result, 'TabLoaded')
      self._RunAsyncInActiveWindow(pyauto.IDC_SHOW_DOWNLOADS)
      self._WaitFor(TabLoaded)
    if a == 'star':
      self._RunAsyncInActiveWindow(pyauto.IDC_BOOKMARK_PAGE)
    if a == 'zoomplus':
      self._Zoom(pyauto.IDC_ZOOM_PLUS)
    if a == 'zoomminus':
      self._Zoom(pyauto.IDC_ZOOM_MINUS)
    if a == 'pagedown':
      return False
    if a == 'back' or a == 'forward' or a == 'navigate':
      tab = self._GrabTab()
      active_window = self.browser.window_position
      old_title = self.GetActiveTabTitle(active_window)
      retries = 0
      nav_result = 0
      while nav_result != 1:
        if retries == 1:
          break
        if retries == 1:
          sys.stdout.write('retry ')
        if retries > 0:
          time.sleep(.1)
          sys.stdout.write('%d, ' % retries)
        if a == 'navigate':
          target = pyauto.GURL(action.split(';')[1])
          nav_result = tab.NavigateToURL(target)
        elif a == 'back':
          self.browser.Back()
          self.browser.Forward()
          nav_result = tab.GoBack()
        elif a == 'forward':
          self.browser.Forward()
          self.browser.Back()
          nav_result = tab.GoForward()
        retries += 1
    if a == 'closetab':
      tab = self._GrabTab()
      ui_model.UpdateState(self.browser, action)
      active_window = self.browser.window_position
      window_count = self.browser.num_windows
      tab_count = self.browser.window.num_tabs
      def WindowCount():
        actual = self.GetBrowserWindowCount()
        result = actual == window_count
        return (result, 'WindowCount (expected %d, actual %d)' %
                (window_count, actual))
      def TabCount():
        actual = self.GetTabCount(active_window)
        result = actual == tab_count
        return (result, 'TabCount (expected %d, actual %d)' %
                (tab_count, actual))
      tab.Close(True)
      self._WaitFor(WindowCount)
      self._WaitFor(TabCount)
      return False
    if a == 'closewindow':
      window_count = self.browser.num_windows - 1
      def WindowCount():
        result = self.GetBrowserWindowCount() == window_count
        return (result, 'WindowCount (expected %d)' % window_count)
      self._RunInActiveWindow(pyauto.IDC_CLOSE_WINDOW)
      self._WaitFor(WindowCount)
    if a == 'dragtabout':
      return False
    if a == 'dragtableft':
      self._RunAsyncInActiveWindow(pyauto.IDC_MOVE_TAB_PREVIOUS)
    if a == 'dragtabright':
      self._RunAsyncInActiveWindow(pyauto.IDC_MOVE_TAB_NEXT)
    if a == 'lasttab':
      self._RunAsyncInActiveWindow(pyauto.IDC_SELECT_PREVIOUS_TAB)
    if a == 'nexttab':
      self._RunAsyncInActiveWindow(pyauto.IDC_SELECT_NEXT_TAB)
    if a == 'restoretab':
      active_window = self.browser.window_position
      self.ApplyAccelerator(pyauto.IDC_RESTORE_TAB, active_window)
      self._GrabTab().WaitForTabToBeRestored(self.action_max_timeout_ms())
      ui_model.UpdateState(self.browser, action)
      return False
    return True


if __name__ == '__main__':
  pyauto_functional.Main()