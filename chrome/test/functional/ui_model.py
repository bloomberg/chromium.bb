#!/usr/bin/python

# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Logic for generating random ui actions on the browser.

Takes into account the expected state of the browser in order to generate
relevant ui actions.
"""

import random


class TabState(object):
  """A set of properties representing a browser tab."""

  def __init__(self, location):
    """Init for a new tab.

    Args:
      location: url string for the initial location of this tab.
    """
    self._history = [location]
    self._position = 0

  @property
  def navs(self):
    return self._position

  @property
  def backs(self):
    return len(self._history) - self._position - 1

  @property
  def location(self):
    return self._history[self._position]

  def Navigate(self, target):
    self._history = self._history[:self._position + 1]
    self._position += 1
    self._history.append(target)

  def Back(self):
    assert self.navs > 0, 'illegal Back'
    self._position -= 1

  def Forward(self):
    assert self.backs > 0, 'illegal Forward'
    self._position += 1


class WindowState(object):
  """A set of properties representing state of browser window."""

  def __init__(self, tab=None, private=False):
    self._tabs = []
    self._saved_position = 0
    self._position = 0
    self._private = private
    if tab:
      self._tabs.append(tab)
    else:
      self.NewTab()

  @property
  def tab(self):
    return self._tabs[self._position]

  @property
  def num_tabs(self):
    return len(self._tabs)

  @property
  def tab_position(self):
    return self._position

  @property
  def private(self):
    return self._private

  def NewTab(self, location='chrome://newtab'):
    new_tab = TabState(location)
    self._tabs.append(new_tab)
    self._saved_position = self._position
    self._position = len(self._tabs) - 1

  def FindTab(self, location):
    """Return position of first tab at location, or -1"""
    for position in xrange(self.num_tabs):
      tab = self._tabs[position]
      if tab.location == location:
        return position
    return -1

  def ForgetPosition(self):
    self._saved_position = None

  def DragLeft(self):
    assert self._position > 0, 'illegal DragLeft'
    tab = self.tab
    self._position -= 1
    self._tabs.remove(tab)
    self._tabs.insert(self._position, tab)

  def DragRight(self):
    assert self._position < self.num_tabs - 1, 'illegal DragRight'
    tab = self.tab
    self._position += 1
    self._tabs.remove(tab)
    self._tabs.insert(self._position, tab)

  def RemoveTab(self):
    self._tabs.pop(self._position)
    if self._saved_position != None:
      self._position = self._saved_position
    if self._position == self.num_tabs:
      self._position -= 1
    self.ForgetPosition()

  def RestoreTab(self, tab, position):
    self._tabs.insert(position, tab)
    self._position = position
    self.ForgetPosition()

  def Focus(self, position):
    self._position = position
    self.ForgetPosition()

  def TabLeft(self):
    if self._position == 0:
      self.Focus(self.num_tabs - 1)
    else:
      self.Focus(self._position - 1)

  def TabRight(self):
    if self._position == self.num_tabs - 1:
      self.Focus(0)
    else:
      self.Focus(self._position + 1)


class BrowserState(object):
  """A set of properties representing browser after an action sequence."""

  def __init__(self, advanced_actions=False):
    self._windows = []
    self._focus_stack = []
    self._closed = []
    blank_tab = TabState('about:blank')
    self.NewWindow(tab=blank_tab)
    self.advanced = advanced_actions

  @property
  def num_windows(self):
    return len(self._windows)

  @property
  def window(self):
    return self._focus_stack[self.num_windows - 1]

  @property
  def window_position(self):
    return self._windows.index(self.window)

  def NewWindow(self, tab=None, private=False):
    window = WindowState(tab=tab, private=private)
    self._windows.append(window)
    self._focus_stack.append(window)

  def RemoveWindow(self):
    assert self.num_windows > 1, 'not enough windows to RemoveWindow'
    window = self._focus_stack.pop()
    window.ForgetPosition()
    self._windows.remove(window)
    self._Remember(window)

  def _Remember(self, window, tab=None, position=None):
    if window.private:
      return
    self._closed.append((window, tab, position))
    if len(self._closed) > 10:
      self._closed = self._closed[-10:]

  def _Focus(self, position):
    window = self._windows[position]
    self._focus_stack.remove(window)
    self._focus_stack.append(window)

  def NewTab(self, location='chrome://newtab'):
    self.window.NewTab(location)

  def Downloads(self):
    position = self.window.FindTab('chrome://downloads')
    if position >= 0:
      self.window.Focus(position)
    else:
      self.NewTab('chrome://downloads')
    self.window.ForgetPosition()

  def RemoveTab(self, destroy_tab=True):
    """Remove active tab from active window.

    Args:
      destroy_tab: boolean, true if the tab is being closed.
    """
    assert self.window.num_tabs > 1 or self.num_windows > 1, 'illegal RemoveTab'
    if self.window.num_tabs == 1:
      self.RemoveWindow()
      return
    if destroy_tab and not self.window.private:
      self._Remember(self.window, self.window.tab, self.window.tab_position)
    self.window.RemoveTab()

  def CanRestore(self):
    """Return True if Restore is a valid action."""
    if self.window.private:
      return False
    if self._closed:
      return True
    return False

  def Restore(self):
    """Restore a previously removed tab/window.

    Expected behavior:
    - If a private window is in focus, you cannot restore tabs.
    - If the last removed tab was in a different window, that window comes back
    into focus.
    - If the window is gone, a new window will come to focus with the restored
      tab.
    - Tabs restore to the same index they were removed at, else the last
      position.
    """
    assert self._closed, 'nothing to Restore'
    assert not self.window.private, 'cannot Restore (private window)'
    window, tab, position = self._closed.pop()
    try:
      i = self._windows.index(window)
      self._Focus(i)
    except ValueError:
      pass
    if self.window == window:
      self.window.RestoreTab(tab, position)
    else:
      self._windows.append(window)
      self._focus_stack.append(window)

  def DragOut(self):
    """Drag tab out of window, spawns new window."""
    assert self.window.num_tabs > 1, 'not enough tabs to DragOut'
    tab = self.window.tab
    self.RemoveTab(destroy_tab=False)
    self.NewWindow(tab=tab, private=self.window.private)

  def DragLeft(self):
    self.window.DragLeft()

  def DragRight(self):
    self.window.DragRight()

  def Navigate(self, target):
    self.window.ForgetPosition()
    self.window.tab.Navigate(target)

  def Back(self):
    self.window.tab.Back()

  def Forward(self):
    self.window.tab.Forward()

  def NextTab(self):
    self.window.TabRight()

  def LastTab(self):
    self.window.TabLeft()


def UpdateState(browser, action):
  """Return new browser state after performing action.

  Args:
    browser: current browser state.
    action: next action performed.

  Returns:
    new browser state.
  """
  a = action.split(';')[0]
  if a == 'openwindow':
    browser.NewWindow()
  elif a == 'goofftherecord':
    browser.NewWindow(private=True)
  elif a == 'newtab':
    browser.NewTab()
  elif a == 'dragtabout':
    browser.DragOut()
  elif a == 'dragtableft':
    browser.DragLeft()
  elif a == 'dragtabright':
    browser.DragRight()
  elif a == 'closetab':
    browser.RemoveTab()
  elif a == 'closewindow':
    browser.RemoveWindow()
  elif a == 'restoretab':
    browser.Restore()
  elif a == 'navigate':
    browser.Navigate(action.split(';')[1])
  elif a == 'downloads':
    browser.Downloads()
  elif a == 'back':
    browser.Back()
  elif a == 'forward':
    browser.Forward()
  elif a == 'nexttab':
    browser.NextTab()
  elif a == 'lasttab':
    browser.LastTab()
  return browser


def GetRandomAction(browser):
  """Return a random possible action for a UI sequence in given state.

  Args:
    browser: current browser state.

  Returns:
    UI action (string).
  """
  possible_actions = []

  def AddActionWithWeight(action, weight=1):
    """Add action to possible actions list with given weight.

    Args:
      action: action string.
      weight: integer weight value.
    """
    for _ in xrange(weight):
      possible_actions.append(action)

  AddActionWithWeight('showbookmarks')
  if browser.num_windows < 6:
    AddActionWithWeight('openwindow')
  if browser.window.num_tabs < 10:
    AddActionWithWeight('newtab', weight=3)

  # Throw in some navigates to generate a realistic environment.
  nav_options = ['http://www.craigslist.com',
                 'http://www.google.com',
                 'http://www.bing.com']
  for location in nav_options:
    if browser.window.tab.location != location:
      AddActionWithWeight('navigate;%s' % location)

  if browser.window.tab.location != 'Downloads':
    AddActionWithWeight('downloads')

  # Actions on a web page.
  if browser.window.tab.navs > 0:
    AddActionWithWeight('star')
    AddActionWithWeight('zoomplus')
    AddActionWithWeight('zoomminus')
    AddActionWithWeight('pagedown', weight=3)

  # Other conditional actions.
  if browser.window.tab.navs > 0:
    AddActionWithWeight('back', weight=3)
  if browser.window.tab.backs > 0:
    AddActionWithWeight('forward', weight=2)
  if browser.window.num_tabs > 1 or browser.num_windows > 1:
    AddActionWithWeight('closetab', weight=2)
  if browser.window.num_tabs > 1:
    AddActionWithWeight('dragtabout')
    if browser.window.tab_position > 0:
      AddActionWithWeight('dragtableft')
    if browser.window.tab_position < browser.window.num_tabs - 1:
      AddActionWithWeight('dragtabright')

  # (v2) actions.  separated for backwards compatability.
  if browser.advanced:
    if browser.num_windows > 1:
      AddActionWithWeight('closewindow')
    #TODO(ace): fix support for restore action.
    #if browser.CanRestore():
    #  AddActionWithWeight('restoretab')
    if browser.window.tab_position > 0:
      AddActionWithWeight('lasttab')
    if browser.window.tab_position < browser.window.num_tabs - 1:
      AddActionWithWeight('nexttab')
    if browser.num_windows < 6:
      AddActionWithWeight('goofftherecord')

  return ChooseFrom(possible_actions)


def Seed(seed=None):
  """Seed random for reproducible command sequences.

  Args:
    seed: optional long int input for random.seed.  If none is given,
          one is generated.

  Returns:
    The given or generated seed.
  """
  if not seed:
    random.seed()
    seed = random.getrandbits(64)
  random.seed(seed)
  return seed


def ChooseFrom(choice_list):
  """Return a random choice from given list.

  Args:
    choice_list: list of possible choices.

  Returns:
    One random element from choice_list
  """
  return random.choice(choice_list)
