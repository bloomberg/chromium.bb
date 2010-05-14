#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

import pyauto_functional  # Must be imported before pyauto
import pyauto


class OmniboxTest(pyauto.PyUITest):
  """TestCase for Omnibox."""

  def Debug(self):
    """Test method for experimentation.

    This method will not run automatically.
    """
    import pprint
    import time
    pp = pprint.PrettyPrinter(indent=2)
    while True:
      pp.pprint(self.GetOmniboxInfo().omniboxdict)
      time.sleep(1)

  def testHistoryResult(self):
    """Verify that omnibox can fetch items from history."""
    url = self.GetFileURLForPath(os.path.join(self.DataDir(), 'title2.html'))
    title = 'Title Of Awesomeness'
    self.AppendTab(pyauto.GURL(url))
    def _VerifyResult(query_text, description):
      """Verify result matching given description for given query_text."""
      self.SetOmniboxText(query_text)
      self.WaitUntilOmniboxQueryDone()
      info = self.GetOmniboxInfo()
      self.assertTrue(info.Matches())
      matches = info.MatchesWithAttributes({'description': description})
      self.assertTrue(matches)
      self.assertEqual(1, len(matches))
      item = matches[0]
      self.assertEqual(url, item['destination_url'])
    # Query using URL
    _VerifyResult(url, title)
    # Query using title
    _VerifyResult(title, title)

  def testSelect(self):
    """Verify omnibox popup selection."""
    url1 = self.GetFileURLForPath(os.path.join(self.DataDir(), 'title2.html'))
    url2 = self.GetFileURLForPath(os.path.join(self.DataDir(), 'title1.html'))
    title1 = 'Title Of Awesomeness'
    self.NavigateToURL(url1)
    self.NavigateToURL(url2)
    self.SetOmniboxText('file://')
    self.WaitUntilOmniboxQueryDone()
    matches = self.GetOmniboxInfo().Matches()
    # Find the index of match for url1
    index = None
    for i, match in enumerate(matches):
      if match['description'] == title1:
        index = i
    self.assertTrue(index is not None)
    self.OmniboxMovePopupSelection(index)  # Select url1 line in popup
    self.assertEqual(url1, self.GetOmniboxInfo().Text())
    self.OmniboxAcceptInput()
    self.assertEqual(title1, self.GetActiveTabTitle())


if __name__ == '__main__':
  pyauto_functional.Main()

