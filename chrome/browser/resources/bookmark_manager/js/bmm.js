// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('bmm', function() {
  const TreeIterator = bmm.TreeIterator;
  const Promise = cr.Promise;

  /**
   * Whether a node contains another node.
   * @param {!BookmarkTreeNode} parent
   * @param {!BookmarkTreeNode} descendant
   * @return {boolean} Whether the parent contains the descendant.
   */
  function contains(parent, descendant) {
    if (descendant.parentId == parent.id)
      return true;
    // the bmm.treeLookup contains all folders
    var parentTreeItem = bmm.treeLookup[descendant.parentId];
    if (!parentTreeItem || !parentTreeItem.bookmarkNode)
      return false;
    return this.contains(parent, parentTreeItem.bookmarkNode);
  }

  /**
   * @param {!BookmarkTreeNode} node The node to test.
   * @return {boolean} Whether a bookmark node is a folder.
   */
  function isFolder(node) {
    return !('url' in node);
  }

  var loadingPromise;

  /**
   * Loads the entire bookmark tree and returns a {@code cr.Promise} that will
   * be fulfilled when done. This reuses multiple loads so that we never load
   * more than one tree at the same time.
   * @return {!cr.Promise} The future promise for the load.
   */
  function loadTree() {
    var p = new Promise;
    if (!loadingPromise) {
      loadingPromise = new Promise;
      chrome.bookmarks.getTree(function(nodes) {
        loadingPromise.value = nodes[0];
        loadingPromise = null;
      });
    }
    loadingPromise.addListener(function(n) {
      p.value = n;
    });
    return p;
  }

  /**
   * Helper function for {@code loadSubtree}. This does an in order search of
   * the tree.
   * @param {!BookmarkTreeNode} node The node to start searching at.
   * @param {string} id The ID of the node to find.
   * @return {BookmarkTreeNode} The found node or null if not found.
   */
  function findNode(node, id) {
    var it = new TreeIterator(node);
    var n;
    while (it.moveNext()) {
      n = it.current;
      if (n.id == id)
        return n;
    }
    return null;
  }

  /**
   * Loads a subtree of the bookmark tree and returns a {@code cr.Promise} that
   * will be fulfilled when done. This reuses multiple loads so that we never
   * load more than one tree at the same time. (This actually loads the entire
   * tree but it will only return the relevant subtree in the value of the
   * future promise.)
   * @return {!cr.Promise} The future promise for the load.
   */
  function loadSubtree(id) {
    var p = new Promise;
    var lp = loadTree();
    lp.addListener(function(tree) {
      var node = findNode(tree, id);
      p.value = node || Error('Failed to load subtree ' + id);
    });
    return p;
  }

  return {
    contains: contains,
    isFolder: isFolder,
    loadSubtree: loadSubtree,
    loadTree: loadTree
  };
});
