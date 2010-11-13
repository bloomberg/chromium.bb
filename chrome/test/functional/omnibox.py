#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import shutil
import tempfile

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

  def testFocusOnStartup(self):
    """Verify that omnibox has focus on startup."""
    self.WaitUntilOmniboxReadyHack()
    self.assertTrue(self.GetOmniboxInfo().Properties('has_focus'))

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

  def testHistoryResult(self):
    """Verify that omnibox can fetch items from history."""
    url = self.GetFileURLForPath(os.path.join(self.DataDir(), 'title2.html'))
    title = 'Title Of Awesomeness'
    self.AppendTab(pyauto.GURL(url))
    def _VerifyHistoryResult(query_list, description, windex=0):
      """Verify result matching given description for given list of queries."""
      for query_text in query_list:
        matches = self._GetOmniboxMatchesFor(
            query_text, windex=windex, attr_dict={'description': description})
        self.assertTrue(matches)
        self.assertEqual(1, len(matches))
        item = matches[0]
        self.assertEqual(url, item['destination_url'])
    # Query using URL & title
    _VerifyHistoryResult([url, title], title)
    # Verify results in another tab
    self.AppendTab(pyauto.GURL())
    _VerifyHistoryResult([url, title], title)
    # Verify results in another window
    self.OpenNewBrowserWindow(True)
    self.WaitUntilOmniboxReadyHack(windex=1)
    _VerifyHistoryResult([url, title], title, windex=1)
    # Verify results in an incognito window
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
    self.WaitUntilOmniboxReadyHack(windex=2)
    _VerifyHistoryResult([url, title], title, windex=2)

  def testSelect(self):
    """Verify omnibox popup selection."""
    url1 = self.GetFileURLForPath(os.path.join(self.DataDir(), 'title2.html'))
    url2 = self.GetFileURLForPath(os.path.join(self.DataDir(), 'title1.html'))
    title1 = 'Title Of Awesomeness'
    self.NavigateToURL(url1)
    self.NavigateToURL(url2)
    matches = self._GetOmniboxMatchesFor('file://')
    self.assertTrue(matches)
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

  def testGoogleSearch(self):
    """Verify Google search item in omnibox results."""
    search_text = 'hello world'
    verify_str = 'Google Search'
    url_re = 'http://www.google.com/search\?.*q=hello\+world.*'
    matches_description = self._GetOmniboxMatchesFor(
        search_text, attr_dict={'description': verify_str})
    self.assertTrue(matches_description)
    self.assertEqual(1, len(matches_description))
    item = matches_description[0]
    self.assertTrue(re.search(url_re, item['destination_url']))
    self.assertEqual('search-what-you-typed', item['type'])

  def testInlinAutoComplete(self):
    """Verify inline autocomplete for a pre-visited url."""
    self.NavigateToURL('http://www.google.com')
    matches = self._GetOmniboxMatchesFor('goog')
    self.assertTrue(matches)
    # Omnibox should suggest auto completed url as the first item
    matches_description = matches[0]
    self.assertTrue('www.google.com' in matches_description['contents'])
    self.assertEqual('history-url', matches_description['type'])
    # The url should be inline-autocompleted in the omnibox
    self.assertTrue('google.com' in self.GetOmniboxInfo().Text())

  def testCrazyFilenames(self):
    """Test omnibox query with filenames containing special chars.

    The files are created on the fly and cleaned after use.
    """
    filename = os.path.join(self.DataDir(), 'downloads', 'crazy_filenames.txt')
    zip_names = self.EvalDataFrom(filename)
    # We got .zip filenames. Change them to .html
    crazy_filenames = [x.replace('.zip', '.html') for x in zip_names]
    title = 'given title'

    def _CreateFile(name):
      """Create the given html file."""
      fp = open(name, 'w')  # name could be unicode
      print >>fp, '<html><title>%s</title><body>' % title
      print >>fp, 'This is a junk file named <h2>%s</h2>' % repr(name)
      print >>fp, '</body></html>'
      fp.close()

    crazy_fileurls = []
    # Temp dir for hosting crazy filenames.
    temp_dir = tempfile.mkdtemp(prefix='omnibox')
    # Windows has a dual nature dealing with unicode filenames.
    # While the files are internally saved as unicode, there's a non-unicode
    # aware API that returns a locale-dependent coding on the true unicode
    # filenames.  This messes up things.
    # Filesystem-interfacing functions like os.listdir() need to
    # be given unicode strings to "do the right thing" on win.
    # Ref: http://boodebr.org/main/python/all-about-python-and-unicode
    try:
      for filename in crazy_filenames:  # filename is unicode.
        file_path = os.path.join(temp_dir, filename.encode('utf-8'))
        _CreateFile(os.path.join(temp_dir, filename))
        file_url = self.GetFileURLForPath(file_path)
        crazy_fileurls.append(file_url)
        self.NavigateToURL(file_url)

      # Verify omnibox queries.
      for file_url in crazy_fileurls:
        matches = self._GetOmniboxMatchesFor(
            file_url, attr_dict={'type': 'url-what-you-typed',
                                 'description': title})
        self.assertTrue(matches)
        self.assertEqual(1, len(matches))
        self.assertTrue(os.path.basename(file_url) in
                        matches[0]['destination_url'])
    finally:
      shutil.rmtree(unicode(temp_dir))  # unicode so that win treats nicely.

  def testSuggest(self):
    """Verify suggest results in omnibox."""
    matches = self._GetOmniboxMatchesFor('apple')
    self.assertTrue(matches)
    self.assertTrue([x for x in matches if x['type'] == 'search-suggest'])

  def testDifferentTypesOfResults(self):
    """Verify different types of results from omnibox.

    This includes history result, bookmark result, suggest results.

    """
    url = 'http://www.google.com/'
    title = 'Google'
    self.AddBookmarkURL(  # Add a bookmark
        self.GetBookmarkModel().BookmarkBar()['id'], 0, title, url)
    self.NavigateToURL(url)  # Build up history
    matches = self._GetOmniboxMatchesFor('google')
    self.assertTrue(matches)
    # Verify starred result (indicating bookmarked url)
    self.assertTrue([x for x in matches if x['starred'] == True])
    for item_type in ('history-url', 'search-what-you-typed',
                      'search-suggest',):
      self.assertTrue([x for x in matches if x['type'] == item_type])

  def testSuggestPref(self):
    """Verify omnibox suggest-service enable/disable pref."""
    self.assertTrue(self.GetPrefsInfo().Prefs(pyauto.kSearchSuggestEnabled))
    matches = self._GetOmniboxMatchesFor('apple')
    self.assertTrue(matches)
    self.assertTrue([x for x in matches if x['type'] == 'search-suggest'])
    # Disable suggest-service
    self.SetPrefs(pyauto.kSearchSuggestEnabled, False)
    self.assertFalse(self.GetPrefsInfo().Prefs(pyauto.kSearchSuggestEnabled))
    matches = self._GetOmniboxMatchesFor('apple')
    self.assertTrue(matches)
    # Verify there are no suggest results
    self.assertFalse([x for x in matches if x['type'] == 'search-suggest'])


if __name__ == '__main__':
  pyauto_functional.Main()
