#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re

import pyauto_functional  # Must be imported before pyauto
import pyauto
import test_utils


class SearchEnginesTest(pyauto.PyUITest):
  """TestCase for Search Engines."""

  _localhost_prefix = 'http://localhost:1000/'

  def _GetSearchEngineWithKeyword(self, keyword):
    """Get search engine info and return an element that matches keyword.

    Args:
      keyword: Search engine keyword field.

    Returns:
      A search engine info dict or None.
    """
    match_list = ([x for x in self.GetSearchEngineInfo()
                   if x['keyword'] == keyword])
    if match_list:
      return match_list[0]
    return None

  def Debug(self):
    """Test method for experimentation.

    This method will not run automatically.
    """
    while True:
      raw_input('Interact with the browser and hit <enter>')
      self.pprint(self.GetSearchEngineInfo())

  def testDiscoverSearchEngine(self):
    """Test that chrome discovers youtube search engine after searching."""
    # Take a snapshot of current search engine info.
    info = self.GetSearchEngineInfo()
    youtube = self._GetSearchEngineWithKeyword('youtube.com')
    self.assertFalse(youtube)
    # Use omnibox to invoke search engine discovery.
    # Navigating using NavigateToURL does not currently invoke this logic.
    self.SetOmniboxText('http://www.youtube.com')
    self.OmniboxAcceptInput()
    def InfoUpdated(old_info):
      new_info = self.GetSearchEngineInfo()
      if len(new_info) > len(old_info):
        return True
      return False
    self.WaitUntil(lambda: InfoUpdated(info))
    youtube = self._GetSearchEngineWithKeyword('youtube.com')
    self.assertTrue(youtube)
    self.assertTrue(re.search('youtube', youtube['short_name'],
                              re.IGNORECASE))
    self.assertFalse(youtube['in_default_list'])
    self.assertFalse(youtube['is_default'])

  def testDeleteSearchEngine(self):
    """Test adding then deleting a search engine."""
    self.AddSearchEngine(title='foo',
                         keyword='foo.com',
                         url='http://foo/?q=%s')
    foo = self._GetSearchEngineWithKeyword('foo.com')
    self.assertTrue(foo)
    self.DeleteSearchEngine('foo.com')
    foo = self._GetSearchEngineWithKeyword('foo.com')
    self.assertFalse(foo)

  def testMakeSearchEngineDefault(self):
    """Test adding then making a search engine default."""
    self.AddSearchEngine(
        title='foo',
        keyword='foo.com',
        url=self._localhost_prefix + '?q=%s')
    foo = self._GetSearchEngineWithKeyword('foo.com')
    self.assertTrue(foo)
    self.assertFalse(foo['is_default'])
    self.MakeSearchEngineDefault('foo.com')
    foo = self._GetSearchEngineWithKeyword('foo.com')
    self.assertTrue(foo)
    self.assertTrue(foo['is_default'])
    self.SetOmniboxText('foobar')
    self.OmniboxAcceptInput()
    self.assertEqual(self._localhost_prefix + '?q=foobar',
                     self.GetActiveTabURL().spec())

  def testDefaultSearchEngines(self):
    """Test that we have 3 default search options."""
    info = self.GetSearchEngineInfo()
    self.assertEqual(len(info), 3)
    # Verify that each can be used as the default search provider.
    default_providers = ['google.com', 'yahoo.com', 'bing.com']
    for keyword in default_providers:
      self.MakeSearchEngineDefault(keyword)
      search_engine = self._GetSearchEngineWithKeyword(keyword)
      self.assertTrue(search_engine['is_default'])
      self.SetOmniboxText('test search')
      self.OmniboxAcceptInput()
      self.assertTrue(re.search(keyword, self.GetActiveTabURL().spec()))


if __name__ == '__main__':
  pyauto_functional.Main()
