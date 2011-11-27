# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""BookmarkModel: python representation of the bookmark model.

Obtain one of these from PyUITestSuite::GetBookmarkModel() call.
"""

import os
import simplejson as json
import sys

class BookmarkModel(object):

  def __init__(self, json_string):
    """Initialize a BookmarkModel from a string of json.

    The JSON representation is the same as used by the bookmark model
    to save to disk.

    Args:
      json_string: a string of JSON.
    """
    self.bookdict = json.loads(json_string)

  def BookmarkBar(self):
    """Return the bookmark bar node as a dict."""
    return self.bookdict['roots']['bookmark_bar']

  def Other(self):
    """Return the 'other' node (e.g. parent of "Other Bookmarks")"""
    return self.bookdict['roots']['other']

  def NodeCount(self, node=None):
    """Return a count of bookmark nodes, including folders.

    The root node itself is included in the count.

    Args:
      node: the root to start with.  If not specified, count all."""
    if node == None:
      return reduce(lambda x, y: x + y,
                    [self.NodeCount(x)
                     for x in self.bookdict['roots'].values()])
    total = 1
    children = node.get('children', None)
    if children:
      total = total + reduce(lambda x,y: x + y,
                             [self.NodeCount(x) for x in children])
    return total

  def FindByID(self, id, nodes=None):
    """Find the bookmark by id.  Return the dict or None.

    Args:
      id: the id to look for.
      nodes: an iterable of nodes to start with.  If not specified, search all.
        'Not specified' means None, not [].
    """
    # Careful; we may get an empty list which is different than not
    # having specified a list.
    if nodes == None:
      nodes = self.bookdict['roots'].values()
    # Check each item.  If it matches, return.  If not, check each of
    # their kids.
    for node in nodes:
      if node['id'] == id:
        return node
      for child in node.get('children', []):
        found_node = self.FindByID(id, [child])
        if found_node:
          return found_node
    # Not found at all.
    return None

  def FindByTitle(self, title, nodes=None):
    """Return a tuple of all nodes which have |title| in their title.

    Args:
      title: the title to look for.
      node: an iterable of nodes to start with.  If not specified, search all.
        'Not specified' means None, not [].
    """
    # Careful; we may get an empty list which is different than not
    # having specified a list.
    if nodes == None:
      nodes = self.bookdict['roots'].values()
    # Check each item.  If it matches, return.  If not, check each of
    # their kids.
    results = []
    for node in nodes:
      node_title = node.get('title', None) or node.get('name', None)
      if title == node_title:
        results.append(node)
      # Note we check everything; unlike the FindByID, we do not stop early.
      for child in node.get('children', []):
        results += self.FindByTitle(title, [child])
    return results
