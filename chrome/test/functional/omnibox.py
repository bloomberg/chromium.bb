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
    self.assertTrue(self.GetOmniboxInfo().Properties('has_focus'))

  def _GetOmniboxMatchesFor(self, text, windex=0, attr_dict=None):
    """Fetch omnibox matches for the given query with the given properties.

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
    self.assertTrue(matches)
    return matches

  def testHistoryResult(self):
    """Verify that omnibox can fetch items from history."""
    url = self.GetFileURLForPath(os.path.join(self.DataDir(), 'title2.html'))
    title = 'Title Of Awesomeness'
    self.AppendTab(pyauto.GURL(url))
    def _VerifyResult(query_text, description):
      """Verify result matching given description for given query_text."""
      matches = self._GetOmniboxMatchesFor(
          query_text, attr_dict={'description': description})
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
    matches = self._GetOmniboxMatchesFor('file://')
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
    self.assertEqual(1, len(matches_description))
    item = matches_description[0]
    self.assertTrue(re.search(url_re, item['destination_url']))
    self.assertEqual('search-what-you-typed', item['type'])

  def testInlinAutoComplete(self):
    """Verify inline autocomplete for a pre-visited url."""
    self.NavigateToURL('http://www.google.com')
    matches = self._GetOmniboxMatchesFor('goog')
    # Omnibox should suggest auto completed url as the first item
    matches_description = matches[0]
    self.assertTrue('www.google.com' in matches_description['contents'])
    self.assertEqual('history-url', matches_description['type'])
    # The url should be inline-autocompleted in the omnibox
    self.assertEqual('google.com/', self.GetOmniboxInfo().Text())

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
                                 'destination_url': file_url,
                                 'description': title})
        self.assertEqual(1, len(matches))
    finally:
      shutil.rmtree(unicode(temp_dir))  # unicode so that win treats nicely.


if __name__ == '__main__':
  pyauto_functional.Main()

