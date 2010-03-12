#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Must import this first
import pyauto_test_utils

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


if __name__ == '__main__':
  unittest.main()
