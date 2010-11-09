#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

import pyauto_functional
from pyauto import PyUITest
from pyauto import GURL

import autotour


class ChromeTest(autotour.Godel, PyUITest):
  """Test Example class which explores OpenTab, CloseTab and Navigate
  actions and calls them randomly using the Exploratory framework.
  Each method defines a weight and a precondition to test
  using the GodelAction decorator before it can be called. The exploratory
  framework uses these annotation and automatically calls different actions
  and drives browser testing.
  """
  def __init__(self, methodName='runTest', **kwargs):
    PyUITest.__init__(self, methodName=methodName, **kwargs)
    self._tab_count = 0

  def CanOpenTab(self):
    """Pre condition for opening a tab."""
    return self._tab_count < 5

  def CanCloseTab(self):
    """Pre condition for closing a tab."""
    return self._tab_count > 1

  @autotour.GodelAction(10, CanOpenTab)
  def OpenTab(self):
    """Opens a new tab and goes to Google.com in the first window."""
    logging.info('In Open Tab')
    self._tab_count = self._tab_count + 1
    self.AppendTab(GURL('http://www.google.com'))

  @autotour.GodelAction(10, CanCloseTab)
  def CloseTab(self):
    """Closes the first tab in the first window."""
    logging.info('In Close Tab')
    self._tab_count = self._tab_count - 1
    self.GetBrowserWindow(0).GetTab(0).Close(True)

  @autotour.GodelAction(10, CanCloseTab)
  def Navigate(self):
    """Navigates to yahoo.com in the current window."""
    logging.info("In Navigate")
    self.NavigateToURL('http://www.yahoo.com')

  def testExplore(self):
    e = autotour.Explorer()
    logging.info('Explorer created')
    e.Add(self)
    logging.info('Object added')
    e.Explore()
    logging.info('Done')


if __name__ == '__main__':
  pyauto_functional.Main()

