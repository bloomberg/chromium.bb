#!/usr/bin/python

# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import cgi
import os

import pyauto_functional  # Must be imported before pyauto
import pyauto


class InstantTest(pyauto.PyUITest):
  """TestCase for Omnibox Instant feature."""

  def setUp(self):
    pyauto.PyUITest.setUp(self)
    self.SetPrefs(pyauto.kInstantEnabled, True)

  def _DoneLoading(self):
    info = self.GetInstantInfo()
    return info.get('current') and not info.get('loading')

  def _DoneLoadingGoogleQuery(self, query):
    """Wait for Omnibox Instant to load Google search result
       and verify location URL contains the specifed query.

       Args:
         query: Value of query parameter.
                E.g., http://www.google.com?q=hi so query is 'hi'.
    """
    self.assertTrue(self.WaitUntil(self._DoneLoading))
    location = self.GetInstantInfo().get('location')
    if location is not None:
      q = cgi.parse_qs(location).get('q')
      if q is not None and query in q:
        return True
    return False

  def testInstantNavigation(self):
    """Test that instant navigates based on omnibox input."""
    self.SetOmniboxText('google.com')
    self.assertTrue(self.WaitUntil(self._DoneLoading))
    location = self.GetInstantInfo()['location']
    self.assertTrue('google.com' in location,
                    msg='No google.com in %s' % location)

    self.SetOmniboxText('google.es')
    self.assertTrue(self.WaitUntil(self._DoneLoading))
    location = self.GetInstantInfo()['location']
    self.assertTrue('google.es' in location,
                    msg='No google.es in %s' % location)

    # Initiate instant search (at default google.com).
    self.SetOmniboxText('chrome instant')
    self.assertTrue(self.WaitUntil(self._DoneLoading))
    location = self.GetInstantInfo()['location']
    self.assertTrue('google.com' in location,
                    msg='No google.com in %s' % location)

  def testInstantDisabledInIncognito(self):
    """Test that instant is disabled in Incognito mode."""
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
    self.SetOmniboxText('google.com', windex=1)
    self.assertFalse(self.GetInstantInfo()['active'],
                     'Instant enabled in Incognito mode.')

  def testInstantOverlayNotStoredInHistory(self):
    """Test that instant overlay page is not stored in history."""
    url = self.GetFileURLForPath(os.path.join(self.DataDir(), 'title2.html'))
    self.SetOmniboxText(url)
    self.assertTrue(self.WaitUntil(self._DoneLoading))
    history = self.GetHistoryInfo().History()
    self.assertEqual(0, len(history))

  def testInstantDisabledForJavaScript(self):
    """Test that instant is disabled for javascript URLs."""
    self.SetOmniboxText('javascript:')
    self.assertFalse(self.GetInstantInfo()['active'],
                     'Instant enabled for javascript URL.')

  def testInstantDisablesPopupsOnPrefetch(self):
    """Test that instant disables popups when prefetching."""
    file_url = self.GetFileURLForPath(os.path.join(
        self.DataDir(), 'popup_blocker', 'popup-blocked-to-post-blank.html'))
    self.SetOmniboxText(file_url)
    self.assertTrue(self.WaitUntil(self._DoneLoading))
    location = self.GetInstantInfo()['location']
    self.assertTrue(file_url in location,
                    msg='Prefetched page is not %s' % file_url)
    blocked_popups = self.GetBlockedPopupsInfo()
    self.assertEqual(0, len(blocked_popups),
                     msg='Unexpected popup in instant preview.')

  def testInstantLoadsFor100CharsLongQuery(self):
    """Test that instant loads for search query of 100 characters."""
    query = '#' * 100
    self.SetOmniboxText(query)
    self.assertTrue(self.WaitUntil(self._DoneLoadingGoogleQuery, args=[query]))


if __name__ == '__main__':
  pyauto_functional.Main()
