#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
import test_utils


class OmniboxTest(pyauto.PyUITest):
  """Test cases for the omnibox."""

  def Debug(self):
    """Test method for experimentation.

    This method will not run automatically.
    """
    import time
    while True:
      self.pprint(self.GetOmniboxInfo().omniboxdict)
      time.sleep(1)

  def testFocusOnStartup(self):
    """Verify that the omnibox has focus on startup."""
    self.WaitUntilOmniboxReadyHack()
    self.assertTrue(self.GetOmniboxInfo().Properties('has_focus'))

  def testHistoryResult(self):
    """Verify that the omnibox can fetch items from the history."""
    url = self.GetFileURLForDataPath('title2.html')
    title = 'Title Of Awesomeness'
    self.AppendTab(pyauto.GURL(url))
    def _VerifyHistoryResult(query_list, description, windex=0):
      """Verify result matching given description for given list of queries."""
      for query_text in query_list:
        matches = test_utils.GetOmniboxMatchesFor(
            self, query_text, windex=windex,
            attr_dict={'description': description})
        self.assertTrue(matches)
        self.assertEqual(1, len(matches))
        item = matches[0]
        self.assertEqual(url, item['destination_url'])
    # Query using URL & title.
    _VerifyHistoryResult([url, title], title)
    # Verify results in another tab.
    self.AppendTab(pyauto.GURL())
    _VerifyHistoryResult([url, title], title)
    # Verify results in another window.
    self.OpenNewBrowserWindow(True)
    self.WaitUntilOmniboxReadyHack(windex=1)
    _VerifyHistoryResult([url, title], title, windex=1)
    # Verify results in an incognito window.
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
    self.WaitUntilOmniboxReadyHack(windex=2)
    _VerifyHistoryResult([url, title], title, windex=2)

  def _VerifyOmniboxURLMatches(self, url, description, windex=0):
    """Verify URL match results from the omnibox.

    Args:
      url: The URL to use.
      description: The string description within the history page and Google
                   search to match against.
      windex: The window index to work on. Defaults to 0 (first window).
    """
    matches_description = test_utils.GetOmniboxMatchesFor(
        self, url, windex=windex, attr_dict={'description': description})
    self.assertEqual(1, len(matches_description))
    if description == 'Google Search':
      self.assertTrue(re.match('http://www.google.com/search.+',
                               matches_description[0]['destination_url']))
    else:
      self.assertEqual(url, matches_description[0]['destination_url'])

  def testFetchHistoryResultItems(self):
    """Verify omnibox fetches history items in 2nd tab, window and incognito."""
    url = self.GetFileURLForDataPath('title2.html')
    title = 'Title Of Awesomeness'
    desc = 'Google Search'
    # Fetch history page item in the second tab.
    self.AppendTab(pyauto.GURL(url))
    self._VerifyOmniboxURLMatches(url, title)
    # Fetch history page items in the second window.
    self.OpenNewBrowserWindow(True)
    self.NavigateToURL(url, 1, 0)
    self._VerifyOmniboxURLMatches(url, title, windex=1)
    # Fetch google search items in incognito window.
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
    matches = test_utils.GetOmniboxMatchesFor(self, 'file://')
    self.assertTrue(matches)
    # Find the index of match for |url1|.
    index = None
    for i, match in enumerate(matches):
      if match['description'] == title1:
        index = i
    self.assertTrue(index is not None)
    self.OmniboxMovePopupSelection(index)  # Select |url1| line in popup.
    self.assertEqual(url1, self.GetOmniboxInfo().Text())
    self.OmniboxAcceptInput()
    self.assertEqual(title1, self.GetActiveTabTitle())

  def testGoogleSearch(self):
    """Verify Google search item in omnibox results."""
    search_text = 'hello world'
    verify_str = 'Google Search'
    url_re = 'http://www.google.com/search\?.*q=hello\+world.*'
    matches_description = test_utils.GetOmniboxMatchesFor(
        self, search_text, attr_dict={'description': verify_str})
    self.assertTrue(matches_description)
    # There should be a least one entry with the description Google. Suggest
    # results may end up having 'Google Search' in them, so use >=.
    self.assertTrue(len(matches_description) >= 1)
    item = matches_description[0]
    self.assertTrue(re.search(url_re, item['destination_url']))
    self.assertEqual('search-what-you-typed', item['type'])

  def testInlineAutoComplete(self):
    """Verify inline autocomplete for a pre-visited URL."""
    self.NavigateToURL('http://www.google.com')
    matches = test_utils.GetOmniboxMatchesFor(self, 'goog')
    self.assertTrue(matches)
    # Omnibox should suggest auto completed URL as the first item.
    matches_description = matches[0]
    self.assertTrue('www.google.com' in matches_description['contents'])
    self.assertEqual('history-url', matches_description['type'])
    # The URL should be inline-autocompleted in the omnibox.
    self.assertTrue('google.com' in self.GetOmniboxInfo().Text())

  def testCrazyFilenames(self):
    """Test omnibox query with filenames containing special chars.

    The files are created on the fly and cleaned after use.
    """
    filename = os.path.join(self.DataDir(), 'downloads', 'crazy_filenames.txt')
    zip_names = self.EvalDataFrom(filename)
    # We got .zip filenames. Change them to .html.
    crazy_filenames = [x.replace('.zip', '.html') for x in zip_names]
    title = 'given title'

    def _CreateFile(name):
      """Create the given html file."""
      fp = open(name, 'w')  # |name| could be unicode.
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
      for filename in crazy_filenames:  # |filename| is unicode.
        file_path = os.path.join(temp_dir, filename.encode('utf-8'))
        _CreateFile(os.path.join(temp_dir, filename))
        file_url = self.GetFileURLForPath(file_path)
        crazy_fileurls.append(file_url)
        self.NavigateToURL(file_url)

      # Verify omnibox queries.
      for file_url in crazy_fileurls:
        matches = test_utils.GetOmniboxMatchesFor(self,
            file_url, attr_dict={'type': 'url-what-you-typed',
                                 'description': title})
        self.assertTrue(matches)
        self.assertEqual(1, len(matches))
        self.assertTrue(os.path.basename(file_url) in
                        matches[0]['destination_url'])
    finally:
      shutil.rmtree(unicode(temp_dir))  # Unicode so that Win treats nicely.

  def testSuggest(self):
    """Verify suggested results in omnibox."""
    matches = test_utils.GetOmniboxMatchesFor(self, 'apple')
    self.assertTrue(matches)
    self.assertTrue([x for x in matches if x['type'] == 'search-suggest'])

  def testDifferentTypesOfResults(self):
    """Verify different types of results from omnibox.

    This includes history result, bookmark result, suggest results.
    """
    url = 'http://www.google.com/'
    title = 'Google'
    search_string = 'google'
    self.AddBookmarkURL(  # Add a bookmark.
        self.GetBookmarkModel().BookmarkBar()['id'], 0, title, url)
    self.NavigateToURL(url)  # Build up history.
    matches = test_utils.GetOmniboxMatchesFor(self, search_string)
    self.assertTrue(matches)
    # Verify starred result (indicating bookmarked url).
    self.assertTrue([x for x in matches if x['starred'] == True])
    for item_type in ('history-url', 'search-what-you-typed',
                      'search-suggest',):
      self.assertTrue([x for x in matches if x['type'] == item_type])

  def testSuggestPref(self):
    """Verify no suggests for omnibox when suggested-services disabled."""
    search_string = 'apple'
    self.assertTrue(self.GetPrefsInfo().Prefs(pyauto.kSearchSuggestEnabled))
    matches = test_utils.GetOmniboxMatchesFor(self, search_string)
    self.assertTrue(matches)
    self.assertTrue([x for x in matches if x['type'] == 'search-suggest'])
    # Disable suggest-service.
    self.SetPrefs(pyauto.kSearchSuggestEnabled, False)
    self.assertFalse(self.GetPrefsInfo().Prefs(pyauto.kSearchSuggestEnabled))
    matches = test_utils.GetOmniboxMatchesFor(self, search_string)
    self.assertTrue(matches)
    # Verify there are no suggest results.
    self.assertFalse([x for x in matches if x['type'] == 'search-suggest'])

  def testAutoCompleteForSearch(self):
    """Verify omnibox autocomplete for search."""
    search_string = 'youtu'
    verify_string = 'youtube'
    matches = test_utils.GetOmniboxMatchesFor(self, search_string)
    # Retrieve last contents element.
    matches_description = matches[-1]['contents'].split()
    self.assertEqual(verify_string, matches_description[0])

  def _GotContentHistory(self, search_text, url):
    """Check if omnibox returns a previously-visited page for given search text.

    Args:
      search_text: The string search text.
      url: The string URL to look for in the omnibox matches.

    Returns:
      True, if the omnibox returns the previously-visited page for the given
      search text, or False otherwise.
    """
    # Omnibox doesn't change results if searching the same text repeatedly.
    # So setting '' in omnibox before the next repeated search.
    self.SetOmniboxText('')
    matches = test_utils.GetOmniboxMatchesFor(self, search_text)
    matches_description = [x for x in matches if x['destination_url'] == url]
    return 1 == len(matches_description)

  def testContentHistory(self):
    """Verify omnibox results when entering page content.

    Test verifies that visited page shows up in omnibox on entering page
    content.
    """
    url = self.GetFileURLForPath(
        os.path.join(self.DataDir(), 'find_in_page', 'largepage.html'))
    self.NavigateToURL(url)
    self.assertTrue(self.WaitUntil(
        lambda: self._GotContentHistory('British throne', url)))

  def testOmniboxSearchHistory(self):
    """Verify page navigation/search from omnibox are added to the history."""
    url = self.GetFileURLForDataPath('title2.html')
    self.NavigateToURL(url)
    self.AppendTab(pyauto.GURL('about:blank'))
    self.SetOmniboxText('java')
    self.WaitUntilOmniboxQueryDone()
    self.OmniboxAcceptInput()
    history = self.GetHistoryInfo().History()
    self.assertEqual(2, len(history))
    self.assertEqual(url, history[1]['url'])
    self.assertEqual('java - Google Search', history[0]['title'])

  def _VerifyHasBookmarkResult(self, matches):
    """Verify that we have a bookmark result.

    Args:
      matches: A list of match items, as returned by
               test_utils.GetOmniboxMatchesFor().
    """
    matches_starred = [result for result in matches if result['starred']]
    self.assertTrue(matches_starred)
    self.assertEqual(1, len(matches_starred))

  def _CheckBookmarkResultForVariousInputs(self, url, title, windex=0):
    """Check if we get the bookmark for complete and partial inputs.

    Args:
      url: A string URL.
      title: A string title for the given URL.
      windex: The window index to use.  Defaults to 0 (first window).
    """
    # Check if the complete URL would get the bookmark.
    url_matches = test_utils.GetOmniboxMatchesFor(self, url, windex=windex)
    self._VerifyHasBookmarkResult(url_matches)
    # Check if the complete title would get the bookmark.
    title_matches = test_utils.GetOmniboxMatchesFor(self, title, windex=windex)
    self._VerifyHasBookmarkResult(title_matches)
    # Check if the partial URL would get the bookmark.
    split_url = urlparse.urlsplit(url)
    partial_url = test_utils.GetOmniboxMatchesFor(
        self, split_url.scheme, windex=windex)
    self._VerifyHasBookmarkResult(partial_url)
    # Check if the partial title would get the bookmark.
    split_title = title.split()
    search_term = split_title[len(split_title) - 1]
    partial_title = test_utils.GetOmniboxMatchesFor(
        self, search_term, windex=windex)
    self._VerifyHasBookmarkResult(partial_title)

  def testBookmarkResultInNewTabAndWindow(self):
    """Verify omnibox finds bookmarks in search options of new tabs/windows."""
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

  def testAutoCompleteForNonAsciiSearch(self):
    """Verify can search/autocomplete with non-ASCII incomplete keywords."""
    search_string = u'\u767e'
    verify_string = u'\u767e\u5ea6\u4e00\u4e0b'
    matches = test_utils.GetOmniboxMatchesFor(self, search_string)
    self.assertTrue(verify_string in matches[-1]['contents'])

  def _InstallAndVerifyApp(self, app_name):
    """Installs a sample packaged app and verifies the install is successful.

    Args:
      app_name: The name of the app to be installed.

    Returns:
      The string ID of the installed app.
    """
    app_crx_file = os.path.abspath(os.path.join(self.DataDir(),
                                   'pyauto_private', 'apps', app_name))
    return self.InstallExtension(app_crx_file)

  def _VerifyOminiboxMatches(self, search_str, app_url, is_incognito):
    """Verify app matches in omnibox.

    Args:
      search_str: Search keyword to find app matches.
      app_url: Chrome app launch URL.
      is_incognito: A boolean indicating if omnibox matches are from an
                    incognito window.
    """
    def OminiboxMatchFound():
      if is_incognito:
        matches = test_utils.GetOmniboxMatchesFor(self, search_str, windex=2)
      else:
        matches = test_utils.GetOmniboxMatchesFor(self, search_str)
      for x in matches:
        if x['destination_url'] == app_url:
          return True
      return False
    self.assertTrue(self.WaitUntil(OminiboxMatchFound),
                    msg='Timed out waiting to verify omnibox matches.')

  def _VerifyAppSearchCurrentWindow(self, app_name, search_str, app_url):
    """Verify app can be searched and launched in omnibox of current window.

    Args:
      app_name: The name of an installed app.
      search_str: The search string to find the app from auto suggest.
      app_url: Chrome app launch URL.
    """
    # Assume current window is at index 0.
    self._InstallAndVerifyApp(app_name)
    # Get app match in omnibox of the current window.
    self._VerifyOminiboxMatches(search_str, app_url, False)
    # Launch app from omnibox and verify it succeeds.
    self.OmniboxAcceptInput() # Blocks until the page loads.
    self.assertTrue(re.search(app_url, self.GetActiveTabURL().spec()))

  def _VerifyAppSearchIncognitoWindow(self, app_name, search_str, app_url):
    """Verify app can be searched and launched in omnibox of incognito window.

    Args:
      app_name: The name of installed app.
      search_str: The search string to find the app from auto suggest.
      app_url: Chrome app launch URL.
    """
    # Assume current window is at index 0.
    self._InstallAndVerifyApp(app_name)

    # Get app matches in omnibox of the 3rd (incognito) window, assume
    # pre-exist 1st window. We have to launch 2 incognito windows here,
    # since there is an issue (crbug.com/99925) with app search in pyauto
    # launched 1st incognito window on Mac.
    # TODO(vivianz@): remove additional incognito window after issue 99925 is
    # fixed.
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
    self.WaitUntilOmniboxReadyHack(windex=1)
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
    self.WaitUntilOmniboxReadyHack(windex=2)
    # Get app match in omnibox of 3rd (incognito) window.
    self._VerifyOminiboxMatches(search_str, app_url, True)
    # Launch app from omnibox and verify it succeeds.
    self.OmniboxAcceptInput(windex=2) # Blocks until the page loads.
    self.assertTrue(re.search(app_url, self.GetActiveTabURL(2).spec()))

  def _VerifyAppSearchNewTab(self, app_name, search_str, app_url):
    """Verify app can be searched in new tab.

    Args:
      app_name: The name of installed app.
      search_str: The search string to find the app from auto suggest.
      app_url: Chrome app launch URL.
    """
    self._InstallAndVerifyApp(app_name)
    # Get app matches in omnibox of the 2nd tab.
    self.AppendTab(pyauto.GURL())
    self._VerifyOminiboxMatches(search_str, app_url, False)

  def testBasicAppSearch(self):
    """Verify that we can search for installed apps."""
    app_name = 'countdown.crx'
    search_str = 'countdown'
    app_url = 'chrome-extension:' \
              '//aeabikdlfbfeihglecobdkdflahfgcpd/launchLocalPath.html'
    self._VerifyAppSearchCurrentWindow(app_name, search_str, app_url)

  def testAppNameWithSpaceSearch(self):
    """Verify that we can search for apps with space in app name."""
    app_name = 'cargo_bridge.crx'
    search_str = 'Cargo Bridge'
    app_url = 'http://webstore.limexgames.com/cargo_bridge'
    self._VerifyAppSearchCurrentWindow(app_name, search_str, app_url)

  def testAppNameWithNumberSearch(self):
    """Verify that we can search for apps with number in app name."""
    app_name = '3d_stunt_pilot.crx'
    search_str = '3D Stunt Pilot'
    app_url = 'chrome-extension://cjhglkpjpcechghgbkdfoofgbphpgjde/index.html'
    self._VerifyAppSearchCurrentWindow(app_name, search_str, app_url)

  def testAppComboNameWithSpecialCharSearch(self):
    """Verify launch special character named app in regular window."""
    app_name = 'atari_lunar_lander.crx'
    search_str = 'Atari - Lunar Lander'
    app_url = 'http://chrome.atari.com/lunarlander/'
    self._VerifyAppSearchCurrentWindow(app_name, search_str, app_url)

  def testAppSearchWithVeryLongAppName(self):
    """Verify that we can search app with very long app name."""
    app_name = '20_thing_I_learn_from_browser_and_web.crx'
    search_str = '20 Things I Learned About Browsers & the Web'
    app_url = 'http://www.20thingsilearned.com/'
    self._VerifyAppSearchCurrentWindow(app_name, search_str, app_url)

  def testIncognitoAppNameWithSpaceSearch(self):
    """Verify search for apps with space in app name in incognito window."""
    app_name = 'cargo_bridge.crx'
    search_str = 'Cargo Bridge'
    app_url = 'http://webstore.limexgames.com/cargo_bridge'
    self._VerifyAppSearchIncognitoWindow(app_name, search_str, app_url)

  def testIncognitoAppComboNameWithSpecialCharSearch(self):
    """Verify launch special character named app in incognito window."""
    app_name = 'atari_lunar_lander.crx'
    search_str = 'Atari - Lunar Lander'
    app_url = 'http://chrome.atari.com/lunarlander/'
    self._VerifyAppSearchIncognitoWindow(app_name, search_str, app_url)

  def testIncognitoAppSearchWithVeryLongAppName(self):
    """Verify that we can launch app with very long app name."""
    app_name = '20_thing_I_learn_from_browser_and_web.crx'
    search_str = '20 Things I Learned About Browsers & the Web'
    app_url = 'http://www.20thingsilearned.com/'
    self._VerifyAppSearchIncognitoWindow(app_name, search_str, app_url)

  def testRepeatedlyAppLaunchInTabs(self):
    """Verify that we can repeatedly launch app in tabs."""
    app_name = '3d_stunt_pilot.crx'
    search_str = '3D Stunt Pilot'
    app_url = 'chrome-extension://cjhglkpjpcechghgbkdfoofgbphpgjde/index.html'
    self._VerifyAppSearchNewTab(app_name, search_str, app_url)
    # Launch app twice and verify it succeeds.
    self.OmniboxAcceptInput()
    self.assertTrue(re.search(app_url, self.GetActiveTabURL().spec()))
    self.AppendTab(pyauto.GURL())
    self.SetOmniboxText(search_str)
    self.OmniboxAcceptInput()
    self.assertTrue(re.search(app_url, self.GetActiveTabURL().spec()))

  def testEndPartAppNameSearchInNewTab(self):
    """Verify that we can search app with partial app name (end part)."""
    app_name = 'countdown.crx'
    search_str = 'own'
    app_url = 'chrome-extension:' \
              '//aeabikdlfbfeihglecobdkdflahfgcpd/launchLocalPath.html'
    self._VerifyAppSearchNewTab(app_name, search_str, app_url)

  def testBeginningPartAppNameSearchInNewTab(self):
    """Verify that we can search app with partial app name (beginning part)."""
    app_name = 'cargo_bridge.crx'
    search_str = 'Car'
    app_url = 'http://webstore.limexgames.com/cargo_bridge'
    self._VerifyAppSearchNewTab(app_name, search_str, app_url)


if __name__ == '__main__':
  pyauto_functional.Main()
