#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import time

import pyauto_functional  # Must be imported before pyauto
import pyauto
import test_utils


class HistoryTest(pyauto.PyUITest):
  """TestCase for History."""

  def testBasic(self):
    url = self.GetFileURLForDataPath('title2.html')
    title = 'Title Of Awesomeness'
    self.NavigateToURL(url)

    history = self.GetHistoryInfo().History()
    self.assertEqual(1, len(history))
    self.assertEqual(title, history[0]['title'])
    self.assertEqual(url, history[0]['url'])

  def Debug(self):
    """Test method for experimentation.

    This method will not run automatically.
    """
    while True:
      raw_input('Interact with the browser and hit <enter> to dump history.. ')
      print '*' * 20
      self.pprint(self.GetHistoryInfo().History())

  def testHistoryPersists(self):
    """Verify that history persists after session restart."""
    assert not self.GetHistoryInfo().History(), 'Expecting clean history.'
    url = self.GetFileURLForDataPath('title2.html')
    title = 'Title Of Awesomeness'
    self.NavigateToURL(url)
    history = self.GetHistoryInfo().History()
    self.assertEqual(1, len(history))
    self.assertEqual(title, history[0]['title'])
    self.assertEqual(url, history[0]['url'])
    self.RestartBrowser(clear_profile=False)
    # Verify that history persists.
    history = self.GetHistoryInfo().History()
    self.assertEqual(1, len(history))
    self.assertEqual(title, history[0]['title'])
    self.assertEqual(url, history[0]['url'])

  def testInvalidURLNoHistory(self):
    """Invalid URLs should not go in history."""
    assert not self.GetHistoryInfo().History(), 'Expecting clean history.'
    urls = [ self.GetFileURLForPath('some_non-existing_path'),
             self.GetFileURLForPath('another_non-existing_path'),
           ]
    for url in urls:
      if not url.startswith('file://'):
        logging.warn('Using %s. Might depend on how dns failures are handled'
                     'on the network' % url)
      self.NavigateToURL(url)
    self.assertEqual(0, len(self.GetHistoryInfo().History()))

  def testNewTabNoHistory(self):
    """New tab page - chrome://newtab/ should not show up in history."""
    assert not self.GetHistoryInfo().History(), 'Expecting clean history.'
    self.AppendTab(pyauto.GURL('chrome://newtab/'))
    self.assertEqual(0, len(self.GetHistoryInfo().History()))

  def testIncognitoNoHistory(self):
    """Incognito browsing should not show up in history."""
    assert not self.GetHistoryInfo().History(), 'Expecting clean history.'
    url = self.GetFileURLForDataPath('title2.html')
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
    self.NavigateToURL(url, 1, 0)
    self.assertEqual(0, len(self.GetHistoryInfo().History()))

  def testStarredBookmarkInHistory(self):
    """Verify "starred" URLs in history."""
    url = self.GetFileURLForDataPath('title2.html')
    title = 'Title Of Awesomeness'
    self.NavigateToURL(url)

    # Should not be starred in history yet.
    history = self.GetHistoryInfo().History()
    self.assertEqual(1, len(history))
    self.assertFalse(history[0]['starred'])

    # Bookmark the URL.
    bookmarks = self.GetBookmarkModel()
    bar_id = bookmarks.BookmarkBar()['id']
    self.AddBookmarkURL(bar_id, 0, title, url)

    # Should be starred now.
    history = self.GetHistoryInfo().History()
    self.assertEqual(1, len(history))
    self.assertTrue(history[0]['starred'])

    # Remove bookmark.
    bookmarks = self.GetBookmarkModel()
    node = bookmarks.FindByTitle(title)
    self.assertTrue(node)
    id = node[0]['id']
    self.RemoveBookmark(id)

    # Should not be starred anymore.
    history = self.GetHistoryInfo().History()
    self.assertEqual(1, len(history))
    self.assertFalse(history[0]['starred'])

  def testNavigateMultiTimes(self):
    """Multiple navigations to the same url should have a single history."""
    assert not self.GetHistoryInfo().History(), 'Expecting clean history.'
    url = self.GetFileURLForDataPath('title2.html')
    for i in range(5):
      self.NavigateToURL(url)
    self.assertEqual(1, len(self.GetHistoryInfo().History()))

  def testMultiTabsWindowsHistory(self):
    """Verify history with multiple windows and tabs."""
    assert not self.GetHistoryInfo().History(), 'Expecting clean history.'
    urls = []
    for name in ['title2.html', 'title1.html', 'title3.html', 'simple.html']:
       urls.append(self.GetFileURLForDataPath(name))
    num_urls = len(urls)
    assert num_urls == 4, 'Need 4 urls'

    self.NavigateToURL(urls[0], 0, 0)        # window 0, tab 0
    self.OpenNewBrowserWindow(True)
    self.AppendTab(pyauto.GURL(urls[1]), 0)  # window 0, tab 1
    self.AppendTab(pyauto.GURL(urls[2]), 1)  # window 1
    self.AppendTab(pyauto.GURL(urls[3]), 1)  # window 1

    history = self.GetHistoryInfo().History()
    self.assertEqual(num_urls, len(history))
    # The history should be ordered most recent first.
    for i in range(num_urls):
      self.assertEqual(urls[-1 - i], history[i]['url'])

  def testDownloadNoHistory(self):
    """Downloaded URLs should not show up in history."""
    zip_file = 'a_zip_file.zip'
    assert not self.GetHistoryInfo().History(), 'Expecting clean history.'
    test_utils.DownloadFileFromDownloadsDataDir(self, zip_file)
    test_utils.RemoveDownloadedTestFile(self, zip_file)
    # We shouldn't have any history
    history = self.GetHistoryInfo().History()
    self.assertEqual(0, len(history))

  def testRedirectHistory(self):
    """HTTP meta-refresh redirects should have separate history entries."""
    assert not self.GetHistoryInfo().History(), 'Expecting clean history.'
    file_url = self.GetFileURLForDataPath('History', 'redirector.html')
    landing_url = self.GetFileURLForDataPath('History', 'landing.html')
    tab = self.GetBrowserWindow(0).GetTab(0)
    tab.NavigateToURLBlockUntilNavigationsComplete(pyauto.GURL(file_url), 2)
    self.assertEqual(landing_url, self.GetActiveTabURL().spec())
    # We should have two history items
    history = self.GetHistoryInfo().History()
    self.assertEqual(2, len(history))
    self.assertEqual(landing_url, history[0]['url'])

  def testForge(self):
    """Brief test of forging history items.

    Note the history system can tweak values (e.g. lower-case a URL or
    append an '/' on it) so be careful with exact comparison.
    """
    assert not self.GetHistoryInfo().History(), 'Expecting clean history.'
    # Minimal interface
    self.AddHistoryItem({'url': 'http://ZOINKS'})
    history = self.GetHistoryInfo().History()
    self.assertEqual(1, len(history))
    self.assertTrue('zoinks' in history[0]['url'])  # yes it gets lower-cased.
    # Python's time might be slightly off (~10 ms) from Chrome's time (on win).
    # time.time() on win counts in 1ms steps whereas it's 1us on linux.
    # So give the new history item some time separation, so that we can rely
    # on the history ordering.
    def _GetTimeLaterThan(tm):
      y = time.time()
      if y - tm < 0.5:  # 0.5s should be an acceptable separation
        return 0.5 + y
    new_time = _GetTimeLaterThan(history[0]['time'])
    # Full interface (specify both title and url)
    self.AddHistoryItem({'title': 'Google',
                         'url': 'http://www.google.com',
                         'time': new_time})
    # Expect a second item
    history = self.GetHistoryInfo().History()
    self.assertEqual(2, len(history))
    # And make sure our forged item is there.
    self.assertEqual('Google', history[0]['title'])
    self.assertTrue('google.com' in history[0]['url'])
    self.assertTrue(abs(new_time - history[0]['time']) < 1.0)

  def testHttpsHistory(self):
    """Verify a site using https protocol shows up within history."""
    https_url = 'https://encrypted.google.com/'
    url_title = 'Google'
    self.NavigateToURL(https_url)
    history = self.GetHistoryInfo().History()
    self.assertEqual(len(history), 1)
    self.assertEqual(url_title, history[0]['title'])
    self.assertEqual(https_url, history[0]['url'])

  def testFtpHistory(self):
    """Verify a site using ftp protocol shows up within history."""
    ftp_server = self.StartFTPServer(os.path.join('chrome', 'test', 'data'))
    ftp_title = 'A Small Hello'
    ftp_url = self.GetFtpURLForDataPath(ftp_server, 'History', 'landing.html')
    self.NavigateToURL(ftp_url)
    history = self.GetHistoryInfo().History()
    self.assertEqual(len(history), 1)
    self.assertEqual(ftp_title, history[0]['title'])
    self.StopFTPServer(ftp_server)

  def _CheckHistory(self, title, url, length, index=0):
    """Verify that the current history matches expectations.

    Verify that history item has the given title and url
    and that length of history list is as expected.

    Args:
      title: Expected title of given web page.
      url: Expected address of given web page.
      length: Expected length of history list.
      index: Position of item we want to check in history list.
    """
    history = self.GetHistoryInfo().History()
    self.assertEqual(
        length, len(history),
        msg='History length: expected = %d, actual = %d.'
            % (length, len(history)))
    self.assertEqual(
        title, history[index]['title'],
        msg='Title: expected = %s, actual = %s.'
            % (title,  history[index]['title']))
    self.assertEqual(
        url, history[index]['url'], msg='URL: expected = %s, actual = %s.'
            % (url,  history[index]['url']))

  def _NavigateAndCheckHistory(self, title, page, length):
    """Navigate to a page, then verify the history.

    Args:
      title: Title of given web page.
      page: Filename of given web page.
      length: Length of history list.
    """
    url = self.GetFileURLForDataPath(page)
    self.NavigateToURL(url)
    self._CheckHistory(title, url, length)

  def testNavigateBringPageToTop(self):
    """Verify that navigation brings current page to top of history list."""
    self._NavigateAndCheckHistory('Title Of Awesomeness', 'title2.html', 1)
    self._NavigateAndCheckHistory('Title Of More Awesomeness', 'title3.html',
                                  2)

  def testReloadBringPageToTop(self):
    """Verify that reloading a page brings it to top of history list."""
    url1 = self.GetFileURLForDataPath('title2.html')
    title1 = 'Title Of Awesomeness'
    self._NavigateAndCheckHistory(title1, 'title2.html', 1)

    url2 = self.GetFileURLForDataPath('title3.html')
    title2 = 'Title Of More Awesomeness'
    self.AppendTab(pyauto.GURL(url2))
    self._CheckHistory(title2, url2, 2)

    self.ActivateTab(0)
    self.ReloadActiveTab()
    self._CheckHistory(title1, url1, 2)

  def testBackForwardBringPageToTop(self):
    """Verify that back/forward brings current page to top of history list."""
    url1 = self.GetFileURLForDataPath('title2.html')
    title1 = 'Title Of Awesomeness'
    self._NavigateAndCheckHistory(title1, 'title2.html', 1)

    url2 = self.GetFileURLForDataPath('title3.html')
    title2 = 'Title Of More Awesomeness'
    self._NavigateAndCheckHistory(title2, 'title3.html', 2)

    tab = self.GetBrowserWindow(0).GetTab(0)
    tab.GoBack()
    self._CheckHistory(title1, url1, 2)
    tab.GoForward()
    self._CheckHistory(title2, url2, 2)

  def testAppendTabAddPage(self):
    """Verify that opening a new tab adds that page to history."""
    self._NavigateAndCheckHistory('Title Of Awesomeness', 'title2.html', 1)

    url2 = self.GetFileURLForDataPath('title3.html')
    title2 = 'Title Of More Awesomeness'
    self.AppendTab(pyauto.GURL(url2))
    self._CheckHistory(title2, url2, 2)

  def testOpenWindowAddPage(self):
    """Verify that opening new window to a page adds the page to history."""
    self._NavigateAndCheckHistory('Title Of Awesomeness', 'title2.html', 1)

    url2 = self.GetFileURLForDataPath('title3.html')
    title2 = 'Title Of More Awesomeness'
    self.OpenNewBrowserWindow(True)
    self.NavigateToURL(url2, 1)
    self._CheckHistory(title2, url2, 2)

  def testSubmitFormAddsTargetPage(self):
    """Verify that submitting form adds target page to history list."""
    url1 = self.GetFileURLForDataPath('History', 'form.html')
    self.NavigateToURL(url1)
    self.assertTrue(self.SubmitForm('form'))
    url2 = self.GetFileURLForDataPath('History', 'target.html')
    self.assertEqual(
        'SUCCESS',
        self.GetDOMValue('document.getElementById("result").innerHTML'))
    self._CheckHistory('Target Page', url2, 2)

  def testOneHistoryTabPerWindow(self):
    """Verify history shortcut opens only one history tab per window.

    Also, make sure that existing history tab is activated.
    """
    # Invoke History.
    self.RunCommand(pyauto.IDC_SHOW_HISTORY)
    self.assertEqual('History', self.GetActiveTabTitle(),
                     msg='History page was not opened.')

    # Open new tab, invoke History again.
    self.RunCommand(pyauto.IDC_NEW_TAB)
    self.RunCommand(pyauto.IDC_SHOW_HISTORY)

    # Verify there is only one history tab, and that it is activated.
    tab0url = self.GetBrowserInfo()['windows'][0]['tabs'][0]['url']
    self.assertEqual(
        'chrome://history/', tab0url, msg='Tab 0: expected = %s, actual = %s.'
            % ('chrome://history/',  tab0url))

    tab1url = self.GetBrowserInfo()['windows'][0]['tabs'][1]['url']
    self.assertNotEqual(
        'chrome://history/', tab1url,
        msg='Tab 1: History page not expected.')

    self.assertEqual('History', self.GetActiveTabTitle(),
                     msg='History page is not activated.')


if __name__ == '__main__':
  pyauto_functional.Main()
