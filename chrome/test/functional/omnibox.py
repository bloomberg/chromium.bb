#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import glob
import os
import re
import shutil
import tempfile
import urlparse

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
    url = self.GetFileURLForDataPath('title2.html')
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

  def _VerifyOmniboxURLMatches(self, url, description, windex=0):
    """Verify URL match results from the Omnibox.

    Args:
      url: the url to use
      description: the string description within history page and google search
                   to match against
      windex: the window index to work on. Defaults to 0 (first window)
    """
    matches_description = self._GetOmniboxMatchesFor(
        url, windex=windex, attr_dict={'description': description})
    self.assertEqual(1, len(matches_description))
    if description == 'Google Search':
      self.assertTrue(re.match('http://www.google.com/search.+',
                               matches_description[0]['destination_url']))
    else:
      self.assertEqual(url, matches_description[0]['destination_url'])

  def testFetchHistoryResultItems(self):
    """Verify omnibox fetches history items in second tab, win and Incognito."""
    url = self.GetFileURLForDataPath('title2.html')
    title = 'Title Of Awesomeness'
    desc = 'Google Search'
    # fetch history page item in the second tab.
    self.AppendTab(pyauto.GURL(url))
    self._VerifyOmniboxURLMatches(url, title)
    # fetch history page items in the second window.
    self.OpenNewBrowserWindow(True)
    self.NavigateToURL(url, 1, 0)
    self._VerifyOmniboxURLMatches(url, title, windex=1)
    # fetch google search items in Incognito window.
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
    self.NavigateToURL(url, 2, 0)
    self._VerifyOmniboxURLMatches(url, desc, windex=2)

  def testSelect(self):
    """Verify omnibox popup selection."""
    url1 = self.GetFileURLForDataPath('title2.html')
    url2 = self.GetFileURLForDataPath('title1.html')
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
    """Verify suggested results in omnibox."""
    matches = self._GetOmniboxMatchesFor('apple')
    self.assertTrue(matches)
    self.assertTrue([x for x in matches if x['type'] == 'search-suggest'])

  def testDifferentTypesOfResults(self):
    """Verify different types of results from omnibox.

    This includes history result, bookmark result, suggest results.
    """
    url = 'http://www.google.com/'
    title = 'Google'
    search_string = 'google'
    self.AddBookmarkURL(  # Add a bookmark
        self.GetBookmarkModel().BookmarkBar()['id'], 0, title, url)
    self.NavigateToURL(url)  # Build up history
    matches = self._GetOmniboxMatchesFor(search_string)
    self.assertTrue(matches)
    # Verify starred result (indicating bookmarked url)
    self.assertTrue([x for x in matches if x['starred'] == True])
    for item_type in ('history-url', 'search-what-you-typed',
                      'search-suggest',):
      self.assertTrue([x for x in matches if x['type'] == item_type])

  def testSuggestPref(self):
    """Verify no suggests for omnibox when suggested-services disabled."""
    search_string = 'apple'
    self.assertTrue(self.GetPrefsInfo().Prefs(pyauto.kSearchSuggestEnabled))
    matches = self._GetOmniboxMatchesFor(search_string)
    self.assertTrue(matches)
    self.assertTrue([x for x in matches if x['type'] == 'search-suggest'])
    # Disable suggest-service
    self.SetPrefs(pyauto.kSearchSuggestEnabled, False)
    self.assertFalse(self.GetPrefsInfo().Prefs(pyauto.kSearchSuggestEnabled))
    matches = self._GetOmniboxMatchesFor(search_string)
    self.assertTrue(matches)
    # Verify there are no suggest results
    self.assertFalse([x for x in matches if x['type'] == 'search-suggest'])

  def testAutoCompleteForSearch(self):
    """Verify omnibox autocomplete for search."""
    search_string = 'barac'
    verify_string = 'barack'
    matches = self._GetOmniboxMatchesFor(search_string)
    # retrieve last contents element.
    matches_description = matches[-1]['contents'].split()
    self.assertEqual(verify_string, matches_description[0])

  def _CheckBookmarkResultForVariousInputs(self, url, title, windex=0):
    """Check if we get the Bookmark for complete and partial inputs."""
    # Check if the complete URL would get the bookmark.
    url_matches = self._GetOmniboxMatchesFor(url, windex=windex)
    self._VerifyHasBookmarkResult(url_matches)
    # Check if the complete title would get the bookmark.
    title_matches = self._GetOmniboxMatchesFor(title, windex=windex)
    self._VerifyHasBookmarkResult(title_matches)
    # Check if the partial URL would get the bookmark.
    split_url = urlparse.urlsplit(url)
    partial_url = self._GetOmniboxMatchesFor(split_url.scheme, windex=windex)
    self._VerifyHasBookmarkResult(partial_url)
    # Check if the partial title would get the bookmark.
    split_title = title.split()
    search_term = split_title[len(split_title) - 1]
    partial_title = self._GetOmniboxMatchesFor(search_term, windex=windex)
    self._VerifyHasBookmarkResult(partial_title)

  def _GotContentHistory(self, search_text, url):
    """Determines if omnibox returns a previously visited page for given
       search text
    """
    # Omnibox doesn't change results if searching the same text repeatedly.
    # So setting '' in omnibox before the next repeated search.
    self.SetOmniboxText('')
    matches = self._GetOmniboxMatchesFor(search_text)
    matches_description = [x for x in matches if x['destination_url'] == url]
    return 1 == len(matches_description)

  def testContentHistory(self):
    """Verify omnibox results when entering page content

    Test verifies that visited page shows up in omnibox on entering page
    content.
    """
    url = self.GetFileURLForPath(
        os.path.join(self.DataDir(), 'find_in_page', 'largepage.html'))
    self.NavigateToURL(url)
    self.assertTrue(self.WaitUntil(
        lambda: self._GotContentHistory('British throne', url)))

  def _VerifyHasBookmarkResult(self, matches):
    """Verify that we have a bookmark result."""
    matches_starred = [result for result in matches if result['starred']]
    self.assertTrue(matches_starred)
    self.assertEqual(1, len(matches_starred))

  def testBookmarkResultInNewTabAndWindow(self):
    """Verify that omnibox can recognize a bookmark within search options
    in new tabs and windows."""
    url = self.GetFileURLForDataPath('title2.html')
    self.NavigateToURL(url)
    title = 'This is Awesomeness'
    bookmarks = self.GetBookmarkModel()
    bar_id = bookmarks.BookmarkBar()['id']
    self.AddBookmarkURL(bar_id, 0, title, url)
    bookmarks = self.GetBookmarkModel()
    nodes = bookmarks.FindByTitle(title)
    self.AppendTab(pyauto.GURL(url))
    self._CheckBookmarkResultForVariousInputs(url, title)
    self.OpenNewBrowserWindow(True)
    self.assertEqual(2, self.GetBrowserWindowCount())
    self.NavigateToURL(url, 1, 0)
    self._CheckBookmarkResultForVariousInputs(url, title, windex=1)
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
    self.assertEqual(3, self.GetBrowserWindowCount())
    self.NavigateToURL(url, 2, 0)
    self._CheckBookmarkResultForVariousInputs(url, title, windex=2)


if __name__ == '__main__':
  pyauto_functional.Main()
