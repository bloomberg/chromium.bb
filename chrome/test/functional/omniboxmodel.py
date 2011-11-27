#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import random

import pyauto_functional
from pyauto import PyUITest
from pyauto import GURL

import autotour


class OmniboxModelTest(autotour.Godel, PyUITest):
  """Omnibox Model which Opens tabs, navigates to specific URL's and keeps track
  of URL's visited which are then verified agains the Omnibox's info.
  """
  def __init__(self, methodName='runTest', **kwargs):
    PyUITest.__init__(self, methodName=methodName, **kwargs)
    self._tab_count = 1
    self._url_map = {}
    self.InitUrls()

  def InitUrls(self):
    """Setup Url Map which stores the name of the url and a list of url and
    visited count.
    """
    self._url_map = {
      'google': ('http://www.google.com', 0),
      'yahoo': ('http://www.yahoo.com', 0),
      'msn': ('http://www.msn.com', 0),
      'facebook': ('http://www.facebook.com', 0),
      'twitter': ('http://www.twitter.com', 0),
    }

  def CanOpenTab(self):
    return self._tab_count < 5

  def CanCloseTab(self):
    return self._tab_count > 1

  def CanNavigate(self):
    # FIX: CanNavigate can be called if there is atleast one tab to be closed.
    # Currently this condition is incorrect because CanCloseTab leaves atleast
    # one tab because without that, there is some crash which is under
    # investigation.
    if self.CanCloseTab():
      return False
    for key in self._url_map:
      if self._url_map[key][1] == 0:
        return True
    return False

  def _GetOmniboxMatchesFor(self, text, windex=0, attr_dict=None):
    """Fetch omnibox matches with the given attributes for the given query.

    Args:
      text: the query text to use
      windex: the window index to work on. Defaults to 0 (first window)
      attr_dict: the dictionary of properties to be satisfied

    Returns:
      a list of match items
    """
    self.SetOmniboxText(text, windex=windex)
    self.WaitUntilOmniboxQueryDone(windex=windex)
    if not attr_dict:
      matches = self.GetOmniboxInfo(windex=windex).Matches()
    else:
      matches = self.GetOmniboxInfo(windex=windex).MatchesWithAttributes(
          attr_dict=attr_dict)
    return matches

  @autotour.GodelAction(1, CanOpenTab)
  def OpenTab(self):
    """Opens a tab in the first window and navigates to a random site from
    url map.
    """
    logging.info('#In Open Tab')
    self._tab_count = self._tab_count + 1
    key = random.choice(self._url_map.keys())
    logging.info('#Navigating to ' + self._url_map[key][0])
    self.AppendTab(GURL(self._url_map[key][0]))
    self._url_map[key][1] = self._url_map[key][1] + 1
    self.VerifyOmniboxInfo()

  @autotour.GodelAction(10, CanCloseTab)
  def CloseTab(self):
    """Closes the first tab from the first window"""
    self._tab_count = self._tab_count - 1
    self.GetBrowserWindow(0).GetTab(0).Close(True)

  def VerifyOmniboxInfo(self):
    for key in self._url_map.keys():
      """Verify inline autocomplete for a pre-visited url."""
      search_for = key[:3]
      matches = self._GetOmniboxMatchesFor(search_for, windex=0)
      self.assertTrue(matches)
      # Omnibox should suggest auto completed url as the first item
      matches_description = matches[0]
      term_to_find = search_for
      if self._url_map[key][1] > 0:
        logging.info('#verifying : ' + key)
        logging.info('#verifying ' + key + ' text ' + search_for)
        term_to_find = self._url_map[key][0][7:]
        self.assertEqual('history-url', matches_description['type'])
        self.assertTrue(self._url_map[key][0][11:] in
          self.GetOmniboxInfo().Text())
      self.assertTrue(term_to_find in matches_description['contents'])

  @autotour.GodelAction(10, CanNavigate)
  def Navigate(self):
    """Navigates to a URL by picking a random url from list"""
    logging.info('#In Navigate')
    index = random.randint(0, len(self._url_map.keys()) - 1)
    key = self._url_map.keys()[index]
    logging.info('#navigating to ' + self._url_map[key][0])
    self.NavigateToURL(self._url_map[key][0])
    self._url_map[key][1] = self._url_map[key][1] + 1
    self.VerifyOmniboxInfo()

  def testExplore(self):
    e = autotour.Explorer()
    logging.info('#Explorer created')
    e.Add(self)
    logging.info('#Object added')
    e.Explore(self.CanNavigate)
    logging.info('#Done')


if __name__ == '__main__':
  pyauto_functional.Main()
