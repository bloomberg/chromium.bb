#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Must import this first
import pyauto_functional

import os
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


if __name__ == '__main__':
  pyauto_functional.Main()
