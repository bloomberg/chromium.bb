#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Must import this first
import pyauto_functional

import logging
import os
import sys
import unittest

import pyauto


class BookmarksTest(pyauto.PyUITest):
  """Test of bookmarks."""

  def testBasics(self):
    """Basic tests with an empty bookmark model."""
    bookmarks = self.GetBookmarkModel()
    # Make sure we have the two root nodes and that they are empty
    for node in (bookmarks.BookmarkBar(), bookmarks.Other()):
      self.assertEqual(node['type'], 'folder')
      self.assertFalse(node['children'])

  def testAddOneNode(self):
    """Add a bookmark to the bar; confirm it."""
    bookmarks = self.GetBookmarkModel()
    bar_id = bookmarks.BookmarkBar()['id']
    name = 'Google'
    url = 'http://www.google.com'
    c = bookmarks.NodeCount()
    self.AddBookmarkURL(bar_id, 0, name, url)
    bookmarks = self.GetBookmarkModel()
    node = bookmarks.BookmarkBar()['children'][0]
    self.assertEqual(c+1, bookmarks.NodeCount())
    self.assertEqual(node['type'], 'url')
    self.assertEqual(node['name'], name)
    # URLs may not be exact; e.g. http://www.com --> http://www.com/
    self.assertTrue(url in node['url'])
    # Make sure we can search and find the same thing
    nodes = bookmarks.FindByTitle(name)
    self.assertEqual(1, len(nodes))
    self.assertTrue(nodes[0]['id'] == node['id'])

  def testAddGroup(self):
    """Add a group to the bar; confirm it."""
    bookmarks = self.GetBookmarkModel()
    c = bookmarks.NodeCount()
    parent_id = bookmarks.Other()['id']
    name = 'SuperGroup'
    self.AddBookmarkGroup(parent_id, 0, name)
    # Confirm group.
    bookmarks = self.GetBookmarkModel()
    node = bookmarks.Other()['children'][0]
    self.assertEqual(c+1, bookmarks.NodeCount())
    self.assertEqual(node['type'], 'folder')
    self.assertEqual(node['name'], name)
    # Make sure we can search and find the same thing
    findnode = bookmarks.FindByID(node['id'])
    self.assertEqual(node, findnode)

  def testAddChangeRemove(self):
    """Add some bookmarks.  Change their title and URL.  Remove one."""
    # Add some.
    bookmarks = self.GetBookmarkModel()
    bar_id = bookmarks.BookmarkBar()['id']
    self.AddBookmarkURL(bar_id, 0, 'Title1',
                        'http://www.google.com')
    self.AddBookmarkURL(bar_id, 1, 'Title1',
                        'http://www.google.com/reader')
    # Change a title and URL.
    bookmarks = self.GetBookmarkModel()
    nodes = bookmarks.FindByTitle('Title1')
    self.assertEqual(2, len(nodes))
    self.SetBookmarkTitle(nodes[0]['id'], 'Title2')
    self.SetBookmarkURL(nodes[1]['id'], 'http://www.youtube.com')
    # Confirm, then remove.
    bookmarks = self.GetBookmarkModel()
    nodes = bookmarks.FindByTitle('Title1')
    self.assertEqual(1, len(nodes))
    self.assertTrue('http://www.youtube.com' in nodes[0]['url'])
    nodes = bookmarks.FindByTitle('Title2')
    self.assertEqual(1, len(nodes))
    self.assertTrue('google.com' in nodes[0]['url'])
    self.RemoveBookmark(nodes[0]['id'])
    # Confirm removal.
    bookmarks = self.GetBookmarkModel()
    nodes = bookmarks.FindByTitle('Title2')
    self.assertEqual(0, len(nodes))

  def testReparent(self):
    bookmarks = self.GetBookmarkModel()
    bar_id = bookmarks.BookmarkBar()['id']
    # Add some groups
    for i in range(3):
      self.AddBookmarkGroup(bar_id, i, 'Group' + str(i))
    # Add a bookmark in one group
    bookmarks = self.GetBookmarkModel()
    nodes = bookmarks.FindByTitle('Group0')
    self.AddBookmarkURL(nodes[0]['id'], 0,
                        'marked', 'http://www.youtube.com')
    # Make sure it's not in a different group
    bookmarks = self.GetBookmarkModel()
    nodes = bookmarks.FindByTitle('Group2')
    self.assertFalse(nodes[0]['children'])
    # Move it to that group
    self.ReparentBookmark(bookmarks.FindByTitle('marked')[0]['id'],
                          bookmarks.FindByTitle('Group2')[0]['id'], 0)
    # Confirm.
    bookmarks = self.GetBookmarkModel()
    nodes = bookmarks.FindByTitle('Group0')
    self.assertEqual([], nodes[0]['children'])
    nodes = bookmarks.FindByTitle('Group2')
    self.assertEqual(1, len(nodes[0]['children']))

  def _GetTestURLs(self, filename):
    """Read the given data file and return a dictionary of items."""
    data_file = os.path.join(self.DataDir(), "bookmarks", filename)
    contents = open(data_file).read()
    try:
      dictionary = eval(contents, {'__builtins__': None}, None)
    except:
      print >>sys.stderr, "%s is an invalid data file." % data_file
      raise
    return dictionary

  def testUnicodeStrings(self):
    """Test bookmarks with unicode strings."""
    bookmarks = self.GetBookmarkModel()
    bar_id = bookmarks.BookmarkBar()['id']
    orig_nodes_count = bookmarks.NodeCount()

    sites = self._GetTestURLs("urls_and_titles")
    # Add bookmarks
    for index, (url, name) in enumerate(sites.iteritems()):
      self.AddBookmarkURL(bar_id, index, name, url)

    # Fetch the added bookmarks and verify them.
    bookmarks = self.GetBookmarkModel()
    self.assertEqual(orig_nodes_count + len(sites), bookmarks.NodeCount())

    for node, k in zip(bookmarks.BookmarkBar()['children'], sites.keys()):
      self.assertEqual(node['type'], 'url')
      self.assertEqual(node['name'], sites[k])
      self.assertEqual(node['url'], k)

  def testSizes(self):
    """Verify bookmarks with short and long names."""
    bookmarks = self.GetBookmarkModel()
    bar_id = bookmarks.BookmarkBar()['id']
    orig_nodes_count = bookmarks.NodeCount()

    sites = self._GetTestURLs("long-and-short-names")
    # Add bookmarks
    for index, (url, name) in enumerate(sites.iteritems()):
      self.AddBookmarkURL(bar_id, index, name, url)

    # Fetch the added urls and verify them.
    bookmarks = self.GetBookmarkModel()
    self.assertEqual(orig_nodes_count + len(sites), bookmarks.NodeCount())

    for node, k in  zip(bookmarks.BookmarkBar()['children'], sites.keys()):
      self.assertEqual(node['type'], 'url')
      self.assertEqual(node['name'], sites[k])
      self.assertEqual(node['url'], k)

  def testAddingBookmarksToBarAndOther(self):
    """Add bookmarks to the Bookmark Bar and "Other Bookmarks" group."""
    url_data = self._GetTestURLs("urls_and_titles")
    list_of_urls = url_data.keys()
    list_of_titles = url_data.values()

    # We need at least 3 URLs for this test to run
    self.assertTrue(len(list_of_urls) > 2)

    bookmarks = self.GetBookmarkModel()
    nodes = bookmarks.NodeCount()

    # Add bookmarks to the bar and other
    bar_id = bookmarks.BookmarkBar()['id']
    self.AddBookmarkURL(bar_id, 0, list_of_titles[0], list_of_urls[0])

    other_id = bookmarks.Other()['id']
    self.AddBookmarkURL(other_id, 0, list_of_titles[1], list_of_urls[1])

    # Now check that we added them
    bookmarks = self.GetBookmarkModel()
    self.assertEqual(nodes + 2, bookmarks.NodeCount())

    bar_child = bookmarks.BookmarkBar()['children'][0]
    self.assertEqual(bar_child['type'], 'url')
    self.assertEqual(bar_child['name'], list_of_titles[0])
    self.assertEqual(bar_child['url'], list_of_urls[0])

    other_child = bookmarks.Other()['children'][0]
    self.assertEqual(other_child['type'], 'url')
    self.assertEqual(other_child['name'], list_of_titles[1])
    self.assertEqual(other_child['url'], list_of_urls[1])

  def testAddingFoldersToBarAndOther(self):
    """Add folders to the Bookmark Bar and "Other Bookmarks" group."""
    url_data = self._GetTestURLs("urls_and_titles")
    list_of_titles = url_data.values()

    bookmarks = self.GetBookmarkModel()
    nodes = bookmarks.NodeCount()

    # Add folders to the bar and other
    bar_id = bookmarks.BookmarkBar()['id']
    self.AddBookmarkGroup(bar_id, 0, list_of_titles[0])

    other_id = bookmarks.Other()['id']
    self.AddBookmarkGroup(other_id, 0, list_of_titles[1])

    # Now check that we added them
    bookmarks = self.GetBookmarkModel()
    self.assertEqual(nodes + 2, bookmarks.NodeCount())

    bar_child = bookmarks.BookmarkBar()['children'][0]
    self.assertEqual(bar_child['type'], 'folder')
    self.assertEqual(bar_child['name'], list_of_titles[0])

    other_child = bookmarks.Other()['children'][0]
    self.assertEqual(other_child['type'], 'folder')
    self.assertEqual(other_child['name'], list_of_titles[1])

  def testAddingFoldersWithChildrenToBarAndOther(self):
    """Add folders with children to the bar and "Other Bookmarks" group."""
    url_data = self._GetTestURLs("urls_and_titles")
    list_of_urls = url_data.keys()
    list_of_titles = url_data.values()

    bookmarks = self.GetBookmarkModel()
    nodes = bookmarks.NodeCount()

    # Add a folder to the bar
    bar_id = bookmarks.BookmarkBar()['id']
    self.AddBookmarkGroup(bar_id, 0, list_of_titles[0])

    # Get a handle on the folder, and add a bookmark in it
    bookmarks = self.GetBookmarkModel()
    bar_folder_id = bookmarks.BookmarkBar()['children'][0]['id']
    self.AddBookmarkURL(bar_folder_id, 0, list_of_titles[1], list_of_urls[1])

    # Add a folder to other
    other_id = bookmarks.Other()['id']
    self.AddBookmarkGroup(other_id, 0, list_of_titles[2])

    # Get a handle on the folder, and add a bookmark in it
    bookmarks = self.GetBookmarkModel()
    other_folder_id = bookmarks.Other()['children'][0]['id']
    self.AddBookmarkURL(other_folder_id, 0, list_of_titles[3], list_of_urls[3])

    # Verify we have added a folder in the bar and other, and each folder has
    # a URL in it
    bookmarks = self.GetBookmarkModel()
    self.assertEqual(nodes + 4, bookmarks.NodeCount())

    bar_child = bookmarks.BookmarkBar()['children'][0]
    self.assertEqual(bar_child['type'], 'folder')
    self.assertEqual(bar_child['name'], list_of_titles[0])

    bar_child_0 = bar_child['children'][0]
    self.assertEqual(bar_child_0['type'], 'url')
    self.assertEqual(bar_child_0['name'], list_of_titles[1])
    self.assertEqual(bar_child_0['url'], list_of_urls[1])

    other_child = bookmarks.Other()['children'][0]
    self.assertEqual(other_child['type'], 'folder')
    self.assertEqual(other_child['name'], list_of_titles[2])

    other_child_0 = other_child['children'][0]
    self.assertEqual(other_child_0['type'], 'url')
    self.assertEqual(other_child_0['name'], list_of_titles[3])
    self.assertEqual(other_child_0['url'], list_of_urls[3])

  def testOrdering(self):
    """Add/delete a set of bookmarks, and verify their ordering."""
    bookmarks = self.GetBookmarkModel()
    bar_id = bookmarks.BookmarkBar()['id']
    orig_nodes_count = bookmarks.NodeCount()

    url_data = self._GetTestURLs("urls_and_titles")
    list_of_titles = url_data.values()

    for index, (url, name) in enumerate(url_data.iteritems()):
      self.AddBookmarkURL(bar_id, index, name, url)

    # check that we added them correctly
    bookmarks = self.GetBookmarkModel()
    self.assertEqual(orig_nodes_count + len(url_data) , bookmarks.NodeCount())

    for node, (url, name) in zip(bookmarks.BookmarkBar()['children'],
                                 url_data.iteritems()):
      self.assertEqual(node['type'], 'url')
      self.assertEqual(node['name'], name)
      self.assertEqual(node['url'], url)

    # remove a few of them.
    remove_indices = [3, 5, 9, 12]
    new_list_of_titles = list_of_titles[:]
    assert len(remove_indices) <= len(url_data) and \
           max(remove_indices) < len(url_data)

    for index in remove_indices:
      node = bookmarks.FindByTitle(list_of_titles[index])
      self.RemoveBookmark(node[0]['id'])
      new_list_of_titles.remove(list_of_titles[index])
    logging.debug('Removed: %s' % [list_of_titles[x] for x in remove_indices])

    # Confirm removal.
    bookmarks = self.GetBookmarkModel()
    for index in remove_indices:
      nodes = bookmarks.FindByTitle(list_of_titles[index])
      self.assertEqual(0, len(nodes))

    # Confirm that other bookmarks were not removed and their order is intact.
    self.assertEqual(len(new_list_of_titles),
                     len(bookmarks.BookmarkBar()['children']))
    for title, node in zip(new_list_of_titles,
                           bookmarks.BookmarkBar()['children']):
      self.assertEqual(title, node['name'])

  def testDeepNesting(self):
    """Deep nested folders. Move a bookmark around."""
    url_data = self._GetTestURLs("urls_and_titles")
    list_of_urls = url_data.keys()
    list_of_titles = url_data.values()

    bookmarks = self.GetBookmarkModel()
    nodes = bookmarks.NodeCount()

    bar_folder_id = bookmarks.BookmarkBar()['id']

    # Created deep nested folders
    num_folders = 15
    assert num_folders >= 3
    for i in range(num_folders):
      self.AddBookmarkGroup(bar_folder_id, 0 ,'Group %d' % i)
      bookmarks = self.GetBookmarkModel()
      added_nodes = bookmarks.FindByID(bar_folder_id)['children']
      self.assertEqual(1, len(added_nodes))
      self.assertEqual('Group %d' % i, added_nodes[0]['name'])
      bar_folder_id = added_nodes[0]['id']

    self.assertEqual(nodes + num_folders, bookmarks.NodeCount())

    # Add a bookmark to the leaf folder
    a_url = list_of_urls[0]
    a_title = list_of_titles[0]
    leaf_folder = bookmarks.FindByTitle('Group %d' % (num_folders - 1))[0]
    self.AddBookmarkURL(leaf_folder['id'], 0, a_title, a_url)

    bookmarks = self.GetBookmarkModel()
    nodes = bookmarks.FindByTitle('Group %d' % (num_folders - 1))
    self.assertEqual(1, len(nodes))
    mynode = nodes[0]['children'][0]
    self.assertEqual(a_title, mynode['name'])
    self.assertEqual(a_url, mynode['url'])

    # Move bookmark to another group, say Group 2
    self.ReparentBookmark(bookmarks.FindByTitle(a_title)[0]['id'],
                          bookmarks.FindByTitle('Group 2')[0]['id'], 0)

    bookmarks = self.GetBookmarkModel()
    # Bookmark moves to right place
    node = bookmarks.FindByTitle('Group 2')[0]
    self.assertEqual(2, len(node['children']))
    self.assertEqual(a_title, node['children'][0]['name'])
    self.assertEqual(a_url, node['children'][0]['url'])

    # Bookmark removed from leaf folder
    nodes = bookmarks.FindByTitle('Group %d' % (num_folders - 1))
    self.assertEqual([], nodes[0]['children'])

  def testURLTypes(self):
    """Test bookmarks with different types of URLS."""
    bookmarks = self.GetBookmarkModel()
    bar_id = bookmarks.BookmarkBar()['id']
    orig_nodes_count = bookmarks.NodeCount()
    url_data = self._GetTestURLs("url_types")
    for index, (url, name) in enumerate(url_data.iteritems()):
      self.AddBookmarkURL(bar_id, index, name, url)
    # check that we added them correctly
    bookmarks = self.GetBookmarkModel()
    self.assertEqual(orig_nodes_count + len(url_data), bookmarks.NodeCount())
    for node, (url, name) in zip(bookmarks.BookmarkBar()['children'],
                                 url_data.iteritems()):
      self.assertEqual(node['type'], 'url')
      self.assertEqual(node['name'], name)
      self.assertEqual(node['url'], url)

  def _VerifyBookmarkURL(self, node, name, url):
    """Verify that node is a bookmark URL of the given name and url."""
    self.assertTrue(node)
    self.assertEqual(node['type'], 'url')
    self.assertEqual(node['name'], name)
    self.assertEqual(node['url'], url)

  def testDuplicateBookmarks(self):
    """Verify bookmark duplicates."""
    url_data = self._GetTestURLs("urls_and_titles")
    list_of_urls = url_data.keys()
    list_of_titles = url_data.values()
    bookmarks = self.GetBookmarkModel()
    nodes = bookmarks.NodeCount()
    bar_id = bookmarks.BookmarkBar()['id']
    # Create some duplicate bookmarks
    self.AddBookmarkURL(bar_id, 0, list_of_titles[0], list_of_urls[0])
    self.AddBookmarkURL(bar_id, 1, list_of_titles[0], list_of_urls[0])
    self.AddBookmarkURL(bar_id, 2, list_of_titles[0], list_of_urls[1])
    self.AddBookmarkURL(bar_id, 3, list_of_titles[1], list_of_urls[0])
    # Create a folder with any existing bookmark title
    self.AddBookmarkGroup(bar_id, 4, list_of_titles[0])
    bookmarks = self.GetBookmarkModel()
    self.assertEqual(nodes + 5, bookmarks.NodeCount())
    # Verify 1st bookmark is created
    bar_child = bookmarks.BookmarkBar()['children'][0]
    self._VerifyBookmarkURL(bar_child, list_of_titles[0], list_of_urls[0])
   # Verify this is a duplicate bookmark of 1st bookmark
    bar_child = bookmarks.BookmarkBar()['children'][1]
    self._VerifyBookmarkURL(bar_child, list_of_titles[0], list_of_urls[0])
   # Verify the bookmark with same title and different URL of 1st bookmark
    bar_child = bookmarks.BookmarkBar()['children'][2]
    self._VerifyBookmarkURL(bar_child, list_of_titles[0], list_of_urls[1])
   # Verify the bookmark with different title and same URL of 1st bookmark
    bar_child = bookmarks.BookmarkBar()['children'][3]
    self._VerifyBookmarkURL(bar_child, list_of_titles[1], list_of_urls[0])
   # Verify Bookmark group got created with same title of 1st bookmark
    bar_child = bookmarks.BookmarkBar()['children'][4]
    self.assertEqual(bar_child['type'], 'folder')
    self.assertEqual(bar_child['name'], list_of_titles[0])

  def testBookmarksPersistence(self):
    """Verify that bookmarks and groups persist browser restart."""
    # Populate bookmarks and groups
    bookmarks = self.GetBookmarkModel()
    bar_id = bookmarks.BookmarkBar()['id']
    other_id = bookmarks.Other()['id']
    self.AddBookmarkURL(bar_id, 0, "Google", "http://www.news.google.com/")
    self.AddBookmarkURL(other_id, 0, "Yahoo", "http://www.yahoo.com/")
    self.AddBookmarkGroup(bar_id, 0, "MS")
    bookmarks = self.GetBookmarkModel()
    bar_folder_id = bookmarks.BookmarkBar()['children'][0]['id']
    self.AddBookmarkURL(bar_folder_id, 0, "BING", "http://www.bing.com/")
    self.AddBookmarkGroup(other_id, 0, "Oracle")
    bookmarks = self.GetBookmarkModel()
    other_folder_id = bookmarks.Other()['id']
    self.AddBookmarkURL(other_folder_id, 0, "DB", "http://www.oracle.com/")

    nodes_before = self.GetBookmarkModel().NodeCount()
    self.RestartBrowser(clear_profile=False)
    # Verify that all bookmarks persist
    bookmarks = self.GetBookmarkModel()
    node = bookmarks.FindByTitle('Google')
    self.assertEqual(nodes_before, bookmarks.NodeCount())
    self._VerifyBookmarkURL(node[0], 'Google', 'http://www.news.google.com/')
    self._VerifyBookmarkURL(bookmarks.FindByTitle('Yahoo')[0],
                            'Yahoo', 'http://www.yahoo.com/')
    bmb_child = bookmarks.BookmarkBar()['children'][0]
    self.assertEqual(bmb_child['type'], 'folder')
    self.assertEqual(bmb_child['name'],'MS')
    self._VerifyBookmarkURL(bmb_child['children'][0],
                            'BING', 'http://www.bing.com/')

  def testBookmarksManager(self):
    """Verify bookmark title is displayed in bookmarks manager"""
    bookmarks = self.GetBookmarkModel()
    bar_id = bookmarks.BookmarkBar()['id']
    self.AddBookmarkURL(bar_id, 0, 'GoogleNews', 'http://www.news.google.com/')
    self.NavigateToURL('chrome://bookmarks/')
    self.assertTrue(self.WaitUntil(
        lambda: self.FindInPage('GoogleNews', tab_index=0)['match_count'],
                expect_retval=1))


if __name__ == '__main__':
  pyauto_functional.Main()
