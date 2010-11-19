#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

import pyauto_functional  # Must be imported before pyauto
import pyauto


class NTPTest(pyauto.PyUITest):
  """Test of the NTP."""

  def Debug(self):
    """Test method for experimentation.

    This method is not run automatically.
    """
    while True:
      raw_input('Interact with the browser and hit <enter> to dump NTP info...')
      print '*' * 20
      import pprint
      pp = pprint.PrettyPrinter(indent=2)
      pp.pprint(self._GetNTPInfo())

  def __init__(self, methodName='runTest'):
    super(NTPTest, self).__init__(methodName)

    # Create some dummy file urls we can use in the tests.
    filenames = ['title1.html', 'title2.html']
    titles = [u'', u'Title Of Awesomeness']
    urls = map(lambda name: self.GetFileURLForDataPath(name), filenames)
    self.PAGES = map(lambda url, title: {'url': url, 'title': title},
                     urls, titles)

  def testFreshProfile(self):
    """Tests that the NTP with a fresh profile is correct"""
    thumbnails = self.GetNTPThumbnails()
    default_sites = self.GetNTPDefaultSites()
    self.assertEqual(len(default_sites), len(thumbnails))
    for thumbnail, default_site in zip(thumbnails, default_sites):
      self.assertEqual(thumbnail['url'], default_site)
    self.assertEqual(0, len(self.GetNTPRecentlyClosed()))

  def testRemoveDefaultThumbnails(self):
    """Tests that the default thumbnails can be removed"""
    self.RemoveNTPDefaultThumbnails()
    self.assertFalse(self.GetNTPThumbnails())
    self.RestoreAllNTPThumbnails()
    self.assertEqual(len(self.GetNTPDefaultSites()),
                     len(self.GetNTPThumbnails()))
    self.RemoveNTPDefaultThumbnails()
    self.assertFalse(self.GetNTPThumbnails())

  def testOneMostVisitedSite(self):
    """Tests that a site is added to the most visited sites"""
    self.RemoveNTPDefaultThumbnails()
    self.NavigateToURL(self.PAGES[1]['url'])
    self.assertEqual(self.PAGES[1]['url'], self.GetNTPThumbnails()[0]['url'])
    self.assertEqual(self.PAGES[1]['title'],
                     self.GetNTPThumbnails()[0]['title'])

  def testOneRecentlyClosedTab(self):
    """Tests that closing a tab populates the recently closed tabs list"""
    self.RemoveNTPDefaultThumbnails()
    self.AppendTab(pyauto.GURL(self.PAGES[1]['url']))
    self.GetBrowserWindow(0).GetTab(1).Close(True)
    self.assertEqual(self.PAGES[1]['url'],
                     self.GetNTPRecentlyClosed()[0]['url'])
    self.assertEqual(self.PAGES[1]['title'],
                     self.GetNTPRecentlyClosed()[0]['title'])

  def testMoveThumbnailBasic(self):
    """Tests moving a thumbnail to a different index"""
    self.RemoveNTPDefaultThumbnails()
    self.NavigateToURL(self.PAGES[0]['url'])
    self.NavigateToURL(self.PAGES[1]['url'])
    thumbnails = self.GetNTPThumbnails()
    self.MoveNTPThumbnail(thumbnails[0], 1)
    self.assertTrue(self.IsNTPThumbnailPinned(thumbnails[0]))
    self.assertFalse(self.IsNTPThumbnailPinned(thumbnails[1]))
    self.assertEqual(self.PAGES[0]['url'], self.GetNTPThumbnails()[1]['url'])
    self.assertEqual(1, self.GetNTPThumbnailIndex(thumbnails[0]))

  def testPinningThumbnailBasic(self):
    """Tests that we can pin/unpin a thumbnail"""
    self.RemoveNTPDefaultThumbnails()
    self.NavigateToURL(self.PAGES[0]['url'])
    thumbnail1 = self.GetNTPThumbnails()[0]
    self.assertFalse(self.IsNTPThumbnailPinned(thumbnail1))
    self.PinNTPThumbnail(thumbnail1)
    self.assertTrue(self.IsNTPThumbnailPinned(thumbnail1))
    self.UnpinNTPThumbnail(thumbnail1)
    self.assertFalse(self.IsNTPThumbnailPinned(thumbnail1))


if __name__ == '__main__':
  pyauto_functional.Main()
